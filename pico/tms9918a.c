/*
 *  render related stuff for TMS9918A
 *
 *  Based on the videx code in AppleIIVGA
 *  work in the rendering pipeline
 *
 *  Based heavily on the MAME tms9928a.cpp code
 *
 *  // license:BSD-3-Clause
// copyright-holders:Sean Young, Nathan Woods, Aaron Giles, Wilbert Pol, hap
/*
** File: tms9928a.c -- software implementation of the Texas Instruments
**                     TMS9918(A), TMS9928(A) and TMS9929(A), used by the Coleco, MSX and
**                     TI99/4(A).
**
** All undocumented features as described in the following file
** should be emulated.
**
** http://bifi.msxnet.org/msxnet/tech/tms9918a.txt
**
** By Sean Young 1999 (sean@msxnet.org).
** Based on code by Mike Balfour.
** Improved over the years by MESS and MAME teams.
**
** Todo:
** - External VDP input and sync (pin 34/35 on 9918A)
** - Updates during mid-scanline, probably only used in some MSX1 demos
** - Colours are incorrect. [fixed by R Nabet ?]
** - Sprites 8-31 are ghosted/cloned in mode 3 when using less than
**   three pattern tables. Exact behaviour is not known.
** - Address scrambling when setting TMS99xxA to 4K (not on TMS91xx)

 */
#include "tms9918a.h"

#include <pico/stdlib.h>
#include "buffers.h"
#include "colors.h"
#include "vga.h"


static volatile uint8_t   m_Regs[8];
#define VRAM_SIZE 0x4000
static volatile uint8_t vram[VRAM_SIZE];
static volatile uint8_t m_ReadAhead;
static volatile uint16_t m_Addr;
static volatile uint8_t m_latch;
static volatile uint8_t m_mode;
static volatile uint8_t m_StatusReg;
static volatile uint8_t m_FifthSprite;
static volatile uint8_t m_INT;
static volatile uint16_t m_colour;
static volatile uint16_t m_pattern;
static volatile uint16_t m_nametbl;
static volatile uint16_t m_spriteattribute;
static volatile uint16_t m_spritepattern;
int     m_colourmask;
int     m_patternmask;
const bool m_99 = true;
const bool m_reva = true;

void tms9918a_init() {
	m_StatusReg = 0;
	m_FifthSprite = 31;
	m_nametbl = 0;
	m_pattern = 0;
	m_colour = 0;
	m_spritepattern = 0;
	m_spriteattribute = 0;
	m_colourmask = 0x3fff;
	m_patternmask = 0x3fff;

	m_ReadAhead = 0;
	m_Addr = 0;
	m_latch = 0;
	m_mode = 0;

	for (uint16_t x = 0;x<VRAM_SIZE;x++) {
		vram[x] = 0;
	}
}


uint8_t tms9918a_read(uint_fast16_t address)
{
	uint8_t value = 0;

	if ((address & 1) == 0)
		value = tms9918a_vram_read();
	else
		value = tms9918a_register_read();

	return value;
}

void tms9918a_write(uint_fast16_t address, uint8_t data)
{
	if ((address & 1) == 0)
		tms9918a_vram_write(data);
	else
		tms9918a_register_write(data);
}

uint8_t tms9918a_vram_read()
{
	uint8_t data = m_ReadAhead;

	m_ReadAhead = vram[m_Addr];
	m_Addr = (m_Addr + 1) & (VRAM_SIZE - 1);
	m_latch = 0;

	return data;
}

void tms9918a_vram_write(uint8_t data)
{
	vram[m_Addr] = data;

	m_Addr = (m_Addr + 1) & (VRAM_SIZE - 1);
	m_ReadAhead = data;
	m_latch = 0;
}

void tms9918a_register_write(uint8_t data)
{
	if (m_latch)
	{
		/* set high part of read/write address */
		m_Addr = ((data << 8) | (m_Addr & 0xff)) & (VRAM_SIZE -1);

		if (data & 0x80)
		{
			/* register write */
			tms9918a_change_register (data & 7, m_Addr & 0xff);
		}
		else
		{
			if ( !(data & 0x40) )
			{
				/* read ahead */
				tms9918a_vram_read();
			}
		}
		m_latch = 0;
	}
	else
	{
		/* set low part of read/write address */
		m_Addr = ((m_Addr & 0xff00) | data) & (VRAM_SIZE - 1);
		m_latch = 1;
	}
}

