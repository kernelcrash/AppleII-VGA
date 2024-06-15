#include "render.h"
#include "buffers.h"
#include "v9938.h"
#ifdef COMPUTER_MODEL_IIPLUS
#include "videx_vterm.h"
#endif


void render_init() {
    //generate_hires_tables();
}


void render_loop() {
#ifdef RENDER_TEST_PATTERN
    while(1) {
        render_vga_testpattern();
    }
#else
    while(1) {
        render_v9938();
    }
#endif
}
