/*
 *  render related stuff for V9938
 *
 *  Based on the videx code in AppleIIVGA
 *  work in the rendering pipeline
 *
 *  Based heavily on the MAME v9938.cpp code
 *  // license:BSD-3-Clause
// copyright-holders:Aaron Giles, Nathan Woods

/***************************************************************************


 */
#include "v9938.h"

#include <pico/stdlib.h>
#include "buffers.h"
#include "colors.h"
#include "vga.h"

#define V9938_MODE_TEXT1 0x02
#define V9938_MODE_MULTI 0x01
#define V9938_MODE_GRAPHIC1 0x00
#define V9938_MODE_GRAPHIC2 0x04
#define V9938_MODE_GRAPHIC3 0x08
#define V9938_MODE_GRAPHIC4 0x0c
#define V9938_MODE_GRAPHIC5 0x10
#define V9938_MODE_GRAPHIC6 0x14
#define V9938_MODE_GRAPHIC7 0x1c
#define V9938_MODE_TEXT2 0x0a

#define MODEL_V9938 0
#define MODEL_V9958 1


volatile uint8_t   m_Regs[8];
#define VRAM_SIZE 0x20000
volatile uint8_t vram[VRAM_SIZE];
#define V9938_LONG_WIDTH (512 + 32)
// not supporting expmem, but leaving the define to make the code compile
#define EXPMEM_OFFSET 0x20000

        // Command unit
        struct {
                int SX,SY;
                int DX,DY;
                int TX,TY;
                int NX,NY;
                int MX;
                int ASX,ADX,ANX;
                uint8_t CL;
                uint8_t LO;
                uint8_t CM;
                uint8_t MXS, MXD;
        } m_mmc;
        int  m_vdp_ops_count;
void (*m_vdp_engine)();



// Make these global
uint sl_pos, sl_pos_line_start;
struct vga_scanline *sl;

int m_offset_x, m_offset_y, m_visible_y, m_mode;
// palette
int m_pal_write_first, m_cmd_write_first;
uint8_t m_pal_write, m_cmd_write;
uint8_t m_pal_reg[32], m_stat_reg[10], m_cont_reg[48], m_read_ahead;
uint8_t m_v9958_sp_mode;
uint8_t m_int_state;
int m_blink;


// memory
uint16_t m_address_latch;
int m_vram_size = VRAM_SIZE;
int m_model = MODEL_V9938;




uint8_t v9938_read(uint_fast16_t address)
{
        switch (address & 3)
        {
        case 0: return v9938_vram_r();
        case 1: return v9938_status_r();
        }
        return 0xff;

}

void v9938_write(uint_fast16_t address, uint8_t data)
{
        switch (address & 3)
        {
        case 0: v9938_vram_w(data);     break;
        case 1: v9938_command_w(data);  break;
        case 2: v9938_palette_w(data);  break;
        case 3: v9938_register_w(data); break;
        }
}


uint8_t v9938_vram_r()
{
        uint8_t ret;
        int address;

        address = ((int)m_cont_reg[14] << 14) | m_address_latch;

        m_cmd_write_first = 0;

        ret = m_read_ahead;

        m_read_ahead = v9938_vram_read(address);

        m_address_latch = (m_address_latch + 1) & 0x3fff;
        if ((!m_address_latch) && (m_cont_reg[0] & 0x0c) ) // correct ???
        {
                m_cont_reg[14] = (m_cont_reg[14] + 1) & 7;
        }

        return ret;
}


// In theory we are not implementing reads, but there may be some state chage stuff in here
uint8_t v9938_status_r()
{
        int reg;
        uint8_t ret;

        m_cmd_write_first = 0;

        reg = m_cont_reg[15] & 0x0f;
        if (reg > 9)
                return 0xff;

        switch (reg)
        {
        case 0:
                ret = m_stat_reg[0];
                m_stat_reg[0] &= 0x1f;
                break;
        case 1:
                ret = m_stat_reg[1];
                m_stat_reg[1] &= 0xfe;
                // mouse mode: add button state
                //if ((m_cont_reg[8] & 0xc0) == 0x80)
                //        ret |= m_button_state & 0xc0;
                break;
        case 2:
                /*update_command ();*/
                /*
                WTF is this? Whatever this was intended to do, it is nonsensical.
                Might as well pick a random number....
                This was an attempt to emulate H-Blank flag ;)
                n = cycles_currently_ran ();
                if ( (n < 28) || (n > 199) ) vdp.statReg[2] |= 0x20;
                else vdp.statReg[2] &= ~0x20;
                */
                //if (machine().rand() & 1) m_stat_reg[2] |= 0x20;
                //else m_stat_reg[2] &= ~0x20;
                m_stat_reg[2] &= ~0x20;
                ret = m_stat_reg[2];
                break;
        case 3:
                if ((m_cont_reg[8] & 0xc0) == 0x80)
                {   // mouse mode: return x mouse delta
                //        ret = m_mx_delta;
                //        m_mx_delta = 0;
                }
                else
                        ret = m_stat_reg[3];
                break;
        case 5:
                if ((m_cont_reg[8] & 0xc0) == 0x80)
                {   // mouse mode: return y mouse delta
               //         ret = m_my_delta;
               //         m_my_delta = 0;
                }
                else
                        ret = m_stat_reg[5];
                break;
        case 7:
                ret = m_stat_reg[7];
                m_stat_reg[7] = m_cont_reg[44] = v9938_vdp_to_cpu () ;
                break;
        default:
                ret = m_stat_reg[reg];
                break;
        }

        //check_int ();

        return ret;
}

void v9938_palette_w(uint8_t data)
{
        int indexp;

        if (m_pal_write_first)
        {
                // store in register
                indexp = m_cont_reg[0x10] & 15;
                m_pal_reg[indexp*2] = m_pal_write & 0x77;
                m_pal_reg[indexp*2+1] = data & 0x07;

                // update palette
                set_pen16_rgb(indexp, (m_pal_write & 0x70) >> 4, (data & 0x07), (m_pal_write & 0x07));

                m_cont_reg[0x10] = (m_cont_reg[0x10] + 1) & 15;
                m_pal_write_first = 0;
        }
        else
        {
                m_pal_write = data;
                m_pal_write_first = 1;
        }
}


void v9938_vram_w(uint8_t data)
{
        int address;

        /*update_command ();*/

        m_cmd_write_first = 0;

        address = ((int)m_cont_reg[14] << 14) | m_address_latch;
        if (!(m_cont_reg[45] & 0x40))
        {
                v9938_vram_write(address, data);
        }



        m_address_latch = (m_address_latch + 1) & 0x3fff;
        if ((!m_address_latch) && (m_cont_reg[0] & 0x0c) ) // correct ???
        {
                m_cont_reg[14] = (m_cont_reg[14] + 1) & 7;
        }
}



void v9938_command_w(uint8_t data)
{
        if (m_cmd_write_first)
        {
                if (data & 0x80)
                {
                        if (!(data & 0x40))
                                v9938_register_write (data & 0x3f, m_cmd_write);
                }
                else
                {
                        m_address_latch =
                        (((uint16_t)data << 8) | m_cmd_write) & 0x3fff;
                        if ( !(data & 0x40) ) v9938_vram_r (); // read ahead!
                }

                m_cmd_write_first = 0;
        }
        else
        {
                m_cmd_write = data;
                m_cmd_write_first = 1;
        }
}

void v9938_register_w(uint8_t data)
{
        int reg;

        reg = m_cont_reg[17] & 0x3f;
        if (reg != 17)
                v9938_register_write(reg, data); // true ?

        if (!(m_cont_reg[17] & 0x80))
                m_cont_reg[17] = (m_cont_reg[17] + 1) & 0x3f;
}


void v9938_reset()
{
        int i;

        // offset reset
        m_offset_x = 8;
        m_offset_y = 0;
        m_visible_y = 192;
        // register reset
        v9938_reset_palette (); // palette registers
        for (i=0;i<10;i++) m_stat_reg[i] = 0;
        m_stat_reg[2] = 0x0c;
        if (m_model == MODEL_V9958) m_stat_reg[1] |= 4;
        for (i=0;i<48;i++) m_cont_reg[i] = 0;
        m_cmd_write_first = 0;
	m_pal_write_first = 0;
        m_int_state = 0;
        m_read_ahead = 0; m_address_latch = 0; // ???
        //m_scanline = 0;
        // MZ: The status registers 4 and 6 hold the high bits of the sprite
        // collision location. The unused bits are set to 1.
        // SR3: x x x x x x x x
        // SR4: 1 1 1 1 1 1 1 x
        // SR5: y y y y y y y y
        // SR6: 1 1 1 1 1 1 y y
        // Note that status register 4 is used in detection algorithms to tell
        // apart the tms9929 from the v99x8.

        // TODO: SR3-S6 do not yet store the information about the sprite collision
        m_stat_reg[4] = 0xfe;
        m_stat_reg[6] = 0xfc;
        // Start the timer
        //m_line_timer->adjust(attotime::from_ticks(HTOTAL*2, m_clock), 0, attotime::from_ticks(HTOTAL*2, m_clock));

        //configure_pal_ntsc();
        //set_screen_parameters();
	for (uint32_t j=0; j< VRAM_SIZE;j++) {
		vram[j] = j & 0xff;
	}

	m_vdp_ops_count=1;
	m_vdp_engine = nullptr;

}


// taken from V9938 Technical Data book, page 148. it's in G-R-B format
static const uint8_t pal16[16*3] = {
	0, 0, 0, // 0: black/transparent
	0, 0, 0, // 1: black
	6, 1, 1, // 2: medium green
	7, 3, 3, // 3: light green
	1, 1, 7, // 4: dark blue
	3, 2, 7, // 5: light blue
	1, 5, 1, // 6: dark red
	6, 2, 7, // 7: cyan
	1, 7, 1, // 8: medium red
	3, 7, 3, // 9: light red
	6, 6, 1, // 10: dark yellow
	6, 6, 4, // 11: light yellow
	4, 1, 1, // 12: dark green
	2, 6, 5, // 13: magenta
	5, 5, 5, // 14: gray
	7, 7, 7  // 15: white
};


