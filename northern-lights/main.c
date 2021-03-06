#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "badge.h"

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

/* Sine table, scaled for unsigned bytes, generated by:
import math
for i in range(0,256,8):
   print("   ", end='')
   for j in range(i,i+8):
       val = (math.sin(j * math.pi / 128) + 1) * 127.5
       print(" 0x%02x," % int(val), end='')
   print("")
*/
const uint8_t sin_table[256] = {
    0x7f, 0x82, 0x85, 0x88, 0x8b, 0x8f, 0x92, 0x95,
    0x98, 0x9b, 0x9e, 0xa1, 0xa4, 0xa7, 0xaa, 0xad,
    0xb0, 0xb3, 0xb6, 0xb8, 0xbb, 0xbe, 0xc1, 0xc3,
    0xc6, 0xc8, 0xcb, 0xcd, 0xd0, 0xd2, 0xd5, 0xd7,
    0xd9, 0xdb, 0xdd, 0xe0, 0xe2, 0xe4, 0xe5, 0xe7,
    0xe9, 0xeb, 0xec, 0xee, 0xef, 0xf1, 0xf2, 0xf4,
    0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfb,
    0xfc, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
    0xff, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfd, 0xfd,
    0xfc, 0xfb, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6,
    0xf5, 0xf4, 0xf2, 0xf1, 0xef, 0xee, 0xec, 0xeb,
    0xe9, 0xe7, 0xe5, 0xe4, 0xe2, 0xe0, 0xdd, 0xdb,
    0xd9, 0xd7, 0xd5, 0xd2, 0xd0, 0xcd, 0xcb, 0xc8,
    0xc6, 0xc3, 0xc1, 0xbe, 0xbb, 0xb8, 0xb6, 0xb3,
    0xb0, 0xad, 0xaa, 0xa7, 0xa4, 0xa1, 0x9e, 0x9b,
    0x98, 0x95, 0x92, 0x8f, 0x8b, 0x88, 0x85, 0x82,
    0x7f, 0x7c, 0x79, 0x76, 0x73, 0x6f, 0x6c, 0x69,
    0x66, 0x63, 0x60, 0x5d, 0x5a, 0x57, 0x54, 0x51,
    0x4e, 0x4b, 0x48, 0x46, 0x43, 0x40, 0x3d, 0x3b,
    0x38, 0x36, 0x33, 0x31, 0x2e, 0x2c, 0x29, 0x27,
    0x25, 0x23, 0x21, 0x1e, 0x1c, 0x1a, 0x19, 0x17,
    0x15, 0x13, 0x12, 0x10, 0x0f, 0x0d, 0x0c, 0x0a,
    0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x03,
    0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0c, 0x0d, 0x0f, 0x10, 0x12, 0x13,
    0x15, 0x17, 0x19, 0x1a, 0x1c, 0x1e, 0x21, 0x23,
    0x25, 0x27, 0x29, 0x2c, 0x2e, 0x31, 0x33, 0x36,
    0x38, 0x3b, 0x3d, 0x40, 0x43, 0x46, 0x48, 0x4b,
    0x4e, 0x51, 0x54, 0x57, 0x5a, 0x5d, 0x60, 0x63,
    0x66, 0x69, 0x6c, 0x6f, 0x73, 0x76, 0x79, 0x7c,
};

static uint8_t fp_sin(unsigned int val)
{
    return sin_table[val & 0xff];
}

void main(void)
{
    struct framebuf *frame;

    /* Pick some constants */
    unsigned int rscale = 17;
    unsigned int gscale = 19;
    unsigned int bscale = 29;

    unsigned int rstart = 555;
    unsigned int gstart = 666;
    unsigned int bstart = 777;

    int x, y;

    printf("Starting Northern Lights\n");
            
    /* Allocate a frame buffer */
    frame = framebuf_alloc();
    
    do {
        /* Redraw the frame */
        unsigned int xrval = rstart;
        unsigned int xgval = gstart;
        unsigned int xbval = bstart;
        for (x = 0; x < DISPLAY_HRES; x++) {
            unsigned int yrval = rstart;
            unsigned int ygval = gstart;
            unsigned int ybval = bstart;

            for (y = 0; y < DISPLAY_VRES; y++) {
                unsigned int red   = (fp_sin(fp_sin((xrval) >> 4)) * fp_sin(fp_sin((yrval) >> 4))) >> 8;
                unsigned int green = (fp_sin(fp_sin((xgval) >> 4)) * fp_sin(fp_sin((ygval) >> 4))) >> 8;
                unsigned int blue  = (fp_sin(fp_sin((xbval) >> 4)) * fp_sin(fp_sin((ybval) >> 4))) >> 8;
                frame->data[x + y * DISPLAY_HWIDTH] = ((red & 0xF8) << 8) + ((green & 0xFC) << 3) + ((blue & 0xF8) >> 3);

                yrval += (rscale * 2);
                ygval -= (gscale * 2);
                ybval += (bscale * 2);
            }

            xrval += rscale;
            xgval += gscale;
            xbval -= bscale;
        }
        framebuf_render(frame);

        /* Increment the animation */
        rstart += 29;
        gstart += 23;
        bstart += 19;

        /* Wait for a bit (125ms) */
        usleep(125000);
    } while(1);

    return;
}
