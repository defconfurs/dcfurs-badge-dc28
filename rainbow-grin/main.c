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
    while ((SERIAL->isr & 0x02) == 0) { /* nop */}
    SERIAL->thr = ch;
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


static void update_frame(int framenum) {
    int x, y;
    int address;
    int offset;

    offset = framenum >> 2;

    static const uint16_t colours[] = {
        0xF800,
        0xF300,
        0xF5E0,
        0x07C0,
        0x001F,
        0x7817,
        0xFFFF
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
        
    
    if (framenum & 1) address = 0x40020004;
    else              address = 0x40020404;
    
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 10; x++) {
            if (bitmap[y] & (0x80000000 >> x)) *(uint16_t*)(address + ((  x )<<1) + (y<<6)) = colours[(offset + (x>>1) + (y>>1)) % 6];
            else                               *(uint16_t*)(address + ((  x )<<1) + (y<<6)) = 0;
            if (bitmap[y] & (0x00001000 << x)) *(uint16_t*)(address + ((19-x)<<1) + (y<<6)) = colours[(offset + (x>>1) + (y>>1)) % 6];
            else                               *(uint16_t*)(address + ((19-x)<<1) + (y<<6)) = 0;
        }
    }
    *(uint16_t*)(0x40020000) = address & 0x7FFF;
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

        if (SERIAL->isr & 0x01) {
            uint8_t ch = SERIAL->rhr;

            _putchar(ch);
        }

        if (MISC->button != 3) {
            int delay_count;
            for (delay_count = 0; delay_count < 1000; delay_count++) asm volatile("nop");
            if      (MISC->button == 2 && ANIM_NUM > 0)   ANIM_NUM--;
            else if (MISC->button == 1 && ANIM_NUM < 100) ANIM_NUM++;
            printf("moving to animation: %d\n\r", ANIM_NUM);
            bootload(ANIM_NUM);
        }
    }
}