void v9938_reset_palette()
{
        int i, red;

        for (i=0;i<16;i++)
        {
                // set the palette registers
                m_pal_reg[i*2+0] = pal16[i*3+1] << 4 | pal16[i*3+2];
                m_pal_reg[i*2+1] = pal16[i*3];
                // set the reference table
                set_pen16_rgb(i, pal16[i*3+1], pal16[i*3], pal16[i*3+2]);
        }

        // set internal palette GRAPHIC 7
        for (i=0;i<256;i++)
        {
                red = (i << 1) & 6; if (red == 6) red++;

                set_pen256_rgb(i, ((i & 0x1c) >> 2) <<5, ((i & 0xe0) >> 5) <<5 , red << 5);
        }
}


/***************************************************************************

Memory functions

***************************************************************************/

void v9938_vram_write(int offset, int data)
{
        int newoffset;

        if ( (m_mode == V9938_MODE_GRAPHIC6) || (m_mode == V9938_MODE_GRAPHIC7) )
        {
                newoffset = ((offset & 1) << 16) | (offset >> 1);
                if (newoffset < m_vram_size)
                        vram[newoffset] = data;
        }
        else
        {
                if (offset < m_vram_size)
                        vram[offset] = data;
        }
}

inline int v9938_vram_read(int offset)
{
        if ( (m_mode == V9938_MODE_GRAPHIC6) || (m_mode == V9938_MODE_GRAPHIC7) )
                return vram[((offset & 1) << 16) | (offset >> 1)];
        else
                return vram[offset];
}

void v9938_check_int()
{

}

/***************************************************************************

    Register functions

***************************************************************************/

void v9938_register_write (int reg, int data)
{
        static uint8_t const reg_mask[] =
        {
                0x7e, 0x7b, 0x7f, 0xff, 0x3f, 0xff, 0x3f, 0xff,
                0xfb, 0xbf, 0x07, 0x03, 0xff, 0xff, 0x07, 0x0f,
                0x0f, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                0x00, 0x7f, 0x3f, 0x07
        };

        if (reg <= 27)
        {
                data &= reg_mask[reg];
                if (m_cont_reg[reg] == data)
                        return;
        }

        if (reg > 46)
        {
                return;
        }

        /*update_command ();*/

        switch (reg) {
                // registers that affect interrupt and display mode
        case 0:
        case 1:
                m_cont_reg[reg] = data;
                v9938_set_mode();
                //check_int();
                break;

        case 18:
        case 9:
                m_cont_reg[reg] = data;
                // recalc offset
                m_offset_x = 8 + position_offset(m_cont_reg[18] & 0x0f);
                // Y offset is only applied once per frame?
                break;

        case 15:
                m_pal_write_first = 0;
                break;

                // color burst registers aren't emulated
        case 20:
        case 21:
        case 22:
                break;
        case 25:
        case 26:
        case 27:
                if (m_model != MODEL_V9958)
                {
                        data = 0;
                }
                else
                {
                        if(reg == 25)
                                m_v9958_sp_mode = data & 0x18;
                }
                break;

        case 44:
                v9938_cpu_to_vdp(data);
                break;

        case 46:
                v9938_command_unit_w(data);
                break;
        }

        //if (reg != 15)
         //       LOGMASKED(LOG_REGWRITE, "Write %02x to R#%d\n", data, reg);

        m_cont_reg[reg] = data;
}


/***************************************************************************

Refresh / render function

***************************************************************************/

inline bool v9938_second_field()
{
        return !(((m_cont_reg[9] & 0x04) && !(m_stat_reg[2] & 2)) || m_blink);
}



void v9938_set_mode()
{
        m_mode = (((m_cont_reg[0] & 0x0e) << 1) | ((m_cont_reg[1] & 0x18) >> 3));
}





