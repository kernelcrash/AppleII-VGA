/*
 *  render related stuff for MC6847/MC6883 as found in Coco1/2 and Dragon32/64
 *
 *  Based on the videx code in AppleIIVGA
 *  and much of the code comes from https://github.com/eyalabraham/Dragon32-RPi-Bare-Metal modified to
 *  work in the rendering pipeline
 *
 *  Artifact mode is a 'work in progress' and generally not very exact.
 *
 */
#include "mc6847.h"

#include <pico/stdlib.h>
#include "buffers.h"
#include "colors.h"
#include "textfont/textfont.h"
#include "vga.h"


volatile bool mc6847_enabled;
volatile bool mc6847_mem_selected;

static volatile uint16_t mc6847_sam_vdg_mode;
static volatile uint16_t mc6847_sam_vdg_display_offset;
static volatile uint8_t mc6847_sam_vdg_page;
static volatile uint8_t mc6847_sam_vdg_map_type;

static volatile uint8_t pia_video_mode;

typedef enum
{                       // Colors   Res.     Bytes BASIC
    ALPHA_INTERNAL = 0, // 2 color  32x16    512   Default
    ALPHA_EXTERNAL,     // 4 color  32x16    512
    SEMI_GRAPHICS_4,    // 8 color  64x32    512
    SEMI_GRAPHICS_6,    // 8 color  64x48    512
    SEMI_GRAPHICS_8,    // 8 color  64x64   2048
    SEMI_GRAPHICS_12,   // 8 color  64x96   3072
    SEMI_GRAPHICS_24,   // 8 color  64x192  6144
    GRAPHICS_1C,        // 4 color  64x64   1024
    GRAPHICS_1R,        // 2 color  128x64  1024
    GRAPHICS_2C,        // 4 color  128x64  1536
    GRAPHICS_2R,        // 2 color  128x96  1536   PMODE0
    GRAPHICS_3C,        // 4 color  128x96  3072   PMODE1
    GRAPHICS_3R,        // 2 color  128x192 3072   PMODE2
    GRAPHICS_6C,        // 4 color  128x192 6144   PMODE3
    GRAPHICS_6R,        // 2 color  256x192 6144   PMODE4
    DMA,                // 2 color  256x192 6144
    UNDEFINED,          // Undefined
} video_mode_t;

static video_mode_t mc6847_current_mode;

// Initialize VideoTerm state
//
// Only called from the abus core
void mc6847_init() {
    mc6847_sam_vdg_mode = 0;    // Alphanumeric
    mc6847_sam_vdg_display_offset = 2;  // text page 0x400
    mc6847_sam_vdg_page = 0;
    mc6847_sam_vdg_map_type = 0;
    
    mc6847_current_mode = ALPHA_INTERNAL;
}


/*------------------------------------------------
 * PIA video modes look like this
 *
 *  Set the video display mode from PIA device.
 *  Mode bit are as-is for PIA shifted 3 to the right:
 *  Bit 4   O   Screen Mode G / ^A
 *  Bit 3   O   Screen Mode GM2
 *  Bit 2   O   Screen Mode GM1
 *  Bit 1   O   Screen Mode GM0 / ^INT
 *  Bit 0   O   Screen Mode CSS
 *
 
 */
void mc6847_set_pia_mode(uint8_t pia_mode)
{
    pia_video_mode = pia_mode;
}

void mc6847_set_page(uint8_t page) {
    mc6847_sam_vdg_page = page;
}

void mc6847_set_map_type(uint8_t map_type) {
    mc6847_sam_vdg_map_type = map_type;
}

uint8_t mc6847_get_map_type() {
    return mc6847_sam_vdg_map_type;
}

uint8_t mc6847_get_page() {
    return mc6847_sam_vdg_page;
}
/*------------------------------------------------
 * vdg_get_mode()
 *
 * Parse 'mc6847_sam_vdg_mode' and 'pia_video_mode' and return video mode type.
 *
 * param:  None
 * return: Video mode
 *
 */
