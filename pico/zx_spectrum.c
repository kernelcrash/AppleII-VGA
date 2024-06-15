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
#include "zx_spectrum.h"

#include <pico/stdlib.h>
#include "buffers.h"
#include "colors.h"
#include "textfont/textfont.h"
#include "vga.h"

#define FLASH_THRESHOLD 20
static int32_t flash_counter = FLASH_THRESHOLD;
static uint8_t flash_state = 0;

volatile uint8_t border_color;

#define ZX_FLASH 0x80
#define ZX_BRIGHTNESS 0x40

void zx_spectrum_init() {
	border_color = 7;
}

void render_zx_spectrum_border(uint count) {
    struct vga_scanline *sl = vga_prepare_scanline();
    uint sl_pos = 0;

    while(sl_pos < VGA_WIDTH/16) {
        sl->data[sl_pos] = (zx_spectrum_palette[border_color]|THEN_EXTEND_7) | ((zx_spectrum_palette[border_color]|THEN_EXTEND_7) << 16); // 16 pixels per word
        sl_pos++;
    }

    sl->length = sl_pos;
    sl->repeat_count = count - 1;
    vga_submit_scanline(sl);
}

// Render a single scan line (well, actually two that are the same). line will be 0 to 191
static void render_zx_spectrum_line(unsigned int line) {
    uint mem_offset_of_line;
    uint sl_pos;
    uint32_t bg_color;
    uint32_t fg_color;

    struct vga_scanline *sl = vga_prepare_scanline();
    sl_pos = 0;
    // Pad 40 pixels on the left to center horizontally
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_7) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_7) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_7) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_3) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_3) << 16);  // 8 pixels
    
    mem_offset_of_line = 0x4000 + ((((line & 0x07) << 3) | ((line & 0x38) >> 3) | (line & 0xc0) ) << 5);
    for(uint col = 0; col < 32; col++) {
        uint8_t m = main_memory[mem_offset_of_line + col];
	uint8_t colour_attribute = main_memory[0x5800 + (((line >> 3) << 5) + col)];
        bg_color = zx_spectrum_palette[((colour_attribute & ZX_BRIGHTNESS) >> 3) | ((colour_attribute & 0x38) >> 3)];
        fg_color = zx_spectrum_palette[((colour_attribute & ZX_BRIGHTNESS) >> 3) | (colour_attribute & 0x07)];
	if (colour_attribute & ZX_FLASH) {
		if (flash_state) {
			// swap colors
			uint32_t temp_color;
			temp_color = bg_color;
			bg_color = fg_color;
			fg_color = temp_color;
		}
	}
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
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_7) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_7) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_7) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_7) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_7) << 16);  // 16 pixels
    sl->data[sl_pos++] = (zx_spectrum_palette[border_color] | THEN_EXTEND_3) | ((zx_spectrum_palette[border_color] | THEN_EXTEND_3) << 16);  // 8 pixels
												   
    sl->repeat_count = 1;
    sl->length = sl_pos;
    vga_submit_scanline(sl);

}



// Render a screen of VideoTerm text mode
//
// Only called from the render core

void render_zx_spectrum() {
    vga_prepare_frame();
    // Skip 48 lines to center vertically
    //vga_skip_lines(84);
    render_zx_spectrum_border(64);

    for(int line = 0; line < 192; line++) {
        render_zx_spectrum_line(line);
    }
    render_zx_spectrum_border(32);

    flash_state = (flash_counter==0) ? flash_state ^ 1 : flash_state;
    flash_counter = (flash_counter>0) ? flash_counter-1 : FLASH_THRESHOLD;
}