// The original v9938.c has much of v9938_refresh_line() below as separate functions. Below is an attempt 
// to improve performance by having one very large function. It still does not work 100% at all. MSX1 modes
// and sprites work quite well. But much of the extended stuff in the V9938 does not.
void v9938_refresh_line(int line) {
	uint32_t nametbl_addr, colourtbl_addr, patterntbl_addr, attrtbl_addr, line2, linemask, name, colourmask, patternmask, fg, bg, left_pen, right_pen, colour_zero;
	// even though these are 8 bit, it makes the arithmetic later easier if they are bigger than 8 bit
	uint32_t colour, charcode, pattern;

	uint8_t col[256];
	uint32_t spr;

	sl_pos = 0;
	sl = vga_prepare_scanline();

        // Pad 40 pixels on the left to center horizontally
        sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
        sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
        sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
        sl->data[sl_pos++] = (0 | THEN_EXTEND_3) | ((0 | THEN_EXTEND_3) << 16);  // 8 pixels

	sl_pos_line_start = sl_pos;

	colour_zero = v9938_palette[0];
	if ( !(m_cont_reg[8] & 0x20) && (m_mode != V9938_MODE_GRAPHIC5) ) {
		v9938_palette[0] = v9938_palette[m_cont_reg[7] & 0x0f ];
	}

	switch (m_mode) {
		case 0:
			nametbl_addr = (m_cont_reg[2] << 10);
			colourtbl_addr = (m_cont_reg[3] << 6) + (m_cont_reg[10] << 14);
			patterntbl_addr = (m_cont_reg[4] << 11);

			line2 = (line - m_cont_reg[23]) & 255;

			name = (line2 >> 3) << 5;

			spr=0;
			for (uint32_t x=0;x<32;x++)
			{
				charcode = vram[nametbl_addr + name];
				colour = vram[colourtbl_addr + (charcode >> 3)];
				fg = v9938_palette[colour >> 4];
				bg = v9938_palette[colour & 15];
				pattern = vram[patterntbl_addr + ((charcode  << 3) + (line2 & 7))];

				uint32_t bits_to_pixelpair[4] = {
				   (bg << 16) | (THEN_EXTEND_1 << 16) | bg | THEN_EXTEND_1,
				   (fg << 16) | (THEN_EXTEND_1 << 16) | bg | THEN_EXTEND_1,
				   (bg << 16) | (THEN_EXTEND_1 << 16) | fg | THEN_EXTEND_1,
				   (fg << 16) | (THEN_EXTEND_1 << 16) | fg | THEN_EXTEND_1,
				};

				for(int i = 0; i < 4; i++) {
				   // help init the sprite col
			           col[spr++]=0; col[spr++]=0;
				   sl->data[sl_pos] = bits_to_pixelpair[(pattern >> 6) & 0x03];
				   sl_pos++;
				   pattern <<= 2;
				}

				name++;
			}
			break;

		case 1: /* MULTI */
			nametbl_addr = (m_cont_reg[2] << 10);
			patterntbl_addr = (m_cont_reg[4] << 11);

			line2 = (line - m_cont_reg[23]) & 255;
			name = (line2 >> 3) << 5;

			spr=0;
			for (uint32_t x=0;x<32;x++)
			{
				col[spr++]=0; col[spr++]=0; col[spr++]=0; col[spr++]=0; col[spr++]=0; col[spr++]=0; col[spr++]=0; col[spr++]=0;
				colour = vram[patterntbl_addr + (vram[nametbl_addr + name] << 3) + ((line2 >> 2)&7)];
				uint32_t pen = v9938_palette[colour >> 4];
				/* eight pixels */
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;

				pen = v9938_palette[colour & 15];
				/* eight pixels */
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;
				sl->data[sl_pos++] = (pen << 16) | (THEN_EXTEND_1 << 16) | pen | THEN_EXTEND_1;
				name++;
			}

			break;
		case 2: /* TEXT 40 */
			patterntbl_addr = m_cont_reg[4] << 11;
			nametbl_addr = m_cont_reg[2] << 10;

			fg = v9938_palette[m_cont_reg[7] >> 4];
			bg = v9938_palette[m_cont_reg[7] & 15];

			name = (line >> 3)*40;

			uint32_t bits_to_pixelpair[4] = {
			  (bg << 16) | (THEN_EXTEND_1 << 16) | bg | THEN_EXTEND_1,
			  (fg << 16) | (THEN_EXTEND_1 << 16) | bg | THEN_EXTEND_1,
			  (bg << 16) | (THEN_EXTEND_1 << 16) | fg | THEN_EXTEND_1,
			  (fg << 16) | (THEN_EXTEND_1 << 16) | fg | THEN_EXTEND_1,
			};

			for (uint32_t x=0;x<40;x++)
			{
				pattern = vram[patterntbl_addr + (vram[nametbl_addr + name] << 3) + ((line + m_cont_reg[23]) & 7)];
				for(int i = 0; i < 3; i++) {
					 sl->data[sl_pos] = bits_to_pixelpair[(pattern >> 6) & 0x03];
					 sl_pos++;
					 pattern <<= 2;
				}

				/* width height 212, characters start repeating at the bottom */
				name = (name + 1) & 0x3ff;
			}
			break;
		case 4:
		case 8:
			colourmask = ((m_cont_reg[3] & 0x7f) << 3) | 7;
			patternmask = ((m_cont_reg[4] & 0x03) << 8) | 0xff;

			nametbl_addr =  (m_cont_reg[2] << 10);
			colourtbl_addr =  ((m_cont_reg[3] & 0x80) << 6) + (m_cont_reg[10] << 14);
			patterntbl_addr = ((m_cont_reg[4] & 0x3c) << 11);

			line2 = (line + m_cont_reg[23]) & 255;
			name = (line2 >> 3) << 5;


			spr=0;
			for (uint32_t x=0;x<32;x++)
			{
				charcode = vram[nametbl_addr + name] + ((line2&0xc0) << 2);
				colour = vram[colourtbl_addr + (((charcode&colourmask) << 3)+(line2&7))];
				pattern = vram[patterntbl_addr + (((charcode&patternmask) << 3) + (line2&7))];
				fg = v9938_palette[colour >> 4];
				bg = v9938_palette[colour & 15];

				uint32_t bits_to_pixelpair[4] = {
				   (bg << 16) | (THEN_EXTEND_1 << 16) | bg | THEN_EXTEND_1,
				   (fg << 16) | (THEN_EXTEND_1 << 16) | bg | THEN_EXTEND_1,
				   (bg << 16) | (THEN_EXTEND_1 << 16) | fg | THEN_EXTEND_1,
				   (fg << 16) | (THEN_EXTEND_1 << 16) | fg | THEN_EXTEND_1,
				};

				for(int i = 0; i < 4; i++) {
			           col[spr++]=0; col[spr++]=0;
				   sl->data[sl_pos] = bits_to_pixelpair[(pattern >> 6) & 0x03];
				   sl_pos++;
				   pattern <<= 2;
				}
				name++;
			}

			break;
		case 0x0c: /* graphic4 */
			linemask = ((m_cont_reg[2] & 0x1f) << 3) | 7;

			line2 = ((line + m_cont_reg[23]) & linemask) & 255;

			nametbl_addr = ((m_cont_reg[2] & 0x40) << 10) + line2 * 128;
			if ( (m_cont_reg[2] & 0x20) && v9938_second_field() )
				nametbl_addr += 0x8000;

			spr=0;
//			for (register uint32_t x=0;x<64;x++)
//			{
//				col[spr++]=0; col[spr++]=0;
//				colour = vram[nametbl_addr++];
//				left_pen = v9938_palette[colour >> 4];
//				right_pen = v9938_palette[colour & 15];
//				sl->data[sl_pos++] = (right_pen << 16) | (THEN_EXTEND_1 << 16) | left_pen | THEN_EXTEND_1;
//			}

			// Attempt to speed this up.
			register uint32_t *p;

			p = (uint32_t*) &vram[nametbl_addr];

			for (register uint32_t x=0;x<32;x++)
			{
				col[spr++]=0; col[spr++]=0;
				col[spr++]=0; col[spr++]=0;
				col[spr++]=0; col[spr++]=0;
				col[spr++]=0; col[spr++]=0;
				// grab 4 bytes at a time. The endian-ness makes it so the left most byte is in b7-b0
				register uint32_t col32 = *p++;
				register uint8_t c = (uint8_t) (col32 & 0xff);
				register uint32_t left_pen = v9938_palette[c >> 4];
				register uint32_t right_pen = v9938_palette[c & 15];
				sl->data[sl_pos++] = (right_pen << 16) | (THEN_EXTEND_1 << 16) | left_pen | THEN_EXTEND_1;
				c = (uint8_t) ((col32 >> 8) & 0xff);
				left_pen = v9938_palette[c >> 4];
				right_pen = v9938_palette[c & 15];
				sl->data[sl_pos++] = (right_pen << 16) | (THEN_EXTEND_1 << 16) | left_pen | THEN_EXTEND_1;
				c = (uint8_t) ((col32 >> 16) & 0xff);
				left_pen = v9938_palette[c >> 4];
				right_pen = v9938_palette[c & 15];
				sl->data[sl_pos++] = (right_pen << 16) | (THEN_EXTEND_1 << 16) | left_pen | THEN_EXTEND_1;
				c = (uint8_t) ((col32 >> 24)& 0xff);
				left_pen = v9938_palette[c >> 4];
				right_pen = v9938_palette[c & 15];
				sl->data[sl_pos++] = (right_pen << 16) | (THEN_EXTEND_1 << 16) | left_pen | THEN_EXTEND_1;
			}

			break;
		case 0x10: /* graphic5 */
			linemask = ((m_cont_reg[2] & 0x1f) << 3) | 7;

			line2 = ((line + m_cont_reg[23]) & linemask) & 255;

			nametbl_addr = ((m_cont_reg[2] & 0x40) << 10) + (line2 << 7);
			if ( (m_cont_reg[2] & 0x20) && v9938_second_field() )
				nametbl_addr += 0x8000;

			uint32_t pen_bg0[4];
			uint32_t pen_bg1[4];
			pen_bg1[0] = v9938_palette[m_cont_reg[7] & 0x03];
			pen_bg0[0] = v9938_palette[(m_cont_reg[7] >> 2) & 0x03];

			register uint32_t x;
			x = (m_cont_reg[8] & 0x20) ? 0 : 1;

			for (;x<4;x++)
			{
				pen_bg0[x] = v9938_palette[x];
				pen_bg1[x] = v9938_palette[x];
			}

			spr=0;
			for (x=0;x<128;x++)
			{
				col[spr++]=0;col[spr++]=0;
				colour = vram[nametbl_addr++];
				sl->data[sl_pos++] = ( pen_bg1[(colour>>4)&3] << 16) | pen_bg0[colour>>6];
				sl->data[sl_pos++] = ( pen_bg1[colour&3] << 16) | pen_bg0[(colour>>2)&3];
			}

			//pen_bg1[0] = pen16(m_cont_reg[7] & 0x03);
			//pen_bg0[0] = pen16((m_cont_reg[7] >> 2) & 0x03);
			//xx = 16 - m_offset_x;
			//while (xx--) { *ln++ = pen_bg0[0]; *ln++ = pen_bg1[0]; }
			break;

		case 0x14: /* graphic6 */
			//uint8_t colour;
			//int line2, linemask, x, xx, nametbl_addr;
			//uint32_t pen_bg, fg0;
			//uint32_t fg1;

			linemask = ((m_cont_reg[2] & 0x1f) << 3) | 7;

			line2 = ((line + m_cont_reg[23]) & linemask) & 255;

			nametbl_addr = line2 << 8 ;
			if ( (m_cont_reg[2] & 0x20) && v9938_second_field() )
				nametbl_addr += 0x10000;

			//pen_bg = v9938_palette[m_cont_reg[7] & 0x0f];
			//xx = m_offset_x * 2;
			//while (xx--) *ln++ = pen_bg;

			uint32_t fg0, fg1;
			if (m_cont_reg[2] & 0x40)
			{
				for (x=0;x<32;x++)
				{
					nametbl_addr++;
					colour = vram[((nametbl_addr&1) << 16) | (nametbl_addr>>1)];
					fg0 = v9938_palette[colour >> 4];
					fg1 = v9938_palette[colour & 15];

					sl->data[sl_pos++] = ( fg1 << 16) | fg0;
					sl->data[sl_pos++] = ( fg1 << 16) | fg0;
					sl->data[sl_pos++] = ( fg1 << 16) | fg0;
					sl->data[sl_pos++] = ( fg1 << 16) | fg0;

					sl->data[sl_pos++] = ( fg1 << 16) | fg0;
					sl->data[sl_pos++] = ( fg1 << 16) | fg0;
					sl->data[sl_pos++] = ( fg1 << 16) | fg0;
					sl->data[sl_pos++] = ( fg1 << 16) | fg0;
					nametbl_addr += 7;
				}
			}
			else
			{
				for (x=0;x<256;x++)
				{
					colour = vram[((nametbl_addr&1) << 16) | (nametbl_addr>>1)];
					fg0 = v9938_palette[colour >> 4];
					fg1 = v9938_palette[colour & 15];
					sl->data[sl_pos++] = ( fg1 << 16) | fg0;

					nametbl_addr++;
				}
			}


			break;

		default:
			break;


	}


	// do sprites
	switch (m_mode) {
		case 0:
		case 1:
		case 4:
			if (!(m_cont_reg[8] & 0x02)) {
				int pattern_addr;
				int p, height, p2, i, n, pattern;
				register uint8_t  y, c;
				register int x;

				//for (int i=0;i<256;i++) { col[i]=0; }

				attrtbl_addr = (m_cont_reg[5] << 7) + (m_cont_reg[11] << 15);
				patterntbl_addr = (m_cont_reg[6] << 11);

				// 16x16 or 8x8 sprites
				height = (m_cont_reg[1] & 2) ? 16 : 8;
				// magnified sprites (zoomed)
				if (m_cont_reg[1] & 1) height *= 2;

				p2 = p = 0;
				while (1)
				{
					// sprite attribute tables is 
					//   0: y
					//   1: x
					//   2: pattern table ref
					//   3: bit 7 is 'early clock bit' and b3-b0 is the colour
					y = vram[attrtbl_addr];
					if (y == 208) break;
					y = (y - m_cont_reg[23]) & 255;
					if (y > 208)
						y = -(~y&255);
					else
						y++;

					// if sprite in range, has to be drawn
					if ( (line >= y) && (line  < (y + height) ) )
					{
						if (p2 == 4)
						{
							// max maximum sprites per line!
							if ( !(m_stat_reg[0] & 0x40) )
								m_stat_reg[0] = (m_stat_reg[0] & 0xa0) | 0x40 | p;

							break;
						}
						// get x
						x = vram[attrtbl_addr + 1];
						if (vram[attrtbl_addr + 3] & 0x80) x -= 32;

						// get pattern
						pattern = vram[attrtbl_addr + 2];
						if (m_cont_reg[1] & 2)
							pattern &= 0xfc;
						n = line - y;
						pattern_addr = patterntbl_addr + (pattern << 3) + ((m_cont_reg[1] & 1) ? (n >>1)  : n);
						pattern = (vram[pattern_addr] << 8) | vram[pattern_addr+16];

						// get colour
						c = vram[attrtbl_addr + 3] & 0x0f;

						// draw left part
						n = 0;
						while (1)
						{
							if (n == 0) pattern = vram[pattern_addr];
							else if ( (n == 1) && (m_cont_reg[1] & 2) ) pattern = vram[pattern_addr + 16];
							else break;

							n++;

							for (i=0;i<8;i++)
							{
								if (pattern & 0x80)
								{
									if ( (x >= 0) && (x < 256) )
									{
										if (col[x] & 0x40)
										{
											// we have a collision!
											//if (p2 < 4)
											//        m_stat_reg[0] |= 0x20;
										}
										if ( !(col[x] & 0x80) )
										{
											if (c || (m_cont_reg[8] & 0x20) ) {
												col[x] |= 0xc0 | c;
												if (x & 1) {
													uint32_t last_pixel_pair = sl->data[sl_pos_line_start + (x>>1)];
													sl->data[sl_pos_line_start + (x>>1)] = (last_pixel_pair & 0x0000ffff) | (( v9938_palette[c & 0x0f] | THEN_EXTEND_1) << 16);
												} else {
													uint32_t last_pixel_pair = sl->data[sl_pos_line_start + (x>>1)];
													sl->data[sl_pos_line_start + (x>>1)] = (last_pixel_pair & 0xffff0000) | ( v9938_palette[c & 0x0f] | THEN_EXTEND_1) ;
												}
											} else {
												col[x] |= 0x40;
											}
										}

										// if zoomed, draw another pixel
										if (m_cont_reg[1] & 1)
										{
											if (col[x+1] & 0x40)
											{
												// we have a collision!
												//if (p2 < 4)
												//        m_stat_reg[0] |= 0x20;
											}
											if ( !(col[x+1] & 0x80) )
											{
												if (c || (m_cont_reg[8] & 0x20) ) {
													col[x+1] |= 0xc0 | c;
													if ((x+1) & 1) {
														uint32_t last_pixel_pair = sl->data[sl_pos_line_start + ((x+1)>>1)];
														sl->data[sl_pos_line_start + ((x+1)>>1)] = (last_pixel_pair & 0x0000ffff) | (( v9938_palette[c & 0x0f] | THEN_EXTEND_1) << 16);
													} else {
														uint32_t last_pixel_pair = sl->data[sl_pos_line_start + ((x+1)>>1)];
														sl->data[sl_pos_line_start + ((x+1)>>1)] = (last_pixel_pair & 0xffff0000) | ( v9938_palette[c & 0x0f] | THEN_EXTEND_1) ;
													}
												} else {
													col[x+1] |= 0x80;
												}
											}
										}
									}
								}
								if (m_cont_reg[1] & 1) x += 2; else x++;
								pattern <<= 1;
							}
						}

						p2++;
					}

					if (p >= 31) break;
					p++;
					attrtbl_addr += 4;
				}

				if ( !(m_stat_reg[0] & 0x40) )
					m_stat_reg[0] = (m_stat_reg[0] & 0xa0) | p;
			}

			break;
		case 0x08:
		case 0x0c:
		case 0x10:
		case 0x14:
		case 0x1c:
			if (!(m_cont_reg[8] & 0x02)) {
				int pattern_addr;
				int p, height, p2, i, n, pattern;
				register uint8_t  y, c, first_cc_seen;
				register int x;
				//int attrtbl_addr, patterntbl_addr, pattern_addr, colourtbl_addr;
				//int x,i;
				//int y, p, height, c, p2, n, pattern, colourmask, first_cc_seen;

				//for (int i=0;i<256;i++) { col[i]=0; }

				//attrtbl_addr = ( (m_cont_reg[5] & 0xfc) << 7) + (m_cont_reg[11] << 15);
				//colourtbl_addr =  ( (m_cont_reg[5] & 0xf8) << 7) + (m_cont_reg[11] << 15);
				attrtbl_addr = ( (m_cont_reg[5]  & 0xfc ) << 7) + (m_cont_reg[11] << 15);
				colourtbl_addr =  attrtbl_addr - 512;
				patterntbl_addr = (m_cont_reg[6] << 11);
				colourmask = ( (m_cont_reg[5] & 3) << 3) | 0x7; // check this!

				// 16x16 or 8x8 sprites
				height = (m_cont_reg[1] & 2) ? 16 : 8;
				// magnified sprites (zoomed)
				if (m_cont_reg[1] & 1) height *= 2;

				p2 = p = first_cc_seen = 0;
				while (1)
				{
					y = v9938_vram_read(attrtbl_addr);
					if (y == 216) break;
					y = (y - m_cont_reg[23]) & 255;
					if (y > 216)
						y = -(~y&255);
					else
						y++;

					// if sprite in range, has to be drawn
					if ( (line >= y) && (line  < (y + height) ) )
					{
						if (p2 == 8)
						{
							// max maximum sprites per line!
							if ( !(m_stat_reg[0] & 0x40) )
								m_stat_reg[0] = (m_stat_reg[0] & 0xa0) | 0x40 | p;

							break;
						}

						n = line - y; if (m_cont_reg[1] & 1)  { n = n >> 1; };
						// get colour
						c = v9938_vram_read(colourtbl_addr + (((p&colourmask)<<4) + n));

						// don't draw all sprite with CC set before any sprites
						// with CC = 0 are seen on this line
						if (c & 0x40)
						{
							if (!first_cc_seen)
								goto skip_first_cc_set;
						}
						else
							first_cc_seen = 1;

						// get pattern
						pattern = v9938_vram_read(attrtbl_addr + 2);
						if (m_cont_reg[1] & 2)
							pattern &= 0xfc;
						pattern_addr = patterntbl_addr + pattern * 8 + n;
						pattern = (v9938_vram_read(pattern_addr) << 8) | v9938_vram_read(pattern_addr + 16);

						// get x
						x = v9938_vram_read(attrtbl_addr + 1);
						if (c & 0x80) x -= 32;

						// n is the height of the sprite
						n = (m_cont_reg[1] & 2) ? 16 : 8;
						while (n--)
						{
							for (i=0;i<=(m_cont_reg[1] & 1);i++)
							{
								if ( (x >= 0) && (x < 256) )
								{
									if ( (pattern & 0x8000) && !(col[x] & 0x10) )
									{
										if ( (c & 15) || (m_cont_reg[8] & 0x20) )
										{
											//if ( !(c & 0x40) )
											//{
											//	if (col[x] & 0x20) col[x] |= 0x10;
											//	else {
											//		col[x] |= 0x20 | (c & 15);
											//	}
										//	
											//}
											//else {
											//	col[x] |= c & 15;
											//}

											//col[x] |= 0x80;
											uint_fast32_t last_pixel_pair = sl->data[sl_pos_line_start + (x>>1)];
											if (m_mode != V9938_MODE_GRAPHIC5) {
												if (x & 1) {
													sl->data[sl_pos_line_start + (x>>1)] = (last_pixel_pair & 0x0000ffff) | (( v9938_palette[c & 0x0f] | THEN_EXTEND_1) << 16);
												} else {
													sl->data[sl_pos_line_start + (x>>1)] = (last_pixel_pair & 0xffff0000) | ( v9938_palette[c & 0x0f] | THEN_EXTEND_1) ;
												}
											} else {
												// TODO. graphic5 sprites are actually rendered at 512 pix. THat means rewriting all the rendering engine stuff
												if (x & 1) {
													sl->data[sl_pos_line_start + (x>>1)] = (last_pixel_pair & 0x0000ffff) | (( v9938_palette[(c >> 2) & 3] | THEN_EXTEND_1) << 16);
												} else {
													sl->data[sl_pos_line_start + (x>>1)] = (last_pixel_pair & 0xffff0000) | ( v9938_palette[(c >> 2) & 3] | THEN_EXTEND_1) ;
												}
											}


										}
									}
									else
									{
										//if ( !(c & 0x40) && (col[x] & 0x20) )
										//	col[x] |= 0x10;
									}

									//if ( !(c & 0x60) && (pattern & 0x8000) )
									//{
									//	if (col[x] & 0x40)
									//	{
									//		// sprite collision!
									//		if (p2 < 8)
									//			m_stat_reg[0] |= 0x20;
									//	}
									//	else
									//		col[x] |= 0x40;
									//}

									x++;
								}
							}

							pattern <<= 1;
						}

					skip_first_cc_set:
						p2++;
					}

					if (p >= 31) break;
					p++;
					attrtbl_addr += 4;
				}

				//if ( !(m_stat_reg[0] & 0x40) )
				//	m_stat_reg[0] = (m_stat_reg[0] & 0xa0) | p;
			}

			break;

	}
	
	if ( !(m_cont_reg[8] & 0x20)) {
		v9938_palette[0] = colour_zero;
	}

        sl->repeat_count = 1;
        sl->length = sl_pos;
        vga_submit_scanline(sl);

}


