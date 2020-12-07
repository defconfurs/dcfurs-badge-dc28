#include <stdint.h>
#include <stdio.h>
#include <string.h>
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


static struct framebuf *frames[2];

static void
update_frame(int framenum, int level)
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
    int nexttime = rdcycles_32() + 500;
    int nextframetime = rdcycles_32() + 1000;
    int framenum = 0;
    signed int diff;

    int peak = 0;
    int audio_abs = 0;
    int long_average = 0;
    int audio = 0;
    int updatecount;

    frames[0] = framebuf_alloc();
    frames[1] = framebuf_alloc();
        
    while (1) {
        //----------------------------------------------------------------------------------------------
        // try to get some form of deterministic timing... kinda
        do { diff = nexttime - rdcycles_32(); } while (diff > 0);
        nexttime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 8000);
        //----------------------------------------------------------------------------------------------

        audio_abs = MISC->mic;
        if (audio_abs < 0) audio_abs = -audio_abs;
        
        audio = (audio_abs << 6);
        if (audio < 0) audio = 0;
        if (audio > 8000) audio = 8000;
        
        if (peak > 1) peak = peak - (peak >> 10) - 1;
        if (audio > peak) peak = audio;

        //----------------------------------------------------------------------------------------------
        // update the frames at a lower rate
        diff = nextframetime - rdcycles_32();
        if (diff < 0) {
            nextframetime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 20);

            int level = 0;
            if      (peak < 1000 ) level = 0;
            else if (peak < 2000 ) level = 1;
            else if (peak < 2400 ) level = 2;
            else if (peak < 2800 ) level = 3;
            else if (peak < 3000 ) level = 4;
            else if (peak < 3200 ) level = 5;
            else if (peak < 3600 ) level = 6;
            else if (peak < 4200 ) level = 7;
            else if (peak < 5000 ) level = 8;
            else                   level = 9;

            update_frame(framenum++, level);
        }
    }
}
