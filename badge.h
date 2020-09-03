
#ifndef _BADGE_H
#define _BADGE_H

#include <stdint.h>
#include <stdarg.h>

/*=====================================
 * Functions Exported by the BIOS
 *=====================================
 */
struct bios_vtable {
    void (*bios_bootload)(int slot);
    int (*bios_printf)(const char *fmt, ...);
    int (*bios_vprintf)(const char *fmt, va_list);
};
#define VTABLE ((const struct bios_vtable *)0x00000010)

/* TODO: Would some __builtin_apply() magic be better than macros? */
#define bootload(_slot_) VTABLE->bios_bootload(_slot_)
#define printf(...) VTABLE->bios_printf(__VA_ARGS__)
#define vprintf(...) VTABLE->bios_vprintf(__VA_ARGS__)

/*=====================================
 * Miscellaneous Peripherals
 *=====================================
 */
struct misc_regs {
    uint32_t leds[3];   /* Status LED PWM intensity: Red, Green and Blue */
    uint32_t button;    /* Button Status */
    uint32_t mic;       /* Microphone Data */
};
#define MISC ((volatile struct misc_regs *)0x40000000)

/*=====================================
 * USB/Serial UART
 *=====================================
 */
typedef uint32_t serial_reg_t;
struct serial_regs {
    union {
        serial_reg_t thr;   // Transmit Holding Register.
        serial_reg_t rhr;   // Receive Holding Register.
    };
    serial_reg_t ier;   // Interrupt Enable Register.
    serial_reg_t isr;   // Interrupt Status Register.
};
#define SERIAL ((volatile struct serial_regmap *)0x40010000)
#define SERIAL_INT_DATA_READY   0x01
#define SERIAL_INT_THR_EMPTY    0x02
#define SERIAL_INT_RECVR_LINE   0x04
#define SERIAL_INT_MODEM_STATUS 0x08

/*=====================================
 * Display Memory
 *=====================================
 */
#define DISPLAY_HRES    20  /* Number of active pixels per row */
#define DISPLAY_VRES    14  /* Number of active pixels per column */
#define DISPLAY_HWIDTH  32  /* Number of total pixels per row */
#define DISPLAY_POINTER (*(volatile uint32_t *)0x40020000)
#define DISPLAY_MEMORY  ((volatile void *)0x40020000)

#endif /* _BADGE_H */