static video_mode_t vdg_get_mode(void)
{
    video_mode_t mode = UNDEFINED;

    if ( mc6847_sam_vdg_mode == 7 ) {
        mode = DMA;
    } else if ( (pia_video_mode & 0x10) ) {        
        switch ( pia_video_mode & 0x0e  )
        {
            case 0x00:
                mode = GRAPHICS_1C;
                break;
            case 0x02:
                mode = GRAPHICS_1R;
                break;
            case 0x04:
                mode = GRAPHICS_2C;
                break;
            case 0x06:
                mode = GRAPHICS_2R;
                break;
            case 0x08:
                mode = GRAPHICS_3C;
                break;
            case 0x0a:
                mode = GRAPHICS_3R;
                break;
            case 0x0c:
                mode = GRAPHICS_6C;
                break;
            case 0x0e:
                mode = GRAPHICS_6R;
                break;
        }
    }
    else if ( (pia_video_mode & 0x10) == 0 )
    {
        if ( mc6847_sam_vdg_mode == 0 &&
             (pia_video_mode & 0x02) == 0 )
        {
            mode = ALPHA_INTERNAL;
            // Character bit.7 selects SEMI_GRAPHICS_4;
        }
        else if ( mc6847_sam_vdg_mode == 0 &&
                (pia_video_mode & 0x02) )
        {
            mode = ALPHA_EXTERNAL;
            // Character bit.7 selects SEMI_GRAPHICS_6;
        }
        else if ( mc6847_sam_vdg_mode == 2 &&
                (pia_video_mode & 0x02) == 0 )
        {
            mode = SEMI_GRAPHICS_8;
        }
        else if ( mc6847_sam_vdg_mode == 4 &&
                (pia_video_mode & 0x02) == 0 )
        {
            mode = SEMI_GRAPHICS_12;
        }
        else if ( mc6847_sam_vdg_mode == 4 &&
                (pia_video_mode & 0x02) == 0 )
        {
            mode = SEMI_GRAPHICS_24;
        }
    }

    return mode;
}


// Enable the VideoTerm support
//
// Only called from the abus core
void mc6847_enable() {
    mc6847_enabled = true;
}


// Disable the VideoTerm support
//
// Only called from the abus core
void mc6847_disable() {
    mc6847_enabled = false;
}




void mc6847_sam_write(uint_fast16_t address) {
    switch ( address & 0x1f ) {
        /* VDG mode
            */
        case 0x00:
            mc6847_sam_vdg_mode &= 0xfe;
            break;

        case 0x01:
            mc6847_sam_vdg_mode |= 0x01;
            break;

        case 0x02:
            mc6847_sam_vdg_mode &= 0xfd;
            break;

        case 0x03:
            mc6847_sam_vdg_mode |= 0x02;
            break;

        case 0x04:
            mc6847_sam_vdg_mode &= 0xfb;
            break;

        case 0x05:
            mc6847_sam_vdg_mode |= 0x04;
            break;

        /* Display offset
            */
        case 0x06:
            mc6847_sam_vdg_display_offset &= 0xfe;
            break;

        case 0x07:
            mc6847_sam_vdg_display_offset |= 0x01;
            break;

        case 0x08:
            mc6847_sam_vdg_display_offset &= 0xfd;
            break;

        case 0x09:
            mc6847_sam_vdg_display_offset |= 0x02;
            break;

        case 0x0a:
            mc6847_sam_vdg_display_offset &= 0xfb;
            break;

        case 0x0b:
            mc6847_sam_vdg_display_offset |= 0x04;
            break;

        case 0x0c:
            mc6847_sam_vdg_display_offset &= 0xf7;
            break;

        case 0x0d:
            mc6847_sam_vdg_display_offset |= 0x08;
            break;

        case 0x0e:
            mc6847_sam_vdg_display_offset &= 0xef;
            break;

        case 0x0f:
            mc6847_sam_vdg_display_offset |= 0x10;
            break;

        case 0x10:
            mc6847_sam_vdg_display_offset &= 0xdf;
            break;

        case 0x11:
            mc6847_sam_vdg_display_offset |= 0x20;
            break;

        case 0x12:
            mc6847_sam_vdg_display_offset &= 0xbf;
            break;

        case 0x13:
            mc6847_sam_vdg_display_offset |= 0x40;
            break;

        case 0x14:
            mc6847_sam_vdg_page &= 0xfe;
            break;

        case 0x15:
            mc6847_sam_vdg_page |= 0x01;
            break;

        case 0x1e:
            mc6847_sam_vdg_map_type &= 0xfe;
            break;

        case 0x1f:
            mc6847_sam_vdg_map_type |= 0x01;
            break;

    }

}



