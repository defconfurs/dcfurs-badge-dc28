
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
    void (*bios_bootexit)(int code);
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
    uint32_t i_enable;  /* Interrupt Enable */
    uint32_t i_status;  /* Interrupt Status */ 
};
#define MISC ((volatile struct misc_regs *)0x40000000)

/*=====================================
 * USB/Serial UART
 *=====================================
 */
struct serial_regmap {
    union {
        uint32_t thr;   // Transmit Holding Register.
        uint32_t rhr;   // Receive Holding Register.
    };
    uint32_t ier;   // Interrupt Enable Register.
    uint32_t isr;   // Interrupt Status Register.
} __attribute__((packed));
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

struct framebuf {
    uint16_t data[DISPLAY_HWIDTH * DISPLAY_VRES];
};

struct framebuf *framebuf_alloc(void);
void framebuf_free(struct framebuf *frame);
void framebuf_render(struct framebuf *fframe);

/*=====================================
 * Animation Header Structure
 *=====================================
 */
#define BOOT_HDR_TAG    0xDEADBEEF

struct boot_header {
    uint32_t tag;   /* Must match BOOT_HDR_TAG to be a valid image. */
    uint32_t flags; /* Reserved for future use. */
    uint32_t entry; /* Program entry point address */
    uint32_t data_size;  /* Size of data to load into memory. */
    /* The following are reserved for future use. */
    uint32_t data_start;
    uint32_t frame_size;
    uint32_t frame_start;
    uint32_t name_size;
    uint8_t  name[32];
};

#endif /* _BADGE_H */
