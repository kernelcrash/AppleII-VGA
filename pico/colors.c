#include "colors.h"

// clang-format off
#define RGB333(r, g, b) (uint16_t)( \
    (((((unsigned int)(r) * 256 / 36) + 128) / 256) << 6) | \
    (((((unsigned int)(g) * 256 / 36) + 128) / 256) << 3) | \
    ((((unsigned int)(b) * 256 / 36) + 128) / 256) \
)

const uint16_t zx_spectrum_palette[16] = {

// normal BRIGHT=0 colours
        RGB333(0x00, 0x00, 0x00), /* BLACK */
        RGB333(0x00, 0x00, 0xd7), /* BLUE */
        RGB333(0xd7, 0x00, 0x00), /* RED */
        RGB333(0xd7, 0x00, 0xd7), /* MAGENTA */
        RGB333(0x00, 0xc0, 0x00), /* GREEN */
        RGB333(0x00, 0xd7, 0xd7), /* CYAN */
        RGB333(0xd7, 0xd7, 0x00), /* YELLOW */
        RGB333(0xd7, 0xd7, 0xd7), /* WHITE */
// 'bright' colourset
        RGB333(0x00, 0x00, 0x00), /* BLACK */
        RGB333(0x00, 0x00, 0xff), /* BLUE */
        RGB333(0xff, 0x00, 0x00), /* RED */
        RGB333(0xff, 0x00, 0xff), /* MAGENTA */
        RGB333(0x00, 0xff, 0x00), /* GREEN */
        RGB333(0x00, 0xff, 0xff), /* CYAN */
        RGB333(0xff, 0xff, 0x00), /* YELLOW */
        RGB333(0xff, 0xff, 0xff), /* WHITE */
};