uint8_t tms9918a_register_read()
{
	uint8_t data = m_StatusReg;

	m_StatusReg = m_FifthSprite;
	//check_interrupt();
	m_latch = 0;

	return data;
}

void tms9918a_update_backdrop()
{
	// update backdrop colour to transparent if EXTVID bit is set
	//if ((m_Regs[7] & 15) == 0)
	//      set_pen_color(0, rgb_t(m_Regs[0] & 1 ? 0 : 255,0,0,0));

}


void tms9918a_update_table_masks()
{
	m_colourmask = ( (m_Regs[3] & 0x7f) << 3 ) | 7;

	// on 91xx family, the colour table mask doesn't affect the pattern table mask
	m_patternmask = ( (m_Regs[4] & 3) << 8 ) | ( m_99 ? (m_colourmask & 0xff) : 0xff );
}

void tms9918a_change_register(uint8_t reg, uint8_t val)
{
	static const uint8_t Mask[8] =
		{ 0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff };
	static const char *const modes[] =
	{
		"Mode 0 (GRAPHIC 1)", "Mode 1 (TEXT 1)", "Mode 2 (GRAPHIC 2)",
		"Mode 1+2 (TEXT 1 variation)", "Mode 3 (MULTICOLOR)",
		"Mode 1+3 (BOGUS)", "Mode 2+3 (MULTICOLOR variation)",
		"Mode 1+2+3 (BOGUS)"
	};

	uint8_t prev = m_Regs[reg];
	val &= Mask[reg];
	m_Regs[reg] = val;

	switch (reg)
	{
	case 0:
		/* re-calculate masks and pattern generator & colour */
		if (val & 2)
		{
				m_colour = ((m_Regs[3] & 0x80) * 64) & (VRAM_SIZE - 1);
				m_pattern = ((m_Regs[4] & 4) * 2048) & (VRAM_SIZE - 1);
				tms9918a_update_table_masks();
		}
		else
		{
				m_colour = (m_Regs[3] * 64) & (VRAM_SIZE - 1);
				m_pattern = (m_Regs[4] * 2048) & (VRAM_SIZE - 1);
		}
		m_mode = ( (m_reva ? (m_Regs[0] & 2) : 0) | ((m_Regs[1] & 0x10)>>4) | ((m_Regs[1] & 8)>>1));
		if ((val ^ prev) & 1)
				tms9918a_update_backdrop();
		break;
	case 1:
		//check_interrupt();
		m_mode = ( (m_reva ? (m_Regs[0] & 2) : 0) | ((m_Regs[1] & 0x10)>>4) | ((m_Regs[1] & 8)>>1));
		break;
	case 2:
		m_nametbl = (val * 1024) & (VRAM_SIZE - 1);
		break;
	case 3:
		if (m_Regs[0] & 2)
		{
			m_colour = ((val & 0x80) * 64) & (VRAM_SIZE - 1);
			tms9918a_update_table_masks();
		}
		else
		{
			m_colour = (val * 64) & (VRAM_SIZE - 1);
		}
		break;
	case 4:
		if (m_Regs[0] & 2)
		{
			m_pattern = ((val & 4) * 2048) & (VRAM_SIZE - 1);
			tms9918a_update_table_masks();
		}
		else
		{
			m_pattern = (val * 2048) & (VRAM_SIZE - 1);
		}
		break;
	case 5:
		m_spriteattribute = (val * 128) & (VRAM_SIZE - 1);
		break;
	case 6:
		m_spritepattern = (val * 2048) & (VRAM_SIZE - 1);
		break;
	case 7:
		if ((val ^ prev) & 15)
			tms9918a_update_backdrop();
		break;
	}
}


