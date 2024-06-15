#pragma once

#include <stdint.h>
#include <stdbool.h>


extern volatile bool mc6847_enabled;
extern volatile bool mc6847_80col_enabled;
extern volatile bool mc6847_mem_selected;

extern void mc6847_init();
extern void mc6847_enable();
extern void mc6847_disable();
extern void mc6847_shadow_register(bool is_write, uint_fast16_t address, uint_fast8_t data);
extern void mc6847_shadow_c8xx(bool is_write, uint_fast16_t address, uint_fast8_t value);

extern void mc6847_update_flasher();
extern void render_mc6847();
extern void mc6847_sam_write(uint_fast16_t address);
extern void mc6847_set_pia_mode(uint8_t pia_mode);

extern void mc6847_set_page(uint8_t page);
extern void mc6847_set_map_type(uint8_t map_type);

extern uint8_t mc6847_get_page();
extern uint8_t mc6847_get_map_type();

