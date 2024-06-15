#include "colors.h"

// clang-format off
#define RGB333(r, g, b) (uint16_t)( \
    (((((unsigned int)(r) * 256 / 36) + 128) / 256) << 6) | \
    (((((unsigned int)(g) * 256 / 36) + 128) / 256) << 3) | \
    ((((unsigned int)(b) * 256 / 36) + 128) / 256) \
)

// from MAME
const uint16_t mc6847_palette[16] = {

        RGB333(0x30, 0xd2, 0x00), /* GREEN */
        RGB333(0xc1, 0xe5, 0x00), /* YELLOW */
        RGB333(0x4c, 0x3a, 0xb4), /* BLUE */
        RGB333(0x9a, 0x32, 0x36), /* RED */
        RGB333(0xbf, 0xc8, 0xad), /* BUFF */
        RGB333(0x41, 0xaf, 0x71), /* CYAN */
        RGB333(0xc8, 0x4e, 0xf0), /* MAGENTA */
        RGB333(0xd4, 0x7f, 0x00), /* ORANGE */

        RGB333(0x26, 0x30, 0x16), /* BLACK */
        RGB333(0x30, 0xd2, 0x00), /* GREEN */
        RGB333(0x26, 0x30, 0x16), /* BLACK */
        RGB333(0xbf, 0xc8, 0xad), /* BUFF */

        RGB333(0x00, 0x7c, 0x00), /* ALPHANUMERIC DARK GREEN */
        RGB333(0x30, 0xd2, 0x00), /* ALPHANUMERIC BRIGHT GREEN */
        RGB333(0x6b, 0x27, 0x00), /* ALPHANUMERIC DARK ORANGE */
        RGB333(0xff, 0xb7, 0x00)  /* ALPHANUMERIC BRIGHT ORANGE */
};

const uint16_t mc6847_artifact1_palette[4] = {
        RGB333(0x26, 0x30, 0x16), /* BLACK */
        RGB333(0x00, 0x80, 0xff), /* WEIRD BLUE */
        RGB333(0xd4, 0x7f, 0x00), /* ORANGE */ 
        RGB333(0xbf, 0xc8, 0xad), /* BUFF */

};

