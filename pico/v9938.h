#pragma once

#include <stdint.h>
#include <stdbool.h>

#define nullptr NULL

extern volatile bool v9938_enabled;
extern volatile bool v9938_80col_enabled;
extern volatile bool v9938_mem_selected;

extern void v9938_reset();
        uint8_t v9938_vram_r();
        uint8_t v9938_status_r();
	void v9938_palette_w(uint8_t data);

        void v9938_vram_w(uint8_t data);
        void v9938_command_w(uint8_t data);
        void v9938_register_w(uint8_t data);
extern        void v9938_vram_write(int offset, int data);
extern        int v9938_vram_read(int offset);

extern uint8_t v9938_read(uint_fast16_t address);
extern void v9938_write(uint_fast16_t address, uint8_t data);
extern void v9938_register_write (int reg, int data);
extern void v9938_reset_palette();
extern void v9938_set_mode();
inline int position_offset(uint8_t value) { value &= 0x0f; return (value < 8) ? -value : 16 - value; }
extern void v9938_mode_unknown(int line);


        void v9938_update_command();
        uint8_t v9938_command_unit_w(uint8_t Op);

void v9938_cpu_to_vdp(uint8_t V);
uint8_t v9938_vdp_to_cpu();







extern void v9938_enable();
extern void v9938_disable();
extern void v9938_shadow_register(bool is_write, uint_fast16_t address, uint_fast8_t data);
extern void v9938_shadow_c8xx(bool is_write, uint_fast16_t address, uint_fast8_t value);

extern void v9938_update_flasher();
extern void render_v9938();
extern void v9938_sam_write(uint_fast16_t address);
extern void v9938_set_pia_mode(uint8_t pia_mode);

extern void v9938_set_page(uint8_t page);
extern void v9938_set_map_type(uint8_t map_type);

extern uint8_t v9938_get_page();
extern uint8_t v9938_get_map_type();

//extern uint8_t v9938_read(uint_fast16_t address);
//extern void v9938_write(uint_fast16_t address, uint8_t data);
//extern uint8_t v9938_vram_read();
//extern void v9938_vram_write(uint8_t data); 
//extern void v9938_register_write(uint8_t data);
//extern uint8_t v9938_register_read();
//extern void v9938_change_register(uint8_t reg, uint8_t val);
//extern void v9938_update_table_masks(); 
//extern void v9938_update_backdrop();


