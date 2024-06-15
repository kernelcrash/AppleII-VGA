#pragma once

#include <stdint.h>

extern const uint16_t ntsc_palette[16];
extern const uint16_t ntsc90_palette[16];

extern const uint16_t mono_bg_colors[4];
extern const uint16_t mono_fg_colors[4];
extern uint16_t mono_bg_color;
extern uint16_t mono_fg_color;

extern uint32_t v9938_palette[16+256];
extern uint32_t v9938_default_palette[16+256];
extern void set_pen16(int i, uint32_t color);
extern void set_pen16_rgb(int i, uint8_t red, uint8_t green, uint8_t blue);
extern void set_pen256_rgb(int i, uint8_t red, uint8_t green, uint8_t blue);


extern void set_pen256(int i, uint32_t color);
extern uint32_t pen16(int index);
extern uint32_t pen256(int index);