// Only called from the render core

void render_v9938() {
	uint8_t visible_lines;
	vga_prepare_frame();
	// Skip 48 lines to center vertically
	//vga_skip_lines(84);
	for (int i=0; i< 64; i++) {
		vga_skip_lines(1);
	}


	visible_lines = (m_cont_reg[9] & 0x80) ? 212 : 192;
	for(int line = 0; line < visible_lines; line++) {
		v9938_refresh_line(line);
	}

	for (int i=0;i<256;i++) {
		v9938_update_command();
	}

}

/***************************************************************************

Command unit v9938

***************************************************************************/

/*************************************************************/
/** Completely rewritten by Alex Wulms:                     **/
/**  - VDP Command execution 'in parallel' with CPU         **/
/**  - Corrected behaviour of VDP commands                  **/
/**  - Made it easier to implement correct S7/8 mapping     **/
/**    by concentrating VRAM access in one single place     **/
/**  - Made use of the 'in parallel' VDP command exec       **/
/**    and correct timing. You must call the function       **/
/**    LoopVDP() from LoopZ80 in MSX.c. You must call it    **/
/**    exactly 256 times per screen refresh.                **/
/** Started on       : 11-11-1999                           **/
/** Beta release 1 on:  9-12-1999                           **/
/** Beta release 2 on: 20-01-2000                           **/
/**  - Corrected behaviour of VRM <-> Z80 transfer          **/
/**  - Improved performance of the code                     **/
/** Public release 1.0: 20-04-2000                          **/
/*************************************************************/

#define VDP_VRMP5(MX, X, Y) ((!MX) ? (((Y&1023)<<7) + ((X&255)>>1)) : (EXPMEM_OFFSET + ((Y&511)<<7) + ((X&255)>>1)))
#define VDP_VRMP6(MX, X, Y) ((!MX) ? (((Y&1023)<<7) + ((X&511)>>2)) : (EXPMEM_OFFSET + ((Y&511)<<7) + ((X&511)>>2)))
//#define VDP_VRMP7(MX, X, Y) ((!MX) ? (((Y&511)<<8) + ((X&511)>>1)) : (EXPMEM_OFFSET + ((Y&255)<<8) + ((X&511)>>1)))
#define VDP_VRMP7(MX, X, Y) ((!MX) ? (((X&2)<<15) + ((Y&511)<<7) + ((X&511)>>2)) : (EXPMEM_OFFSET + ((Y&511)<<7) + ((X&511)>>2))/*(EXPMEM_OFFSET + ((Y&255)<<8) + ((X&511)>>1))*/)
//#define VDP_VRMP8(MX, X, Y) ((!MX) ? (((Y&511)<<8) + (X&255)) : (EXPMEM_OFFSET + ((Y&255)<<8) + (X&255)))
#define VDP_VRMP8(MX, X, Y) ((!MX) ? (((X&1)<<16) + ((Y&511)<<7) + ((X>>1)&127)) : (EXPMEM_OFFSET + ((Y&511)<<7) + ((X>>1)&127))/*(EXPMEM_OFFSET + ((Y&255)<<8) + (X&255))*/)

