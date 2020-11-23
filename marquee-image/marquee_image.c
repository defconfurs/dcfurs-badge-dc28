#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "badge.h"

static inline uint32_t rdcycles_32(void)
{
    register uint32_t cyc_lo;
    asm volatile("csrr %[lo],  mcycle" : [lo]"=r"(cyc_lo));
    return cyc_lo;
}

void _putchar(char ch)
{
    /* Cook line endings to play nicely with terminals. */
    static char prev = '\0';
    if ((ch == '\n') && (prev != '\r')) _putchar('\r');
    prev = ch;

    /* Output the character */
    while ((SERIAL->isr & SERIAL_INT_TXFIFO_EMPTY) == 0) { /* nop */}
    SERIAL->txfifo = ch;
}

/* Marquee frame - this is a full sized image. It should be MARQUEE_WIDTH x 28 */
extern const int marquee_fullframe_png_width;
extern const uint16_t marquee_fullframe_png[];
#define MARQUEE_WIDTH (marquee_fullframe_png_width)

static struct framebuf *frames[2];

static void
update_frame(int framenum)
{
    struct framebuf *address;
    int x, y;
    int frame_x, frame_y;
    int frame_address;
    int i;
    int r, g, b;
    const uint16_t *fullframe;
    uint16_t pixel;
    int weights[4];

    address = frames[framenum & 1];
    fullframe = marquee_fullframe_png;

    for (y = 0; y < 14; y++) {
        for (x = 0; x < 20; x++) {
            // determine the weighting based on the position
            if ((x & 1) ^ (y & 1)) {
                weights[0] =  64;
                weights[1] = 255;
                weights[2] =  64;
                weights[3] = 255;
            }
            else {
                weights[0] = 255;
                weights[1] =  64;
                weights[2] = 255;
                weights[3] =  64;
            }

            // find the pixel location
            frame_x = ((x*3) + framenum) % (MARQUEE_WIDTH-1); // -1 due to using 2 pixels at a time
            frame_y = (y<<1);
            frame_address = frame_x + frame_y * MARQUEE_WIDTH;

            // get the first pixel
            pixel = fullframe[frame_address];
            r  = ((pixel >> 11) & 0x1F) * weights[0];
            g  = ((pixel >>  5) & 0x3F) * weights[0];
            b  = ((pixel >>  0) & 0x1F) * weights[0];

            //// second pixel
            pixel = fullframe[frame_address+1];
            r += ((pixel >> 11) & 0x1F) * weights[1];
            g += ((pixel >>  5) & 0x3F) * weights[1];
            b += ((pixel >>  0) & 0x1F) * weights[1];
            
            // third
            pixel = fullframe[frame_address+MARQUEE_WIDTH];
            r += ((pixel >> 11) & 0x1F) * weights[2];
            g += ((pixel >>  5) & 0x3F) * weights[2];
            b += ((pixel >>  0) & 0x1F) * weights[2];
            
            // fourth
            pixel = fullframe[frame_address+1+MARQUEE_WIDTH];
            r += ((pixel >> 11) & 0x1F) * weights[3];
            g += ((pixel >>  5) & 0x3F) * weights[3];
            b += ((pixel >>  0) & 0x1F) * weights[3];
            
            // scale the value back down
            r >>= (2 + 8);
            g >>= (2 + 8);
            b >>= (2 + 8);
            if (r > 0x1F) r = 0x1F;
            if (g > 0x3F) g = 0x3F;
            if (b > 0x1F) b = 0x1F;
            
            // and output
            pixel = (r << 11) + (g << 5) + (b);
            address->data[x + (y<<5)] = pixel;
        }
    }

    framebuf_render(address);
}

void main(void)
{
    int nexttime = rdcycles_32() + 500;
    int nextframetime = rdcycles_32() + 1000;
    int framenum = 0;
    signed int diff;

    frames[0] = framebuf_alloc();
    frames[1] = framebuf_alloc();

    printf("Starting marquee image\n");

    while (1) {
        //----------------------------------------------------------------------------------------------
        // try to get some form of deterministic timing... kinda
        do {
            /* If there are characters received, echo them back. */
            if (SERIAL->isr & SERIAL_INT_RXDATA_READY) {
                uint8_t ch = SERIAL->rxfifo;
                
                _putchar(ch);

                if (ch == 0x20) {
                    printf("No, really, otters are the bestest!\n\r");
                }
            }
            diff = nexttime - rdcycles_32();
        } while (diff > 0);
        nexttime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 20);
        //----------------------------------------------------------------------------------------------

        update_frame(framenum++);
    }
}
