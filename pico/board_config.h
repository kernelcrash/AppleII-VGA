#pragma once

#define CONFIG_SYSCLOCK 126.0 /* MHz */

// Pin configuration
#define CONFIG_PIN_Z80_DATA_BASE 0 /* 8+2 pins */
#define CONFIG_PIN_Z80_MREQ (CONFIG_PIN_Z80_DATA_BASE + 8)
#define CONFIG_PIN_Z80_IORQ (CONFIG_PIN_Z80_DATA_BASE + 9)
#define CONFIG_PIN_APPLEBUS_SYNC 10
#define CONFIG_PIN_APPLEBUS_CONTROL_BASE 11 /* 3 pins */
#define CONFIG_PIN_Z80_WR 26
#define CONFIG_PIN_HSYNC 28
#define CONFIG_PIN_VSYNC 27
#define CONFIG_PIN_RGB_BASE 14 /* 9 pins */

// Other resources
#define CONFIG_VGA_PIO pio0
#define CONFIG_VGA_SPINLOCK_ID 31
#define CONFIG_ABUS_PIO pio1
