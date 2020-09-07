#include <stdint.h>
#include <string.h>
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

//    static const uint32_t bitmap[] = {
//        0b11100000000000000111111111111111,
//        0b11000000000000000011111111111111,
//        0b10000000011000000001111111111111,
//        0b10000000011000000001111111111111,
//        0b10000000011000000001111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b00000000011000000000111111111111,
//        0b10000000000000000001111111111111,
//        0b11100000000000000111111111111111,
//    };


static void
update_frame(int framenum)
{
    static int init = 0;
    static struct framebuf *frames[2];
    
    struct framebuf *address;
    int x, y, ystart;
    int offset;

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

    static const uint32_t bitmap[] = {
        0b00000000000000000000111111111111,
        0b00000000000000000000111111111111,
        0b00000000000000000000111111111111,
        0b00000000000000000000111111111111,
        0b00000001111110000000111111111111,
        0b01000011111111000010111111111111,
        0b01000100111100100010111111111111,
        0b00111000111100011100111111111111,
        0b00111001111110011100111111111111,
        0b00011101111110111000111111111111,
        0b00011101111110111000111111111111,
        0b00001101111110110000111111111111,
        0b00000111111111100000111111111111,
        0b00000001111110000000111111111111,
        0b00000000000000000000111111111111,
        0b00000000000000000000111111111111,
    };
        
    address = frames[framenum & 1];

    ystart = 0;
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 10; x++) {
            if (bitmap[y] & (0x80000000 >> x)) address->data[x + ystart] = colours[(offset + (x>>1) + (y>>1)) % 6];
            else                               address->data[x + ystart] = 0;
            if (bitmap[y] & (0x00001000 << x)) address->data[19 - x + ystart] = colours[(offset + (x>>1) + (y>>1)) % 6];
            else                               address->data[19 - x + ystart] = 0;
        }
        ystart += DISPLAY_HWIDTH;
    }
    framebuf_render(address);
}

void main(void) {
    int frame_countdown = 1000;
    int framenum = 0;

    // And finally - the main loop.
    while (1) {
        if (frame_countdown-- == 0) {
            frame_countdown = 20000;
            
            update_frame(framenum++);
        }

        if (SERIAL->isr & SERIAL_INT_RXDATA_READY) {
            uint8_t ch = SERIAL->rxfifo;

            _putchar(ch);
        }
    }
}
