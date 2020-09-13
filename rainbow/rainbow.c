#include <stdint.h>
#include <string.h>
#include "badge.h"

static inline uint32_t rdcycles_32(void)
{
    register uint32_t cyc_lo;
    asm volatile("csrr %[lo],  mcycle" : [lo]"=r"(cyc_lo));
    return cyc_lo;
}

static void
update_frame(int framenum)
{
    static int init = 0;
    static struct framebuf *frames[2];
    
    struct framebuf *address;
    int x, y, ystart;
    int offset;
    uint16_t colour;
    uint16_t hue;

    /* Allocate some frames */
    if (!init) {
        frames[0] = framebuf_alloc();
        frames[1] = framebuf_alloc();
        init = 1;
    }

    offset = framenum >> 2;

    static const uint16_t colours[] = {
        0xF800, /* Red */
        0xF300, /* Orange */
        0xF5E0, /* Yellow */
        0x07C0, /* Green */
        0x001F, /* Blue */
        0x7817, /* Purple */
        0xFFFF  /* White */
    };

    address = frames[framenum & 1];

    ystart = 0;
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 10; x++) {
            hue = ((offset<<2) + (x<<4) + (y<<4));
            colour = hsv2pixel(hue, 255, 255);
            address->data[x + ystart]      = colour;
            address->data[19 - x + ystart] = colour;
        }
        ystart += DISPLAY_HWIDTH;
    }
    framebuf_render(address);
}

static inline void check_serial(void) {
    if (SERIAL->isr & SERIAL_INT_RXDATA_READY) {
        uint8_t ch = SERIAL->rxfifo;
        printf("%c", ch);
    }
}

void main(void) {
    int nexttime = 0;
    int framenum = 0;
    signed int diff;

    // And finally - the main loop.
    while (1) {
        //----------------------------------------------------------------------------------------------
        // try to get some form of deterministic timing... kinda
        do { diff = nexttime - rdcycles_32(); } while (diff > 0);
        nexttime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 150);
        // edgecase for the roll-over event so it doesn't miss it (loop must be less than 512 cycles)
        if (nexttime > 0xFFFFF700) nexttime = 0;
        //----------------------------------------------------------------------------------------------

        update_frame(framenum++);
    }
}
