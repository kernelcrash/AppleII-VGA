#include "colors.h"

// clang-format off
#define RGB333(r, g, b) (uint32_t)( \
    (((((unsigned int)(r) * 256 / 36) + 128) / 256) << 6) | \
    (((((unsigned int)(g) * 256 / 36) + 128) / 256) << 3) | \
    ((((unsigned int)(b) * 256 / 36) + 128) / 256) \
)

// from MAME
uint32_t v9938_palette[16+256];
uint32_t v9938_default_palette[16+256] = {

        RGB333(0x00, 0x00, 0x00), /* BLACK */
        RGB333(0x00, 0x00, 0x00), /* BLACK */
        RGB333(33,200,66),
        RGB333(94,220,120),
        RGB333(84,85,237),
        RGB333(125,118,252),
        RGB333(212,82,77),
        RGB333(66,235,245),
        RGB333(252,85,84),
        RGB333(255,121,120),
        RGB333(212,193,84),
        RGB333(230,206,128),
        RGB333(33,176,59),
        RGB333(201,91,186),
        RGB333(204,204,204),
        RGB333(255,255,255)

};

void set_pen16(int i, uint32_t color){
	v9938_palette[i] = color;
}

//void set_pen16_rgb(int i, uint8_t red, uint8_t green, uint8_t blue){
//	v9938_palette[i] = RGB333(red,green,blue);
//}

// This one takes values of 0-7 for each colour component
void set_pen16_rgb(int i, uint8_t red, uint8_t green, uint8_t blue){
	v9938_palette[i] = (red << 6) | (green << 3) | (blue & 0x07);
}

void set_pen256(int i, uint32_t color){
	v9938_palette[i+16] = color;
}

void set_pen256_rgb(int i, uint8_t red, uint8_t green, uint8_t blue){
	v9938_palette[i+16] = RGB333(red,green,blue);
}

uint32_t pen16(int index) { return v9938_palette[index]; }
uint32_t pen256(int index) { return v9938_palette[index + 16]; }

