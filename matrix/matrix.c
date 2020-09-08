#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdatomic.h>
#include "badge.h"

static const uint16_t colours[8] = {
    0xF800, /* Red */
    0xF300, /* Orange */
    0xF5E0, /* Yellow */
    0x07C0, /* Green */
    0x07FF, /* Cyan */
    0x001F, /* Blue */
    0x7817, /* Purple */
    0xFFFF  /* White */
};

void main(void)
{
    struct framebuf *frames[2];
    struct framebuf *buf;
    int counter = 0;
    uint8_t state[DISPLAY_HRES][DISPLAY_VRES];

    /* Seed the PRNG */
    register uint32_t seed;
    asm volatile ("csrr %0, mcycle" : "=r"(seed));
    srand(seed);

    printf("Take the Red Pill\n");

    /* Allocate some frames. */
    frames[0] = framebuf_alloc();
    frames[1] = framebuf_alloc();
    memset(state, 0, sizeof(state));

    for (counter = 0;; counter++) {
        int x, y;

        /* Move each pixel down a row and decay. */
        for (x = 0; x < DISPLAY_HRES; x++) {
            uint8_t *column = state[x];

            for (y = DISPLAY_VRES-1; y > 0; y--) {
                if (column[y-1] == 0xff) column[y] = 0xff; /* Drop moves down */
                else if (column[y] >= 32) column[y] -= 32; /* Otherwise decay */
                else column[y] = 0;
            }

            /* Top always decays. */
            if (column[0] >= 32) column[0] -= 32;
            else column[0] = 0;
        }
        
        /* Generate a new raindrop every other frame */
        if (counter & 1) {
            unsigned int i = rand() % DISPLAY_HWIDTH;
            /* Ignore drops that would land outside the display, or
             * in a column that already has an active drop */
            if ((i < DISPLAY_HRES) && (state[i][0] < 32)) {
                state[i][0] = 0xff;
            }
        }

        /* Redraw the frame */
        buf = frames[counter & 1];
        for (x = 0; x < DISPLAY_HRES; x++) {
            uint8_t *column = state[x];
            for (y = 0; y < DISPLAY_VRES; y++) {
                uint16_t value;
                if (column[y] == 0xff) value = colours[rand() & 0x7];
                else if (column[y]) value = (column[y] & 0xFC) << 3;
                else value = (3 << 5);

                buf->data[y * DISPLAY_HWIDTH + x] = value;
            }
        }
        framebuf_render(buf);

        /* Wait for some time */
        usleep(200000);
    }
}