#define VDP_VRMP(M, MX, X, Y) v9938_VDPVRMP(M, MX, X, Y)
#define VDP_POINT(M, MX, X, Y) v9938_VDPpoint(M, MX, X, Y)
#define VDP_PSET(M, MX, X, Y, C, O) v9938_VDPpset(M, MX, X, Y, C, O)

#define CM_ABRT  0x0
#define CM_POINT 0x4
#define CM_PSET  0x5
#define CM_SRCH  0x6
#define CM_LINE  0x7
#define CM_LMMV  0x8
#define CM_LMMM  0x9
#define CM_LMCM  0xA
#define CM_LMMC  0xB
#define CM_HMMV  0xC
#define CM_HMMM  0xD
#define CM_YMMM  0xE
#define CM_HMMC  0xF

/*************************************************************
Many VDP commands are executed in some kind of loop but
essentially, there are only a few basic loop structures
that are re-used. We define the loop structures that are
re-used here so that they have to be entered only once
*************************************************************/
#define pre_loop \
while ((cnt-=delta) > 0) {
	#define post_loop \
}

// Loop over DX, DY
#define post__x_y(MX) \
if (!--ANX || ((ADX+=TX)&MX)) { \
	if (!(--NY&1023) || (DY+=TY)==-1) \
		break; \
	else { \
		ADX=DX; \
		ANX=NX; \
	} \
} \
post_loop

// Loop over DX, SY, DY
#define post__xyy(MX) \
if ((ADX+=TX)&MX) { \
	if (!(--NY&1023) || (SY+=TY)==-1 || (DY+=TY)==-1) \
		break; \
	else \
		ADX=DX; \
} \
post_loop

// Loop over SX, DX, SY, DY
#define post_xxyy(MX) \
if (!--ANX || ((ASX+=TX)&MX) || ((ADX+=TX)&MX)) { \
	if (!(--NY&1023) || (SY+=TY)==-1 || (DY+=TY)==-1) \
		break; \
	else { \
		ASX=SX; \
		ADX=DX; \
		ANX=NX; \
	} \
} \
post_loop

/*************************************************************/
/** Variables visible only in this module                   **/
/*************************************************************/
static const uint8_t Mask[4] = { 0x0F,0x03,0x0F,0xFF };
static const int  PPB[4]  = { 2,4,2,1 };
static const int  PPL[4]  = { 256,512,512,256 };

//  SprOn SprOn SprOf SprOf
//  ScrOf ScrOn ScrOf ScrOn
static const int srch_timing[8]={
	818, 1025,  818,  830, // ntsc
	696,  854,  696,  684  // pal
};
static const int line_timing[8]={
	1063, 1259, 1063, 1161,
	904,  1026, 904,  953
};
static const int hmmv_timing[8]={
	439,  549,  439,  531,
	366,  439,  366,  427
};
static const int lmmv_timing[8]={
	873,  1135, 873, 1056,
	732,  909,  732,  854
};
static const int ymmm_timing[8]={
	586,  952,  586,  610,
	488,  720,  488,  500
};
static const int hmmm_timing[8]={
	818,  1111, 818,  854,
	684,  879,  684,  708
};
static const int lmmm_timing[8]={
	1160, 1599, 1160, 1172,
	964,  1257, 964,  977
};

/** VDPVRMP() **********************************************/
/** Calculate addr of a pixel in vram                       **/
/*************************************************************/
inline int v9938_VDPVRMP(uint8_t M,int MX,int X,int Y)
{
	switch(M)
	{
	case 0: return VDP_VRMP5(MX,X,Y);
	case 1: return VDP_VRMP6(MX,X,Y);
	case 2: return VDP_VRMP7(MX,X,Y);
	case 3: return VDP_VRMP8(MX,X,Y);
	}

	return 0;
}

/** VDPpoint5() ***********************************************/
/** Get a pixel on screen 5                                 **/
/*************************************************************/
inline uint8_t v9938_VDPpoint5(int MXS, int SX, int SY)
{
	return (vram[VDP_VRMP5(MXS, SX, SY)] >>
		(((~SX)&1)<<2)
		)&15;
}

/** VDPpoint6() ***********************************************/
/** Get a pixel on screen 6                                 **/
/*************************************************************/
inline uint8_t v9938_VDPpoint6(int MXS, int SX, int SY)
{
	return (vram[VDP_VRMP6(MXS, SX, SY)] >>
		(((~SX)&3)<<1)
		)&3;
}

/** VDPpoint7() ***********************************************/
/** Get a pixel on screen 7                                 **/
/*************************************************************/
inline uint8_t v9938_VDPpoint7(int MXS, int SX, int SY)
{
	return (vram[VDP_VRMP7(MXS, SX, SY)] >>
		(((~SX)&1)<<2)
		)&15;
}

/** VDPpoint8() ***********************************************/
/** Get a pixel on screen 8                                 **/
/*************************************************************/
inline uint8_t v9938_VDPpoint8(int MXS, int SX, int SY)
{
	return vram[VDP_VRMP8(MXS, SX, SY)];
}

/** VDPpoint() ************************************************/
/** Get a pixel on a screen                                 **/
/*************************************************************/
inline uint8_t v9938_VDPpoint(uint8_t SM, int MXS, int SX, int SY)
{
	switch(SM)
	{
	case 0: return v9938_VDPpoint5(MXS,SX,SY);
	case 1: return v9938_VDPpoint6(MXS,SX,SY);
	case 2: return v9938_VDPpoint7(MXS,SX,SY);
	case 3: return v9938_VDPpoint8(MXS,SX,SY);
	}

	return(0);
}

/** VDPpsetlowlevel() ****************************************/
/** Low level function to set a pixel on a screen           **/
/** Make it inline to make it fast                          **/
/*************************************************************/
inline void v9938_VDPpsetlowlevel(int addr, uint8_t CL, uint8_t M, uint8_t OP)
{
	// If this turns out to be too slow, get a pointer to the address space
	// and work directly on it.
	uint8_t val = vram[addr];
	switch (OP)
	{
	case 0: val = (val & M) | CL; break;
	case 1: val = val & (CL | M); break;
	case 2: val |= CL; break;
	case 3: val ^= CL; break;
	case 4: val = (val & M) | ~(CL | M); break;
	case 8: if (CL) val = (val & M) | CL; break;
	case 9: if (CL) val = val & (CL | M); break;
	case 10: if (CL) val |= CL; break;
	case 11:  if (CL) val ^= CL; break;
	case 12:  if (CL) val = (val & M) | ~(CL|M); break;
	default:
		break;
	}

	vram[addr] = val;
}

/** VDPpset5() ***********************************************/
/** Set a pixel on screen 5                                 **/
/*************************************************************/
inline void v9938_VDPpset5(int MXD, int DX, int DY, uint8_t CL, uint8_t OP)
{
	uint8_t SH = ((~DX)&1)<<2;
	v9938_VDPpsetlowlevel(VDP_VRMP5(MXD, DX, DY), CL << SH, ~(15<<SH), OP);
}

/** VDPpset6() ***********************************************/
/** Set a pixel on screen 6                                 **/
/*************************************************************/
inline void v9938_VDPpset6(int MXD, int DX, int DY, uint8_t CL, uint8_t OP)
{
	uint8_t SH = ((~DX)&3)<<1;

	v9938_VDPpsetlowlevel(VDP_VRMP6(MXD, DX, DY), CL << SH, ~(3<<SH), OP);
}

/** VDPpset7() ***********************************************/
/** Set a pixel on screen 7                                 **/
/*************************************************************/
inline void v9938_VDPpset7(int MXD, int DX, int DY, uint8_t CL, uint8_t OP)
{
	uint8_t SH = ((~DX)&1)<<2;

	v9938_VDPpsetlowlevel(VDP_VRMP7(MXD, DX, DY), CL << SH, ~(15<<SH), OP);
}

/** VDPpset8() ***********************************************/
/** Set a pixel on screen 8                                 **/
/*************************************************************/
inline void v9938_VDPpset8(int MXD, int DX, int DY, uint8_t CL, uint8_t OP)
{
	v9938_VDPpsetlowlevel(VDP_VRMP8(MXD, DX, DY), CL, 0, OP);
}

/** VDPpset() ************************************************/
/** Set a pixel on a screen                                 **/
/*************************************************************/
inline void v9938_VDPpset(uint8_t SM, int MXD, int DX, int DY, uint8_t CL, uint8_t OP)
{
	switch (SM) {
	case 0: v9938_VDPpset5(MXD, DX, DY, CL, OP); break;
	case 1: v9938_VDPpset6(MXD, DX, DY, CL, OP); break;
	case 2: v9938_VDPpset7(MXD, DX, DY, CL, OP); break;
	case 3: v9938_VDPpset8(MXD, DX, DY, CL, OP); break;
	}
}

#define FORCE_NTSC 0

/** get_vdp_timing_value() **************************************/
/** Get timing value for a certain VDP command              **/
/*************************************************************/
int v9938_get_vdp_timing_value(const int *timing_values)
{
	int t;
	// timing_values is an array of 8. The first 4 values related to NTSC. The next 4 to PAL
	// The 3 bit index consists of b2 = (NTSC=0, PAL=1), b1 = (sprites_on = 0, sprites_off = 1), b0 = (screen_off = 0, screen_on = 1)
	//return(timing_values[((m_cont_reg[1]>>6)&1)|(m_cont_reg[8]&2)|((m_cont_reg[9]<<1)&4)]);
	t = (timing_values[((m_cont_reg[1]>>6)&1)|(m_cont_reg[8]&2)| FORCE_NTSC]);
	t-=200;
	if (t < 0) {t = 5;}
	return 1;
	//return 80;
	//return 5;
}

