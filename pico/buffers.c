#include "buffers.h"

// Shadow copy of the Apple soft-switches
volatile uint32_t soft_switches;
volatile uint32_t soft_video7_mode;
volatile bool soft_80col;
volatile bool soft_80store;
volatile bool soft_altcharset;
volatile bool soft_dhires;
volatile bool soft_monochrom;
volatile bool soft_ramwrt;

// Custom device soft-switches
volatile bool soft_scanline_emulation;


// The currently programmed character generator ROM for text mode
uint8_t character_rom[64 * 8];

// The lower 24K of main and aux memory (on IIe) where the video memory resides
uint8_t main_memory[64 * 1024];

