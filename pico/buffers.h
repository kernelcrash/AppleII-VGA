#pragma once

#include <stdint.h>
#include <stdbool.h>



extern volatile uint32_t soft_switches;
extern volatile uint32_t soft_video7_mode;
extern volatile bool soft_80col;
extern volatile bool soft_80store;
extern volatile bool soft_altcharset;
extern volatile bool soft_dhires;
extern volatile bool soft_monochrom;
extern volatile bool soft_ramwrt;


extern volatile bool soft_scanline_emulation;

#define CHARACTER_ROM_SIZE 512

extern uint8_t character_rom[CHARACTER_ROM_SIZE];

extern uint8_t main_memory[64 * 1024];