/** SrchEgine()** ********************************************/
/** Search a dot                                            **/
/*************************************************************/
void v9938_srch_engine()
{
	int SX=m_mmc.SX;
	int SY=m_mmc.SY;
	int TX=m_mmc.TX;
	int ANX=m_mmc.ANX;
	uint8_t CL=m_mmc.CL;
	int MXD = m_mmc.MXD;
	int cnt;
	int delta;

	delta = v9938_get_vdp_timing_value(srch_timing);
	cnt = m_vdp_ops_count;

	#define post_srch(MX) \
	{ m_stat_reg[2]|=0x10; /* Border detected */ break; } \
	if ((SX+=TX) & MX) { m_stat_reg[2] &= 0xEF; /* Border not detected */ break; }
	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop if ((v9938_VDPpoint5(MXD, SX, SY)==CL) ^ANX)  post_srch(256) post_loop
			break;
	case V9938_MODE_GRAPHIC5: pre_loop if ((v9938_VDPpoint6(MXD, SX, SY)==CL) ^ANX)  post_srch(512) post_loop
			break;
	case V9938_MODE_GRAPHIC6: pre_loop if ((v9938_VDPpoint7(MXD, SX, SY)==CL) ^ANX)  post_srch(512) post_loop
			break;
	case V9938_MODE_GRAPHIC7: pre_loop if ((v9938_VDPpoint8(MXD, SX, SY)==CL) ^ANX)  post_srch(256) post_loop
			break;
	}

	if ((m_vdp_ops_count=cnt)>0) {
		// Command execution done
		m_stat_reg[2] &= 0xFE;
		m_vdp_engine = nullptr;
		// Update SX in VDP registers
		m_stat_reg[8] = SX & 0xFF;
		m_stat_reg[9] = (SX>>8) | 0xFE;
	}
	else {
		m_mmc.SX=SX;
	}
}

/** LineEgine()** ********************************************/
/** Draw a line                                             **/
/*************************************************************/
void v9938_line_engine()
{
	int DX=m_mmc.DX;
	int DY=m_mmc.DY;
	int TX=m_mmc.TX;
	int TY=m_mmc.TY;
	int NX=m_mmc.NX;
	int NY=m_mmc.NY;
	int ASX=m_mmc.ASX;
	int ADX=m_mmc.ADX;
	uint8_t CL=m_mmc.CL;
	uint8_t LO=m_mmc.LO;
	int MXD = m_mmc.MXD;
	int cnt;
	int delta;

	delta = v9938_get_vdp_timing_value(line_timing);
	cnt = m_vdp_ops_count;

	#define post_linexmaj(MX) \
	DX+=TX; \
	if ((ASX-=NY)<0) { \
		ASX+=NX; \
		DY+=TY; \
	} \
	ASX&=1023; /* Mask to 10 bits range */\
	if (ADX++==NX || (DX&MX)) \
		break; \
	post_loop

	#define post_lineymaj(MX) \
	DY+=TY; \
	if ((ASX-=NY)<0) { \
		ASX+=NX; \
		DX+=TX; \
	} \
	ASX&=1023; /* Mask to 10 bits range */\
	if (ADX++==NX || (DX&MX)) \
		break; \
	post_loop

	if ((m_cont_reg[45]&0x01)==0)
		// X-Axis is major direction
	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop v9938_VDPpset5(MXD, DX, DY, CL, LO); post_linexmaj(256)
		break;
	case V9938_MODE_GRAPHIC5: pre_loop v9938_VDPpset6(MXD, DX, DY, CL, LO); post_linexmaj(512)
		break;
	case V9938_MODE_GRAPHIC6: pre_loop v9938_VDPpset7(MXD, DX, DY, CL, LO); post_linexmaj(512)
		break;
	case V9938_MODE_GRAPHIC7: pre_loop v9938_VDPpset8(MXD, DX, DY, CL, LO); post_linexmaj(256)
		break;
	}
	else
		// Y-Axis is major direction
	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop v9938_VDPpset5(MXD, DX, DY, CL, LO); post_lineymaj(256)
		break;
	case V9938_MODE_GRAPHIC5: pre_loop v9938_VDPpset6(MXD, DX, DY, CL, LO); post_lineymaj(512)
		break;
	case V9938_MODE_GRAPHIC6: pre_loop v9938_VDPpset7(MXD, DX, DY, CL, LO); post_lineymaj(512)
		break;
	case V9938_MODE_GRAPHIC7: pre_loop v9938_VDPpset8(MXD, DX, DY, CL, LO); post_lineymaj(256)
		break;
	}

	if ((m_vdp_ops_count=cnt)>0) {
		// Command execution done
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		m_cont_reg[38]=DY & 0xFF;
		m_cont_reg[39]=(DY>>8) & 0x03;
	}
	else {
		m_mmc.DX=DX;
		m_mmc.DY=DY;
		m_mmc.ASX=ASX;
		m_mmc.ADX=ADX;
	}
}

/** lmmv_engine() *********************************************/
/** VDP -> Vram                                             **/
/*************************************************************/
void v9938_lmmv_engine()
{
	int DX=m_mmc.DX;
	int DY=m_mmc.DY;
	int TX=m_mmc.TX;
	int TY=m_mmc.TY;
	int NX=m_mmc.NX;
	int NY=m_mmc.NY;
	int ADX=m_mmc.ADX;
	int ANX=m_mmc.ANX;
	uint8_t CL=m_mmc.CL;
	uint8_t LO=m_mmc.LO;
	int MXD = m_mmc.MXD;
	int cnt;
	int delta;

	delta = v9938_get_vdp_timing_value(lmmv_timing);
	cnt = m_vdp_ops_count;

	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop v9938_VDPpset5(MXD, ADX, DY, CL, LO); post__x_y(256)
		break;
	case V9938_MODE_GRAPHIC5: pre_loop v9938_VDPpset6(MXD, ADX, DY, CL, LO); post__x_y(512)
		break;
	case V9938_MODE_GRAPHIC6: pre_loop v9938_VDPpset7(MXD, ADX, DY, CL, LO); post__x_y(512)
		break;
	case V9938_MODE_GRAPHIC7: pre_loop v9938_VDPpset8(MXD, ADX, DY, CL, LO); post__x_y(256)
		break;
	}

	if ((m_vdp_ops_count=cnt)>0) {
		// Command execution done
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		if (!NY)
			DY+=TY;
		m_cont_reg[38]=DY & 0xFF;
		m_cont_reg[39]=(DY>>8) & 0x03;
		m_cont_reg[42]=NY & 0xFF;
		m_cont_reg[43]=(NY>>8) & 0x03;
	}
	else {
		m_mmc.DY=DY;
		m_mmc.NY=NY;
		m_mmc.ANX=ANX;
		m_mmc.ADX=ADX;
	}
}

/** lmmm_engine() *********************************************/
/** Vram -> Vram                                            **/
/*************************************************************/
void v9938_lmmm_engine()
{
	int SX=m_mmc.SX;
	int SY=m_mmc.SY;
	int DX=m_mmc.DX;
	int DY=m_mmc.DY;
	int TX=m_mmc.TX;
	int TY=m_mmc.TY;
	int NX=m_mmc.NX;
	int NY=m_mmc.NY;
	int ASX=m_mmc.ASX;
	int ADX=m_mmc.ADX;
	int ANX=m_mmc.ANX;
	uint8_t LO=m_mmc.LO;
	int MXS = m_mmc.MXS;
	int MXD = m_mmc.MXD;
	int cnt;
	int delta;

	delta = v9938_get_vdp_timing_value(lmmm_timing);
	cnt = m_vdp_ops_count;

	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop v9938_VDPpset5(MXD, ADX, DY, v9938_VDPpoint5(MXS, ASX, SY), LO); post_xxyy(256)
		break;
	case V9938_MODE_GRAPHIC5: pre_loop v9938_VDPpset6(MXD, ADX, DY, v9938_VDPpoint6(MXS, ASX, SY), LO); post_xxyy(512)
		break;
	case V9938_MODE_GRAPHIC6: pre_loop v9938_VDPpset7(MXD, ADX, DY, v9938_VDPpoint7(MXS, ASX, SY), LO); post_xxyy(512)
		break;
	case V9938_MODE_GRAPHIC7: pre_loop v9938_VDPpset8(MXD, ADX, DY, v9938_VDPpoint8(MXS, ASX, SY), LO); post_xxyy(256)
		break;
	}

	if ((m_vdp_ops_count=cnt)>0) {
		// Command execution done
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		if (!NY) {
			SY+=TY;
			DY+=TY;
		}
		else
			if (SY==-1)
			DY+=TY;
		m_cont_reg[42]=NY & 0xFF;
		m_cont_reg[43]=(NY>>8) & 0x03;
		m_cont_reg[34]=SY & 0xFF;
		m_cont_reg[35]=(SY>>8) & 0x03;
		m_cont_reg[38]=DY & 0xFF;
		m_cont_reg[39]=(DY>>8) & 0x03;
	}
	else {
		m_mmc.SY=SY;
		m_mmc.DY=DY;
		m_mmc.NY=NY;
		m_mmc.ANX=ANX;
		m_mmc.ASX=ASX;
		m_mmc.ADX=ADX;
	}
}

/** lmcm_engine() *********************************************/
/** Vram -> CPU                                             **/
/*************************************************************/
void v9938_lmcm_engine()
{
	if ((m_stat_reg[2]&0x80)!=0x80) {
		//m_stat_reg[7]=m_cont_reg[44]=VDP_POINT(((m_mode >= 5) && (m_mode <= 8)) ? (m_mode-5) : 0, m_mmc.MXS, m_mmc.ASX, m_mmc.SY);
		uint8_t extended_mode = (m_mode < 0x1c) ? ((m_mode>>2)-3) : 3; // should be 0 to 3 for v9938 modes
		m_stat_reg[7]=m_cont_reg[44]=VDP_POINT(((m_mode >= 0x0c) && (m_mode <= 0x1c)) ? extended_mode : 0, m_mmc.MXS, m_mmc.ASX, m_mmc.SY);
		m_vdp_ops_count-=v9938_get_vdp_timing_value(lmmv_timing);
		m_stat_reg[2]|=0x80;

		if (!--m_mmc.ANX || ((m_mmc.ASX+=m_mmc.TX)&m_mmc.MX)) {
			if (!(--m_mmc.NY & 1023) || (m_mmc.SY+=m_mmc.TY)==-1) {
				m_stat_reg[2]&=0xFE;
				m_vdp_engine=nullptr;
				if (!m_mmc.NY)
					m_mmc.DY+=m_mmc.TY;
				m_cont_reg[42]=m_mmc.NY & 0xFF;
				m_cont_reg[43]=(m_mmc.NY>>8) & 0x03;
				m_cont_reg[34]=m_mmc.SY & 0xFF;
				m_cont_reg[35]=(m_mmc.SY>>8) & 0x03;
			}
			else {
				m_mmc.ASX=m_mmc.SX;
				m_mmc.ANX=m_mmc.NX;
			}
		}
	}
}

