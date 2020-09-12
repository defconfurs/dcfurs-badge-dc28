
#ifndef _BADGE_H
#define _BADGE_H

#include <stdint.h>
#include <stdarg.h>

/* The CPU clock speed */
#define CORE_CLOCK_FREQUENCY    16000000

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
    uint32_t __reserved0;
    /* Hardware Multiply helper - experimental */
    uint32_t mul_ctrl;      /* DSP multiply control register */
    uint32_t __reserved1;
    uint32_t mul_op_a;      /* DSP multiply operand A */
    uint32_t mul_op_b;      /* DSP multiply operand B */
    uint32_t mul_accum;     /* DSP multiply operand C */
    uint32_t mul_xaccum;    /* DSP multiply extended operand C (not implemented) */
    uint32_t mul_result;    /* DSP multiply result = C +/- (A*B) */
    uint32_t mul_xresult;   /* DSP multiply extended result (not implemented) */ 
};
#define MISC ((volatile struct misc_regs *)0x40000000)

#define MUL_STATUS_CARRY    (1 << 0)    /* Carry output to multiply */
#define MUL_CONTROL_CARRY   (1 << 4)    /* Carry input to multiply */
#define MUL_CONTROL_FMADD   (0 << 6)    /* Compute RES = C + (A*B) */
#define MUL_CONTROL_FMSUB   (1 << 6)    /* Compute RES = C - (A*B) */

/*=====================================
 * USB/Serial UART
 *=====================================
 */
struct serial_regmap {
    uint32_t rxfifo;    /* Receive Holding Register. */
    uint32_t txfifo;    /* Transmit Holding Register. */
    uint32_t ier;       /* Interrupt Enable Register. */
    uint32_t isr;       /* Interrupt Status Register. */
} __attribute__((packed));
#define SERIAL ((volatile struct serial_regmap *)0x40010000)

#define SERIAL_INT_RXDATA_READY 0x01    /* One or more bytes are ready in the RXFIFO */
#define SERIAL_INT_TXFIFO_EMPTY 0x02    /* The TXFIFO is empty */
#define SERIAL_INT_DTR_ACTIVE   0x04    /* Data Terminal Ready signal is active */
#define SERIAL_INT_RTS_ACTIVE   0x08    /* Ready to Send signal is active */

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
void framebuf_render(const struct framebuf *frame);

uint16_t hsv2pixel(unsigned int hue, uint8_t sat, uint8_t value);

/*=====================================
 * Animation Header Structure
 *=====================================
 */
#define BOOT_HDR_TAG    0xDEADBEEF

struct boot_header {
    uint32_t tag;   /* Must match BOOT_HDR_TAG to be a valid image. */
    uint32_t flags; /* Reserved for future use. */
    uint32_t entry; /* Program entry point address */
    /* Program Section Headers */
    uint32_t data_size;
    uint32_t data_start;
    uint32_t frame_size;
    uint32_t frame_start;
    uint32_t name_size;
    uint8_t  name[32];
};

#endif /* _BADGE_H */