// Render a single scan line (well, actually two that are the same). line will be 0 to 191
static void render_mc6847_line(unsigned int line, uint base_addr, video_mode_t vdg_mode) {
    uint mem_offset_of_line;
    uint glyph_line;
    uint sl_pos;
    uint32_t bg_color;
    uint32_t fg_color;
    uint8_t colorset;


    struct vga_scanline *sl = vga_prepare_scanline();
    sl_pos = 0;
    // Pad 40 pixels on the left to center horizontally
    sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (0 | THEN_EXTEND_7) | ((0 | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (0 | THEN_EXTEND_3) | ((0 | THEN_EXTEND_3) << 16);  // 8 pixels
    
    switch (vdg_mode) {
        case ALPHA_INTERNAL:
            mem_offset_of_line = base_addr + ((line/12) * 32);
            glyph_line = line % 12;

            for(uint col = 0; col < 32; col++) {
                uint8_t m = main_memory[mem_offset_of_line + col];
                if (m & 0x80) {
                    // semi graphics 4
                    fg_color = mc6847_palette[(m & 0b01110000) >> 4];
                    bg_color = mc6847_palette[MC6847_COLOR_BLACK];
                    uint32_t bits_to_pixelpair[4] = {
                        (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
                        (fg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
                        (bg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
                        (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
                    };
                    uint_fast16_t bits = mc6847_semi_graph_4[(m & 0x0f) * 12 + glyph_line];
                    for(int i = 0; i < 4; i++) {
                        sl->data[sl_pos] = bits_to_pixelpair[(bits >> 6) & 0x03];
                        sl_pos++;
                        bits <<= 2;
                    }

                } else {
                    fg_color = mc6847_palette[MC6847_COLOR_ALPHANUMERIC_BRIGHT_GREEN];
                    bg_color = mc6847_palette[MC6847_COLOR_ALPHANUMERIC_DARK_GREEN];

                    uint32_t bits_to_pixelpair[4] = {
                        (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
                        (fg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
                        (bg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
                        (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
                    };
                    uint_fast16_t bits = mc6847_normal[(m & 0x3f) * 12 + glyph_line];
                    // check inverse
                    if (main_memory[mem_offset_of_line + col]  & 0x40) {
                        bits ^= 0xff;
                    }
                    for(int i = 0; i < 4; i++) {
                        sl->data[sl_pos] = bits_to_pixelpair[(bits >> 6) & 0x03];
                        sl_pos++;
                        bits <<= 2;
                    }
                }
            }
            sl->repeat_count = 1;

            sl->length = sl_pos;
            
            break;
        case SEMI_GRAPHICS_6:
            // TODO
            break;
        case GRAPHICS_1C:
            // TODO
            break;
        case GRAPHICS_2C:
            mem_offset_of_line = base_addr + ((line/3) * 32);
            for(uint col = 0; col < 32; col++) {
                uint8_t m = main_memory[mem_offset_of_line + col];
                colorset = 4 * (pia_video_mode & 0x01);
                sl->data[sl_pos++] = (mc6847_palette[colorset+ ((m >> 4) & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_palette[colorset + ((m >> 6) & 0x3)] | THEN_EXTEND_3 );
                sl->data[sl_pos++] = (mc6847_palette[colorset + (m & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_palette[colorset + ((m >> 2) & 0x3)] | THEN_EXTEND_3 );
            }
            sl->repeat_count = 1;
            sl->length = sl_pos;
            break;
        case GRAPHICS_3C:
            mem_offset_of_line = base_addr + ((line/2) * 32);
            for(uint col = 0; col < 32; col++) {
                uint8_t m = main_memory[mem_offset_of_line + col];
                colorset = 4 * (pia_video_mode & 0x01);
                sl->data[sl_pos++] = (mc6847_palette[colorset + ((m >> 4) & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_palette[colorset + ((m >> 6) & 0x3)] | THEN_EXTEND_3 );
                sl->data[sl_pos++] = (mc6847_palette[colorset + (m & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_palette[colorset + ((m >> 2) & 0x3)] | THEN_EXTEND_3 );
            }
            sl->repeat_count = 1;
            sl->length = sl_pos;
            break;
        case GRAPHICS_6C:
            mem_offset_of_line = base_addr + (line * 32);
            for(uint col = 0; col < 32; col++) {
                uint8_t m = main_memory[mem_offset_of_line + col];
                colorset = 4 * (pia_video_mode & 0x01);
                sl->data[sl_pos++] = (mc6847_palette[colorset + ((m >> 4) & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_palette[colorset + ((m >> 6) & 0x3)] | THEN_EXTEND_3 );
                sl->data[sl_pos++] = (mc6847_palette[colorset + (m & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_palette[colorset + ((m >> 2) & 0x3)] | THEN_EXTEND_3 );
            }
            sl->repeat_count = 1;
            sl->length = sl_pos;
            break;
        case GRAPHICS_1R:
            mem_offset_of_line = base_addr + ((line/3) * 16);
            colorset = 2 * (pia_video_mode & 0x01);
            bg_color = mc6847_palette[8 + colorset + 0];
            fg_color = mc6847_palette[8 + colorset + 1];
            for(uint col = 0; col < 16; col++) {
                uint8_t m = main_memory[mem_offset_of_line + col];
                uint32_t bits_to_pixelpair[4] = {
                    (bg_color << 16) | (THEN_EXTEND_3 << 16) | bg_color | THEN_EXTEND_3,
                    (fg_color << 16) | (THEN_EXTEND_3 << 16) | bg_color | THEN_EXTEND_3,
                    (bg_color << 16) | (THEN_EXTEND_3 << 16) | fg_color | THEN_EXTEND_3,
                    (fg_color << 16) | (THEN_EXTEND_3 << 16) | fg_color | THEN_EXTEND_3,
                };

                for(int i = 0; i < 4; i++) {
                    sl->data[sl_pos] = bits_to_pixelpair[(m >> 6) & 0x03];
                    sl_pos++;
                    m <<= 2;
                }
            }
            sl->repeat_count = 1;
            sl->length = sl_pos;
            break;

        case GRAPHICS_2R:
            mem_offset_of_line = base_addr + ((line/2) * 16);
            colorset = 2 * (pia_video_mode & 0x01);
            bg_color = mc6847_palette[8 + colorset + 0];
            fg_color = mc6847_palette[8 + colorset + 1];

            for(uint col = 0; col < 16; col++) {
                uint8_t m = main_memory[mem_offset_of_line + col];
                uint32_t bits_to_pixelpair[4] = {
                    (bg_color << 16) | (THEN_EXTEND_3 << 16) | bg_color | THEN_EXTEND_3,
                    (fg_color << 16) | (THEN_EXTEND_3 << 16) | bg_color | THEN_EXTEND_3,
                    (bg_color << 16) | (THEN_EXTEND_3 << 16) | fg_color | THEN_EXTEND_3,
                    (fg_color << 16) | (THEN_EXTEND_3 << 16) | fg_color | THEN_EXTEND_3,
                };

                for(int i = 0; i < 4; i++) {
                    sl->data[sl_pos] = bits_to_pixelpair[(m >> 6) & 0x03];
                    sl_pos++;
                    m <<= 2;
                }
            }
            sl->repeat_count = 1;
            sl->length = sl_pos;
            break;

        case GRAPHICS_3R:
            mem_offset_of_line = base_addr + (line * 16);
            colorset = 2 * (pia_video_mode & 0x01);
            bg_color = mc6847_palette[8 + colorset + 0];
            fg_color = mc6847_palette[8 + colorset + 1];
            for(uint col = 0; col < 16; col++) {
                uint8_t m = main_memory[mem_offset_of_line + col];
                uint32_t bits_to_pixelpair[4] = {
                    (bg_color << 16) | (THEN_EXTEND_3 << 16) | bg_color | THEN_EXTEND_3,
                    (fg_color << 16) | (THEN_EXTEND_3 << 16) | bg_color | THEN_EXTEND_3,
                    (bg_color << 16) | (THEN_EXTEND_3 << 16) | fg_color | THEN_EXTEND_3,
                    (fg_color << 16) | (THEN_EXTEND_3 << 16) | fg_color | THEN_EXTEND_3,
                };

                for(int i = 0; i < 4; i++) {
                    sl->data[sl_pos] = bits_to_pixelpair[(m >> 6) & 0x03];
                    sl_pos++;
                    m <<= 2;
                }
            }
            sl->repeat_count = 1;
            sl->length = sl_pos;
            break;

        case GRAPHICS_6R:
            mem_offset_of_line = base_addr + (line * 32);

            if (0 ==1) {
                // artifact hi-res
                for(uint col = 0; col < 32; col++) {
                    uint8_t m = main_memory[mem_offset_of_line + col];
                    sl->data[sl_pos++] = (mc6847_artifact1_palette[((m >> 4) & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_artifact1_palette[ ((m >> 6) & 0x3)] | THEN_EXTEND_3 );
                    sl->data[sl_pos++] = (mc6847_artifact1_palette[ (m  & 0x3)] | THEN_EXTEND_3 ) << 16 | (mc6847_artifact1_palette[ ((m >> 2)& 0x3)] | THEN_EXTEND_3 );
                }
                sl->repeat_count = 1;
                sl->length = sl_pos;
                break;
            } else if (2 == 2) {
                // this artifact generator is not great but actually works reasonably OK across a bunch of games. Its only one color set. you really just need to swap the 1 and 2 colors to get the other set
                fg_color = mc6847_artifact1_palette[0];
                bg_color = mc6847_artifact1_palette[3];
                uint32_t bits;
                uint32_t next_pair;

                for(uint col = 0; col < 32; col++) {
                    uint8_t m = main_memory[mem_offset_of_line + col];
                    int i = 0;
                    while (i < 4) {
                        if (i<3) {
                            next_pair = (m>>4) & 0x03;
                        } else {
                            next_pair = ((main_memory[mem_offset_of_line + col+1])>>6) & 0x03;
                        }

                        // render two identical color pixels
                        bits = mc6847_artifact1_palette[ (( m>>6) & 0x03)] | THEN_EXTEND_1 | ( mc6847_artifact1_palette[(( m>>6) & 0x03)] | THEN_EXTEND_1 ) << 16;
                        // if the next pair is different then switch back to black and white mode
                        if ( ((m>>6) & 0x03) != next_pair) {
                            //bits = (((( m>>6) & 0x01) ? bg_color: fg_color) | THEN_EXTEND_1) << 16 |  (((( m>>7) & 0x01) ? bg_color: fg_color) | THEN_EXTEND_1) ;
                            bits = (((( m>>6) & 0x01) ? bg_color: mc6847_artifact1_palette[ (( m>>6) & 0x03)]) | THEN_EXTEND_1) << 16 |  (((( m>>7) & 0x01) ? bg_color: mc6847_artifact1_palette[ (( m>>6) & 0x03)]) | THEN_EXTEND_1) ;
                        }

                        sl->data[sl_pos++] = bits;
                        m <<=2;
                        i++;
                    }
                }
                sl->repeat_count = 1;
                sl->length = sl_pos;
            } else {
                // normal hi-res
                colorset = 2 * (pia_video_mode & 0x01);
                bg_color = mc6847_palette[8 + colorset + 0];
                fg_color = mc6847_palette[8 + colorset + 1];
                for(uint col = 0; col < 32; col++) {
                    uint8_t m = main_memory[mem_offset_of_line + col];
                    uint32_t bits_to_pixelpair[4] = {
                        (bg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
                        (fg_color << 16) | (THEN_EXTEND_1 << 16) | bg_color | THEN_EXTEND_1,
                        (bg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
                        (fg_color << 16) | (THEN_EXTEND_1 << 16) | fg_color | THEN_EXTEND_1,
                    };

                    for(int i = 0; i < 4; i++) {
                        sl->data[sl_pos] = bits_to_pixelpair[(m >> 6) & 0x03];
                        sl_pos++;
                        m <<= 2;
                    }
                }
                sl->repeat_count = 1;
                sl->length = sl_pos;
            }
            break;
           
    }
    vga_submit_scanline(sl);

}


// Only called from the render core

void render_mc6847() {
    vga_prepare_frame();
    // Skip 48 lines to center vertically
    vga_skip_lines(84);

    for(int line = 0; line < 192; line++) {
        render_mc6847_line(line, (mc6847_sam_vdg_display_offset << 9 ),vdg_get_mode());
    }
}