/** lmmc_engine() *********************************************/
/** CPU -> Vram                                             **/
/*************************************************************/
void v9938_lmmc_engine()
{
	if ((m_stat_reg[2]&0x80)!=0x80) {
		//uint8_t SM=((m_mode >= 5) && (m_mode <= 8)) ? (m_mode-5) : 0;
		uint8_t extended_mode = (m_mode < 0x1c) ? ((m_mode>>2)-3) : 3; // should be 0 to 3 for v9938 modes
		uint8_t SM=((m_mode >= 0x0c) && (m_mode <= 0x1c)) ? extended_mode : 0;

		m_stat_reg[7]=m_cont_reg[44]&=Mask[SM];
		VDP_PSET(SM, m_mmc.MXD, m_mmc.ADX, m_mmc.DY, m_cont_reg[44], m_mmc.LO);
		m_vdp_ops_count-=v9938_get_vdp_timing_value(lmmv_timing);
		m_stat_reg[2]|=0x80;

		if (!--m_mmc.ANX || ((m_mmc.ADX+=m_mmc.TX)&m_mmc.MX)) {
			if (!(--m_mmc.NY&1023) || (m_mmc.DY+=m_mmc.TY)==-1) {
				m_stat_reg[2]&=0xFE;
				m_vdp_engine=nullptr;
				if (!m_mmc.NY)
					m_mmc.DY+=m_mmc.TY;
				m_cont_reg[42]=m_mmc.NY & 0xFF;
				m_cont_reg[43]=(m_mmc.NY>>8) & 0x03;
				m_cont_reg[38]=m_mmc.DY & 0xFF;
				m_cont_reg[39]=(m_mmc.DY>>8) & 0x03;
			}
			else {
				m_mmc.ADX=m_mmc.DX;
				m_mmc.ANX=m_mmc.NX;
			}
		}
	}
}

/** hmmv_engine() *********************************************/
/** VDP --> Vram                                            **/
/*************************************************************/
void v9938_hmmv_engine()
{
	int DX=m_mmc.DX;
	int DY=m_mmc.DY;
	int TX=m_mmc.TX;
	int TY=m_mmc.TY;
	int NX=m_mmc.NX;
	int NY=m_mmc.NY;
	int ADX=m_mmc.ADX;
	int ANX=m_mmc.ANX;
	uint8_t CL=m_mmc.CL;
	int MXD = m_mmc.MXD;
	int cnt;
	int delta;

	delta = v9938_get_vdp_timing_value(hmmv_timing);
	cnt = m_vdp_ops_count;

	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop vram[VDP_VRMP5(MXD, ADX, DY)]= CL; post__x_y(256)
		break;
	case V9938_MODE_GRAPHIC5: pre_loop vram[VDP_VRMP6(MXD, ADX, DY)]= CL; post__x_y(512)
		break;
	case V9938_MODE_GRAPHIC6: pre_loop vram[VDP_VRMP7(MXD, ADX, DY)]= CL; post__x_y(512)
		break;
	case V9938_MODE_GRAPHIC7: pre_loop vram[VDP_VRMP8(MXD, ADX, DY)]= CL; post__x_y(256)
		break;
	}

	if ((m_vdp_ops_count=cnt)>0) {
		// Command execution done
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		if (!NY)
			DY+=TY;
		m_cont_reg[42]=NY & 0xFF;
		m_cont_reg[43]=(NY>>8) & 0x03;
		m_cont_reg[38]=DY & 0xFF;
		m_cont_reg[39]=(DY>>8) & 0x03;
	}
	else {
		m_mmc.DY=DY;
		m_mmc.NY=NY;
		m_mmc.ANX=ANX;
		m_mmc.ADX=ADX;
	}
}

/** hmmm_engine() *********************************************/
/** Vram -> Vram                                            **/
/*************************************************************/
void v9938_hmmm_engine()
{
	int SX=m_mmc.SX;
	int SY=m_mmc.SY;
	int DX=m_mmc.DX;
	int DY=m_mmc.DY;
	int TX=m_mmc.TX;
	int TY=m_mmc.TY;
	int NX=m_mmc.NX;
	int NY=m_mmc.NY;
	int ASX=m_mmc.ASX;
	int ADX=m_mmc.ADX;
	int ANX=m_mmc.ANX;
	int MXS = m_mmc.MXS;
	int MXD = m_mmc.MXD;
	int cnt;
	int delta;

	delta = v9938_get_vdp_timing_value(hmmm_timing);
	cnt = m_vdp_ops_count;

	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop vram[VDP_VRMP5(MXD, ADX, DY)]= vram[VDP_VRMP5(MXS, ASX, SY)]; post_xxyy(256)
		break;
	case V9938_MODE_GRAPHIC5: pre_loop vram[VDP_VRMP6(MXD, ADX, DY)]= vram[VDP_VRMP6(MXS, ASX, SY)]; post_xxyy(512)
		break;
	case V9938_MODE_GRAPHIC6: pre_loop vram[VDP_VRMP7(MXD, ADX, DY)]= vram[VDP_VRMP7(MXS, ASX, SY)]; post_xxyy(512)
		break;
	case V9938_MODE_GRAPHIC7: pre_loop vram[VDP_VRMP8(MXD, ADX, DY)]= vram[VDP_VRMP8(MXS, ASX, SY)]; post_xxyy(256)
		break;
	}

	if ((m_vdp_ops_count=cnt)>0) {
		// Command execution done
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		if (!NY) {
			SY+=TY;
			DY+=TY;
		}
		else
			if (SY==-1)
			DY+=TY;
		m_cont_reg[42]=NY & 0xFF;
		m_cont_reg[43]=(NY>>8) & 0x03;
		m_cont_reg[34]=SY & 0xFF;
		m_cont_reg[35]=(SY>>8) & 0x03;
		m_cont_reg[38]=DY & 0xFF;
		m_cont_reg[39]=(DY>>8) & 0x03;
	}
	else {
		m_mmc.SY=SY;
		m_mmc.DY=DY;
		m_mmc.NY=NY;
		m_mmc.ANX=ANX;
		m_mmc.ASX=ASX;
		m_mmc.ADX=ADX;
	}
}

/** ymmm_engine() *********************************************/
/** Vram -> Vram                                            **/
/*************************************************************/

void v9938_ymmm_engine()
{
	int SY=m_mmc.SY;
	int DX=m_mmc.DX;
	int DY=m_mmc.DY;
	int TX=m_mmc.TX;
	int TY=m_mmc.TY;
	int NY=m_mmc.NY;
	int ADX=m_mmc.ADX;
	int MXD = m_mmc.MXD;
	int cnt;
	int delta;

	delta = v9938_get_vdp_timing_value(ymmm_timing);
	cnt = m_vdp_ops_count;

	switch (m_mode) {
	default:
	case V9938_MODE_GRAPHIC4: pre_loop vram[VDP_VRMP5(MXD, ADX, DY)]=  vram[VDP_VRMP5(MXD, ADX, SY)]; post__xyy(256)
		break;
	case V9938_MODE_GRAPHIC5: pre_loop vram[VDP_VRMP6(MXD, ADX, DY)]= vram[VDP_VRMP6(MXD, ADX, SY)]; post__xyy(512)
		break;
	case V9938_MODE_GRAPHIC6: pre_loop vram[VDP_VRMP7(MXD, ADX, DY)]= vram[VDP_VRMP7(MXD, ADX, SY)]; post__xyy(512)
		break;
	case V9938_MODE_GRAPHIC7: pre_loop vram[VDP_VRMP8(MXD, ADX, DY)]= vram[VDP_VRMP8(MXD, ADX, SY)]; post__xyy(256)
		break;
	}

	if ((m_vdp_ops_count=cnt)>0) {
		// Command execution done
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		if (!NY) {
			SY+=TY;
			DY+=TY;
		}
		else
			if (SY==-1)
			DY+=TY;
		m_cont_reg[42]=NY & 0xFF;
		m_cont_reg[43]=(NY>>8) & 0x03;
		m_cont_reg[34]=SY & 0xFF;
		m_cont_reg[35]=(SY>>8) & 0x03;
		m_cont_reg[38]=DY & 0xFF;
		m_cont_reg[39]=(DY>>8) & 0x03;
	}
	else {
		m_mmc.SY=SY;
		m_mmc.DY=DY;
		m_mmc.NY=NY;
		m_mmc.ADX=ADX;
	}
}

