#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "badge.h"

extern const struct framebuf missingno_png;

void
main(void)
{
    unsigned int offset = 0;
    struct framebuf *buf = framebuf_alloc();

    printf("Wild MISSINGNO appeared.\n");
    while (1) {
        unsigned int x, y;

        /* Copy the missingno template and mirror it around the centre of the mask. */
        for (x = 0; x < DISPLAY_HRES/2; x++) {
            /* Get the source X position from the template. */
            int srcx = DISPLAY_HRES - offset + x;
            if (srcx >= DISPLAY_HRES) {
                srcx -= DISPLAY_HRES;
            }

            for (y = 0; y < DISPLAY_VRES; y++) {
                int ystart = (y * DISPLAY_HWIDTH);
                uint16_t pixel = missingno_png.data[ystart + srcx];
                buf->data[ystart + (DISPLAY_HRES/2) + x] = pixel;
                buf->data[ystart + (DISPLAY_HRES/2) - x - 1] = pixel;
            }
        }
        offset++;
        if (offset >= DISPLAY_HRES) {
            offset -= DISPLAY_HRES;
        }

        framebuf_render(buf);
        usleep(250000);
    }
}