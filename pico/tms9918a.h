#pragma once

#include <stdint.h>
#include <stdbool.h>


extern volatile bool tms9918a_enabled;
extern volatile bool tms9918a_80col_enabled;
extern volatile bool tms9918a_mem_selected;

extern void tms9918a_init();
extern void tms9918a_enable();
extern void tms9918a_disable();
extern void tms9918a_shadow_register(bool is_write, uint_fast16_t address, uint_fast8_t data);
extern void tms9918a_shadow_c8xx(bool is_write, uint_fast16_t address, uint_fast8_t value);

extern void tms9918a_update_flasher();
extern void render_tms9918a();
extern void tms9918a_sam_write(uint_fast16_t address);
extern void tms9918a_set_pia_mode(uint8_t pia_mode);

extern void tms9918a_set_page(uint8_t page);
extern void tms9918a_set_map_type(uint8_t map_type);

extern uint8_t tms9918a_get_page();
extern uint8_t tms9918a_get_map_type();

extern uint8_t tms9918a_read(uint_fast16_t address);
extern void tms9918a_write(uint_fast16_t address, uint8_t data);
extern uint8_t tms9918a_vram_read();
extern void tms9918a_vram_write(uint8_t data); 
extern void tms9918a_register_write(uint8_t data);
extern uint8_t tms9918a_register_read();
extern void tms9918a_change_register(uint8_t reg, uint8_t val);
extern void tms9918a_update_table_masks(); 
extern void tms9918a_update_backdrop();