/** hmmc_engine() *********************************************/
/** CPU -> Vram                                             **/
/*************************************************************/
void v9938_hmmc_engine()
{
	if ((m_stat_reg[2]&0x80)!=0x80) {
		//vram[VDP_VRMP(((m_mode >= 5) && (m_mode <= 8)) ? (m_mode-5) : 0, m_mmc.MXD, m_mmc.ADX, m_mmc.DY)]= m_cont_reg[44];
		uint8_t extended_mode = (m_mode < 0x1c) ? ((m_mode>>2)-3) : 3; // should be 0 to 3 for v9938 modes
		vram[VDP_VRMP(((m_mode >= 0x0c) && (m_mode <= 0x1c)) ? extended_mode : 0, m_mmc.MXD, m_mmc.ADX, m_mmc.DY)]= m_cont_reg[44];
		m_vdp_ops_count -= v9938_get_vdp_timing_value(hmmv_timing);
		m_stat_reg[2]|=0x80;

		if (!--m_mmc.ANX || ((m_mmc.ADX+=m_mmc.TX)&m_mmc.MX)) {
			if (!(--m_mmc.NY&1023) || (m_mmc.DY+=m_mmc.TY)==-1) {
				m_stat_reg[2]&=0xFE;
				m_vdp_engine=nullptr;
				if (!m_mmc.NY)
					m_mmc.DY+=m_mmc.TY;
				m_cont_reg[42]=m_mmc.NY & 0xFF;
				m_cont_reg[43]=(m_mmc.NY>>8) & 0x03;
				m_cont_reg[38]=m_mmc.DY & 0xFF;
				m_cont_reg[39]=(m_mmc.DY>>8) & 0x03;
			}
			else {
				m_mmc.ADX=m_mmc.DX;
				m_mmc.ANX=m_mmc.NX;
			}
		}
	}
}

/** VDPWrite() ***********************************************/
/** Use this function to transfer pixel(s) from CPU to m_ **/
/*************************************************************/
void v9938_cpu_to_vdp(uint8_t V)
{
	m_stat_reg[2]&=0x7F;
	m_stat_reg[7]=m_cont_reg[44]=V;
	if(m_vdp_engine&&(m_vdp_ops_count>0)) (*m_vdp_engine)();
}

/** VDPRead() ************************************************/
/** Use this function to transfer pixel(s) from VDP to CPU. **/
/*************************************************************/
uint8_t v9938_vdp_to_cpu()
{
	m_stat_reg[2]&=0x7F;
	if(m_vdp_engine&&(m_vdp_ops_count>0)) (*m_vdp_engine)();
	return(m_cont_reg[44]);
}

/** report_vdp_command() ***************************************/
/** Report VDP Command to be executed                       **/
/*************************************************************/
void v9938_report_vdp_command(uint8_t Op)
{
	static const char *const Ops[16] =
	{
		"SET ","AND ","OR  ","XOR ","NOT ","NOP ","NOP ","NOP ",
		"TSET","TAND","TOR ","TXOR","TNOT","NOP ","NOP ","NOP "
	};
	static const char *const Commands[16] =
	{
		" ABRT"," ????"," ????"," ????","POINT"," PSET"," SRCH"," LINE",
		" LMMV"," LMMM"," LMCM"," LMMC"," HMMV"," HMMM"," YMMM"," HMMC"
	};

	uint8_t CL, CM, LO;
	int SX,SY, DX,DY, NX,NY;

	// Fetch arguments
	CL = m_cont_reg[44];
	SX = (m_cont_reg[32]+((int)m_cont_reg[33]<<8)) & 511;
	SY = (m_cont_reg[34]+((int)m_cont_reg[35]<<8)) & 1023;
	DX = (m_cont_reg[36]+((int)m_cont_reg[37]<<8)) & 511;
	DY = (m_cont_reg[38]+((int)m_cont_reg[39]<<8)) & 1023;
	NX = (m_cont_reg[40]+((int)m_cont_reg[41]<<8)) & 1023;
	NY = (m_cont_reg[42]+((int)m_cont_reg[43]<<8)) & 1023;
	CM = Op>>4;
	LO = Op&0x0F;

}

/** VDPDraw() ************************************************/
/** Perform a given V9938 operation Op.                     **/
/*************************************************************/
uint8_t v9938_command_unit_w(uint8_t Op)
{
	int SM;

	// V9938 ops only work in SCREENs 5-8
	if (m_mode<0x0c)
		return(0);

	//SM = m_mode-5;         // Screen mode index 0..3
	SM = (m_mode < 0x1c) ? ((m_mode>>2)-3) : 3;         // Screen mode index 0..3

	m_mmc.CM = Op>>4;
	if ((m_mmc.CM & 0x0C) != 0x0C && m_mmc.CM != 0)
		// Dot operation: use only relevant bits of color
	m_stat_reg[7]=(m_cont_reg[44]&=Mask[SM]);

	//  if(Verbose&0x02)
	//v9938_report_vdp_command(Op);

	//if ((m_vdp_engine != nullptr) && (m_mmc.CM != CM_ABRT))
	//	LOGMASKED(LOG_WARN, "Command overrun; previous command not completed\n");

	switch(Op>>4) {
	case CM_ABRT:
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		return 1;
	case CM_POINT:
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		m_stat_reg[7]=m_cont_reg[44]=
		VDP_POINT(SM, (m_cont_reg[45] & 0x10) != 0,
			m_cont_reg[32]+((int)m_cont_reg[33]<<8),
			m_cont_reg[34]+((int)m_cont_reg[35]<<8));
		return 1;
	case CM_PSET:
		m_stat_reg[2]&=0xFE;
		m_vdp_engine=nullptr;
		VDP_PSET(SM, (m_cont_reg[45] & 0x20) != 0,
			m_cont_reg[36]+((int)m_cont_reg[37]<<8),
			m_cont_reg[38]+((int)m_cont_reg[39]<<8),
			m_cont_reg[44],
			Op&0x0F);
		return 1;
	case CM_SRCH:
		m_vdp_engine=&v9938_srch_engine;
		break;
	case CM_LINE:
		m_vdp_engine=&v9938_line_engine;
		break;
	case CM_LMMV:
		m_vdp_engine=&v9938_lmmv_engine;
		break;
	case CM_LMMM:
		m_vdp_engine=&v9938_lmmm_engine;
		break;
	case CM_LMCM:
		m_vdp_engine=&v9938_lmcm_engine;
		break;
	case CM_LMMC:
		m_vdp_engine=&v9938_lmmc_engine;
		break;
	case CM_HMMV:
		m_vdp_engine=&v9938_hmmv_engine;
		break;
	case CM_HMMM:
		m_vdp_engine=&v9938_hmmm_engine;
		break;
	case CM_YMMM:
		m_vdp_engine=&v9938_ymmm_engine;
		break;
	case CM_HMMC:
		m_vdp_engine=&v9938_hmmc_engine;
		break;
	default:
		//LOGMASKED(LOG_WARN, "Unrecognized opcode %02Xh\n",Op);
		return(0);
	}

	// Fetch unconditional arguments
	m_mmc.SX = (m_cont_reg[32]+((int)m_cont_reg[33]<<8)) & 511;
	m_mmc.SY = (m_cont_reg[34]+((int)m_cont_reg[35]<<8)) & 1023;
	m_mmc.DX = (m_cont_reg[36]+((int)m_cont_reg[37]<<8)) & 511;
	m_mmc.DY = (m_cont_reg[38]+((int)m_cont_reg[39]<<8)) & 1023;
	m_mmc.NY = (m_cont_reg[42]+((int)m_cont_reg[43]<<8)) & 1023;
	m_mmc.TY = m_cont_reg[45]&0x08? -1:1;
	m_mmc.MX = PPL[SM];
	m_mmc.CL = m_cont_reg[44];
	m_mmc.LO = Op&0x0F;
	m_mmc.MXS = (m_cont_reg[45] & 0x10) != 0;
	m_mmc.MXD = (m_cont_reg[45] & 0x20) != 0;

	// Argument depends on uint8_t or dot operation
	if ((m_mmc.CM & 0x0C) == 0x0C) {
		m_mmc.TX = m_cont_reg[45]&0x04? -PPB[SM]:PPB[SM];
		m_mmc.NX = ((m_cont_reg[40]+((int)m_cont_reg[41]<<8)) & 1023)/PPB[SM];
	}
	else {
		m_mmc.TX = m_cont_reg[45]&0x04? -1:1;
		m_mmc.NX = (m_cont_reg[40]+((int)m_cont_reg[41]<<8)) & 1023;
	}

	// X loop variables are treated specially for LINE command
	if (m_mmc.CM == CM_LINE) {
		m_mmc.ASX=((m_mmc.NX-1)>>1);
		m_mmc.ADX=0;
	}
	else {
		m_mmc.ASX = m_mmc.SX;
		m_mmc.ADX = m_mmc.DX;
	}

	// NX loop variable is treated specially for SRCH command
	if (m_mmc.CM == CM_SRCH)
		m_mmc.ANX=(m_cont_reg[45]&0x02)!=0; // Do we look for "==" or "!="?
	else
		m_mmc.ANX = m_mmc.NX;

	// Command execution started
	m_stat_reg[2]|=0x01;

	// Start execution if we still have time slices
	if(m_vdp_engine&&(m_vdp_ops_count>0)) (*m_vdp_engine)();

	// Operation successfully initiated
	return(1);
}

/** LoopVDP() ************************************************
Run X steps of active VDP command
*************************************************************/
void v9938_update_command()
{
	if(m_vdp_ops_count<=0)
	{
		m_vdp_ops_count+=13662;
		if(m_vdp_engine&&(m_vdp_ops_count>0)) (*m_vdp_engine)();
	}
	else
	{
		m_vdp_ops_count=13662;
		if(m_vdp_engine) (*m_vdp_engine)();
	}
}

void v9938_device_post_load() // TODO: is there a better way to restore this?
{
	switch(m_mmc.CM)
	{
	case CM_ABRT:
	case CM_POINT:
	case CM_PSET:
		m_vdp_engine=nullptr;
		break;
	case CM_SRCH:
		m_vdp_engine=&v9938_srch_engine;
		break;
	case CM_LINE:
		m_vdp_engine=&v9938_line_engine;
		break;
	case CM_LMMV:
		m_vdp_engine=&v9938_lmmv_engine;
		break;
	case CM_LMMM:
		m_vdp_engine=&v9938_lmmm_engine;
		break;
	case CM_LMCM:
		m_vdp_engine=&v9938_lmcm_engine;
		break;
	case CM_LMMC:
		m_vdp_engine=&v9938_lmmc_engine;
		break;
	case CM_HMMV:
		m_vdp_engine=&v9938_hmmv_engine;
		break;
	case CM_HMMM:
		m_vdp_engine=&v9938_hmmm_engine;
		break;
	case CM_YMMM:
		m_vdp_engine=&v9938_ymmm_engine;
		break;
	case CM_HMMC:
		m_vdp_engine=&v9938_hmmc_engine;
		break;
	}
}
