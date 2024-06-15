#pragma once

#include <stdint.h>

extern const uint16_t ntsc_palette[16];
extern const uint16_t ntsc90_palette[16];

extern const uint16_t mono_bg_colors[4];
extern const uint16_t mono_fg_colors[4];
extern uint16_t mono_bg_color;
extern uint16_t mono_fg_color;

extern const uint16_t mc6847_palette[16];
extern const uint16_t mc6847_artifact1_palette[4];


#define     MC6847_COLOR_LIGHT_GREEN	0
#define     MC6847_COLOR_YELLOW 	1
#define     MC6847_COLOR_LIGHT_BLUE	2
#define     MC6847_COLOR_LIGHT_RED	3
#define     MC6847_COLOR_WHITE		4
#define     MC6847_COLOR_CYAN		5
#define     MC6847_COLOR_LIGHT_MAGENTA	6
#define     MC6847_COLOR_BROWN		7
#define     MC6847_COLOR_BLACK		8
#define     MC6847_COLOR_LIGHT_GREEN2	9
#define     MC6847_COLOR_BLACK2		10
#define     MC6847_COLOR_WHITE_2	11
#define     MC6847_COLOR_ALPHANUMERIC_DARK_GREEN	12
#define     MC6847_COLOR_ALPHANUMERIC_BRIGHT_GREEN	13
#define     MC6847_COLOR_ALPHANUMERIC_DARK_ORANGE	14
#define     MC6847_COLOR_ALPHANUMERIC_BRIGHT_ORANGE	15