static void render_tms9918a_line(unsigned int line) {

	uint16_t BackColour = m_Regs[7] & 15; 

	uint sl_pos,sl_pos_backup, sl_pos_main_pixels_start;
	struct vga_scanline *sl = vga_prepare_scanline();
	sl_pos = 0;
	sl_pos_backup =sl_pos;
	uint32_t bg_color;
	uint32_t fg_color;

	// Pad 40 pixels on the left to center horizontally
	sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
	sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
	sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
	sl->data[sl_pos++] = (0 | THEN_EXTEND_3) | ((0 | THEN_EXTEND_3) << 16);  // 8 pixels

	// TODO You probably need to rewrite this so that all pixesl are just THEN_EXTEND_1 ones. If you dont do that the chunky mode
	// wont work right
	sl_pos_main_pixels_start = sl_pos;

	switch( m_mode )
	{
		case 0:             /* MODE 0 */
			// if (vpos==100 ) popmessage("TMS9928A MODE 0");
			{
				uint16_t addr = m_nametbl + ( ( line & 0xF8 ) << 2 );

				for ( int x = 0; x < 256; x+= 8, addr++ )
				{
						uint8_t charcode = vram[addr];
						uint8_t pattern =  vram[ m_pattern + ( charcode << 3 ) + ( line & 7 ) ];
						uint8_t colour =  vram[m_colour + ( charcode >> 3 ) ];
						fg_color = tms9918a_palette[(colour >> 4) ? (colour >> 4) : BackColour];
						bg_color = tms9918a_palette[(colour & 15) ? (colour & 15) : BackColour];
						uint32_t bits_to_pixelpair[4] = {
							(bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
							(fg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
							(bg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
							(fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
						};

						for(int i = 0; i < 4; i++) {
							sl->data[sl_pos] = bits_to_pixelpair[(pattern >> 6) & 0x03];
							sl_pos++;
							pattern <<= 2;
						}
				}
			}
			break;

		case 1:             /* MODE 1 */
			//if (vpos==100 ) popmessage("TMS9928A MODE 1");
			{
				uint16_t addr = m_nametbl + ( ( line >> 3 ) * 40 );
				fg_color = tms9918a_palette[(m_Regs[7] >> 4) ? (m_Regs[7] >> 4) : BackColour];
				bg_color = tms9918a_palette[BackColour];
				uint32_t bits_to_pixelpair[4] = {
					(bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
					(fg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
					(bg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
					(fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
				};

				// Supposedly some extra blank indentation first in text mode
				for ( int x = 0; x < 240; x+= 6, addr++ )
				{
						uint8_t charcode = vram[addr];
						uint8_t pattern =  vram[ m_pattern + ( charcode << 3 ) + ( line & 7 ) ];

						for(int i = 0; i < 3; i++) {
							sl->data[sl_pos] = bits_to_pixelpair[(pattern >> 6) & 0x03];
							sl_pos++;
							pattern <<= 2;
						}
				}
			}
			break;

		case 2:             /* MODE 2 */
				//if (vpos==100 ) popmessage("TMS9928A MODE 2");
			{
				uint16_t addr = m_nametbl + ( ( line >> 3 ) * 32 );

				for ( int x = 0; x < 256; x+= 8, addr++ )
				{
						uint16_t charcode = vram[addr] + ( ( line >> 6 ) << 8 );
						uint8_t pattern =  vram[ m_pattern + ( ( charcode & m_patternmask ) << 3 ) + ( line & 7 ) ];
						uint8_t colour =  vram[m_colour + ( ( charcode & m_colourmask ) << 3 ) + ( line & 7 ) ];
						fg_color = tms9918a_palette[(colour >> 4) ? (colour >> 4) : BackColour];
						bg_color = tms9918a_palette[(colour & 15) ? (colour & 15) : BackColour];
						uint32_t bits_to_pixelpair[4] = {
							(bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
							(fg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
							(bg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
							(fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
						};

						for(int i = 0; i < 4; i++) {
							sl->data[sl_pos] = bits_to_pixelpair[(pattern >> 6) & 0x03];
							sl_pos++;
							pattern <<= 2;
						}
				}
			}
			break;

		case 3:             /* MODE 1+2 */
				//if (vpos==100) popmessage("TMS9928A MODE1+2");
			{
				uint16_t addr = m_nametbl + ( ( line >> 3 ) * 40 );
				fg_color = tms9918a_palette[(m_Regs[7] >> 4) ? (m_Regs[7] >> 4) : BackColour];
				bg_color = tms9918a_palette[BackColour];
				uint32_t bits_to_pixelpair[4] = {
					(bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
					(fg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
					(bg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
					(fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
				};

				// Supposedly some extra blank indentation first in text mode
				for ( int x = 0; x < 240; x+= 6, addr++ )
				{
						uint16_t charcode = ( vram[addr] + ( ( line >> 6 ) << 8 ) ) & m_patternmask;
						uint8_t pattern =  vram[ m_pattern + ( charcode << 3 ) + ( line & 7 ) ];

						for(int i = 0; i < 3; i++) {
							sl->data[sl_pos] = bits_to_pixelpair[(pattern >> 6) & 0x03];
							sl_pos++;
							pattern <<= 2;
						}
				}
			}
			break;



		case 4:             /* MODE 3 */
				//if (vpos==100 ) popmessage("TMS9928A MODE 3");
			{
				uint16_t addr = m_nametbl + ( ( line >> 3 ) * 32 );

				for ( int x = 0; x < 256; x+= 8, addr++ )
				{
					uint8_t charcode = vram[addr];
					
					uint8_t colour =  vram[m_pattern + ( charcode << 3 ) + ( ( line >> 2 ) & 7 ) ];
					fg_color = tms9918a_palette[(colour >> 4) ? (colour >> 4) : BackColour];
					bg_color = tms9918a_palette[(colour & 15) ? (colour & 15) : BackColour];
					sl->data[sl_pos++] = (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1;

				}
			}
			break;



		case 5: case 7:     /* MODE bogus */
				//if (vpos==100 ) popmessage("TMS9928A MODE bogus");
			{
				fg_color = tms9918a_palette[(m_Regs[7] >> 4) ? (m_Regs[7] >> 4) : BackColour];
				bg_color = tms9918a_palette[BackColour];


				for ( int x = 0; x < 240; x+= 6 )
				{
					sl->data[sl_pos++] = (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1;

				}


			}
			break;

		case 6:             /* MODE 2+3 */
				//if (vpos==100 ) popmessage("TMS9928A MODE 2+3");
			{
				uint16_t addr = m_nametbl + ( ( line >> 3 ) * 32 );

				for ( int x = 0; x < 256; x+= 8, addr++ )
				{
					uint8_t charcode = vram[addr];
					uint8_t colour =  vram[m_pattern + ( ( ( charcode + ( ( line >> 2 ) & 7 ) + ( ( line >> 6 ) << 8 ) ) & m_patternmask ) << 3 ) ];
					fg_color = tms9918a_palette[(colour >> 4) ? (colour >> 4) : BackColour];
					bg_color = tms9918a_palette[(colour & 15) ? (colour & 15) : BackColour];
					sl->data[sl_pos++] = (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1;
					sl->data[sl_pos++] = (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1;

				}
			}
			break;

	}

	sl_pos_backup = sl_pos;

	sl_pos = sl_pos_main_pixels_start;

	// TODO: The sprite stuff needs to overwrite what is in sl->data
	/* Draw sprites */
	if ( ( m_Regs[1] & 0x50 ) != 0x40 )
	{
			/* sprites are disabled */
			m_FifthSprite = 31;
	}
	else
	{
			uint8_t sprite_size = ( m_Regs[1] & 0x02 ) ? 16 : 8;
			uint8_t sprite_mag = m_Regs[1] & 0x01;
			uint8_t sprite_height = sprite_size * ( sprite_mag + 1 );
			uint8_t spr_drawn[32+256+32] = { 0 };
			uint8_t num_sprites = 0;
			bool fifth_encountered = false;

			for ( uint16_t sprattr = 0; sprattr < 128; sprattr += 4 )
			{
					int spr_y =  vram[ m_spriteattribute + sprattr + 0 ];

					m_FifthSprite = sprattr / 4;

					/* Stop processing sprites */
					if ( spr_y == 208 )
							break;

					if ( spr_y > 0xE0 )
							spr_y -= 256;

					/* vert pos 255 is displayed on the first line of the screen */
					spr_y++;

					/* is sprite enabled on this line? */
					if ( spr_y <= line && line < spr_y + sprite_height )
					{
							int spr_x =  vram[ m_spriteattribute + sprattr + 1 ];
							uint8_t sprcode =  vram[ m_spriteattribute + sprattr + 2 ];
							uint8_t sprcol =  vram[ m_spriteattribute + sprattr + 3 ];
							uint16_t pataddr = m_spritepattern + ( ( sprite_size == 16 ) ? sprcode & ~0x03 : sprcode ) * 8;

							num_sprites++;

							/* Fifth sprite encountered? */
							if ( num_sprites == 5 )
							{
									fifth_encountered = true;
									break;
							}

							if ( sprite_mag )
									pataddr += ( ( ( line - spr_y ) & 0x1F ) >> 1 );
							else
									pataddr += ( ( line - spr_y ) & 0x0F );

							uint8_t pattern =  vram[ pataddr ];

							if ( sprcol & 0x80 )
									spr_x -= 32;

							sprcol &= 0x0f;

							for ( int s = 0; s < sprite_size; s += 8 )
							{
									for ( int i = 0; i < 8; pattern <<= 1, i++ )
									{
											int colission_index = spr_x + ( sprite_mag ? i * 2 : i ) + 32;

											for ( int z = 0; z <= sprite_mag; colission_index++, z++ )
											{
													/* Check if pixel should be drawn */
													if ( pattern & 0x80 )
													{
															if ( colission_index >= 32 && colission_index < 32 + 256 )
															{
																	/* Check for colission */
																	if ( spr_drawn[ colission_index ] )
																			m_StatusReg |= 0x20;
																	spr_drawn[ colission_index ] |= 0x01;

																	if ( sprcol )
																	{
																			/* Has another sprite already drawn here? */
																			if ( ! ( spr_drawn[ colission_index ] & 0x02 ) )
																			{
																					spr_drawn[ colission_index ] |= 0x02;
																					// actually draw a pixel
																					//p[ HORZ_DISPLAY_START + colission_index - 32 ] = pen(sprcol);
																					if ( (colission_index - 32) & 1) {
																						// odd x coord
																						uint32_t last_pixel_pair = sl->data[sl_pos +  ((colission_index - 32) >>1)];
																						// update the upper 16 bits (which is the right most of a pixel pair)
																						sl->data[sl_pos+((colission_index - 32) >>1)] = (last_pixel_pair & 0xffff) | ((tms9918a_palette[sprcol] | THEN_EXTEND_1) <<16);

																					} else {
																						// even x coord
																						uint32_t last_pixel_pair = sl->data[sl_pos+((colission_index - 32) >>1)];
																						// update the lower 16 bits (which is the left most of a pixel pair)
																						sl->data[sl_pos+((colission_index - 32) >>1)] = (last_pixel_pair & 0xffff0000) | tms9918a_palette[sprcol] | THEN_EXTEND_1;
																					}
																			}
																	}
															}
													}
											}
									}

									pattern =  vram[ pataddr + 16 ];
									spr_x += sprite_mag ? 16 : 8;
							}
					}
			}

			/* Update sprite overflow bits */
			if (~m_StatusReg & 0x40)
			{
					m_StatusReg = (m_StatusReg & 0xe0) | m_FifthSprite;
					if (fifth_encountered && ~m_StatusReg & 0x80)
							m_StatusReg |= 0x40;
			}
	}
	sl->repeat_count = 1;
	// Use the backup (thought possibly not required)
	sl->length = sl_pos_backup;
	vga_submit_scanline(sl);


}



										 




// Only called from the render core

void render_tms9918a() {
	vga_prepare_frame();
	// Skip 48 lines to center vertically
	vga_skip_lines(84);

	for(int line = 0; line < 192; line++) {
		render_tms9918a_line(line);
	}
}

