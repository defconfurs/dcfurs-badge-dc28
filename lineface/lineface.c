#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "badge.h"

static inline uint32_t rdcycles_32(void)
{
    register uint32_t cyc_lo;
    asm volatile("csrr %[lo],  mcycle" : [lo]"=r"(cyc_lo));
    return cyc_lo;
}

static uint8_t brightness[20][14];
static uint8_t colour[20][14];

static struct framebuf *frames[2];

static int modulator;

#define MOD_RATE (100)
#define DECAY (4)

static void update_frame(int framenum, int audio, int max) {
    struct framebuf *address;
    int x, y;
    int i;
    uint16_t hue;
    uint8_t sat, val;
    int pos;
    int angle;
    int ymod3 = 0;
    int e;
    
    address = frames[framenum & 1];
    
    modulator += max;
    if (modulator > 0) {
        modulator -= MOD_RATE;

        do { angle = rand() & 0xF; } while (angle >= 6);
        pos = rand() & 0xF;
        pos = ((pos * pos) >> 4);
        if (rand()&1) pos = -pos;
        hue = rand() & 0xFF;

        x = 10;
        y = 6;

        switch(angle) {
        case 0:
            y = y+pos;
            if (y < 0)   break;
            if (y >= 14) break;
            for (x = 0; x < 20; x++) {
                colour[x][y  ] = hue; brightness[x][y  ] = 255;
            }
            break;

        case 1:
            x = x+pos;
            if (x < 0)  break;
            if (x >= 20) break;
            for (y = 0; y < 14; y++) {
                colour[x][y] = hue;
                brightness[x][y] = 255;
            }
            break;
            
        case 2:
            x = x+pos - 2;
            e = x&1;
            if (x < -4)  break;
            if (x >= 16) break;
            for (y = 0; y < 14; y++) {
                if (ymod3 == e) x++;
                else if (x >= 0 && y >= 0 && x < 20 && y < 14) {
                        colour[x][y] = hue; brightness[x][y] = 255;
                }
                if (++ymod3 > 2) ymod3 = 0;
            }
            break;

        case 3:
            x = x+pos + 2;
            e = x&1;
            if (x < 4)  break;
            if (x >= 24) break;
            for (y = 0; y < 14; y++) {
                if (ymod3 == 1-e) x--;
                else if (x >= 0 && y >= 0 && x < 20 && y < 14) {
                        colour[x][y] = hue; brightness[x][y] = 255;
                }
                if (++ymod3 > 2) ymod3 = 0;
            }
            break;
            
        case 4:
            x = x+pos - 6;
            e = x&1;
            if (x < 4)  break;
            if (x >= 24) break;
            for (y = 0; y < 14; y++) {
                if (x >= 0 && y >= 0 && x < 20 && y < 14) { colour[x][y] = hue; brightness[x][y] = 255; }
                x++;
                if (x >= 0 && y >= 0 && x < 20 && y < 14) { colour[x][y] = hue; brightness[x][y] = 255; }
                if (x&1 && !y&1) x++;
            }
            break;

        case 5:
            x = x+pos + 6;
            e = x&1;
            if (x < 4)  break;
            if (x >= 24) break;
            for (y = 0; y < 14; y++) {
                if (x >= 0 && y >= 0 && x < 20 && y < 14) { colour[x][y] = hue; brightness[x][y] = 255; }
                x--;
                if (x >= 0 && y >= 0 && x < 20 && y < 14) { colour[x][y] = hue; brightness[x][y] = 255; }
                if (x&1 && y&1) x--;
            }
            break;
        }
    }
    
    
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 20; x++) {            
            address->data[x + (y<<5)] = hsv2pixel(colour[x][y], brightness[x][y], brightness[x][y]);

            if (brightness[x][y] > DECAY) brightness[x][y] -= DECAY;
            else                          brightness[x][y] = 0;
        }
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
        
    printf("Dude, you have a line on your face... or two\n\r");
    
    while (1) {
        //----------------------------------------------------------------------------------------------
        // try to get some form of deterministic timing... kinda
        do { diff = nexttime - rdcycles_32(); } while (diff > 0);
        nexttime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 8000);
        //----------------------------------------------------------------------------------------------

        audio_abs = MISC->mic;
        if (audio_abs < 0) audio_abs = -audio_abs;
        
        //long_average = long_average - (long_average>>5) + audio_abs;
        //audio_average = audio_average - (audio_average >> 4) + (audio_abs - (long_average>>8));
        audio = (audio_abs << 6);// - (long_average<<1);
        if (audio < 0) audio = 0;

        if (peak > 1) peak = peak - (peak >> 10) - 1;
        if (audio > peak) peak = audio;
        if (peak > 8000) peak = 8000;

        
        //----------------------------------------------------------------------------------------------
        // update the frames at a lower rate
        diff = nextframetime - rdcycles_32();
        if (diff < 0) {
            nextframetime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 60);

            int max = 1;
            if      (peak < 1000 ) max = 1;
            else if (peak < 2000 ) max = 6;
            else if (peak < 2400 ) max = 7;
            else if (peak < 2800 ) max = 10;
            else if (peak < 3000 ) max = 15;
            else if (peak < 3200 ) max = 20;
            else if (peak < 3600 ) max = 30;
            else if (peak < 4200 ) max = 50;
            else if (peak < 5000 ) max = 75;
            else                   max = 100;

            printf("%2d %d/%d\n\r", MISC->mic, peak, max);
            update_frame(framenum++, audio, max);
        }
    }
}
