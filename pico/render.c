#include "render.h"
#include "buffers.h"
#include "tms9918a.h"


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
        render_tms9918a();
    }
#endif
}
