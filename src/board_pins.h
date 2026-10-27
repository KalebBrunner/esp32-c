#pragma once

// ESP32-S3 GPIO numbers, not physical header positions.
// Avoid GPIO0/3/45/46 for general wiring: boot strapping pins.
// Keep GPIO19/20 free for our native USB connection.
// Avoid GPIO26-32 (flash/PSRAM); GPIO33-37 may also be reserved on octal-memory modules.
// GPIO43/44 are UART0; GPIO39-42 are external JTAG pins. Reserve them if using those interfaces.
// Check the exact board schematic for LEDs and other onboard connections before adding pins.

// GPIO33/34 may not be exposed; GPIO35-37 depend on module memory wiring.
// GPIO38/48 may drive the onboard RGB LED, depending on board revision.
// Unused means unassigned by this firmware, not guaranteed free on every board.
//

enum Pin {
    Pin_BootStrap0 = 0,
    Pin_SDA = 1,
    Pin_SCL = 2,
    Pin_BootStrap3 = 3,
    Pin_Unused4 = 4,
    Pin_Unused5 = 5,
    Pin_Unused6 = 6,
    Pin_Unused7 = 7,
    Pin_Unused8 = 8,
    Pin_Unused9 = 9,
    Pin_Unused10 = 10,
    Pin_Unused11 = 11,
    Pin_Unused12 = 12,
    Pin_Unused13 = 13,
    Pin_Unused14 = 14,
    Pin_Unused15 = 15,
    Pin_Unused16 = 16,
    Pin_Unused17 = 17,
    Pin_Unused18 = 18,
    Pin_USB_DMinus = 19,
    Pin_USB_DPlus = 20,
    Pin_Unused21 = 21,
    // GPIO22-25 do not exist on ESP32-S3.
    Pin_Memory26 = 26,
    Pin_Memory27 = 27,
    Pin_Memory28 = 28,
    Pin_Memory29 = 29,
    Pin_Memory30 = 30,
    Pin_Memory31 = 31,
    Pin_Memory32 = 32,
    Pin_ModuleDependent33 = 33,
    Pin_ModuleDependent34 = 34,
    Pin_ModuleDependent35 = 35,
    Pin_ModuleDependent36 = 36,
    Pin_ModuleDependent37 = 37,
    Pin_BoardDependent38 = 38,
    Pin_JTAG_MTCK = 39,
    Pin_JTAG_MTDO = 40,
    Pin_JTAG_MTDI = 41,
    Pin_JTAG_MTMS = 42,
    Pin_UART0_TX = 43,
    Pin_UART0_RX = 44,
    Pin_BootStrap45 = 45,
    Pin_BootStrap46 = 46,
    Pin_Unused47 = 47,
    Pin_BoardDependent48 = 48
};
