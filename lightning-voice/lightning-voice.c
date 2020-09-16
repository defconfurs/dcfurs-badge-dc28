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

static uint8_t depth[20][14];

static void recursive_branch(int x, int y, int max) {
    int value = depth[x][y];
    int branch = 0;
    int newdir;
    int tried = 0;
    int attempt;

    if (value >= max) return;
    
    if ((x & 1) ^ (y & 1)) {
        if      (x < 19 && depth[x+1][y] > value) { recursive_branch(x+1, y, max); branch = 1; }
        else if (y > 0  && depth[x][y-1] > value) { recursive_branch(x, y-1, max); branch = 1; }
        else if (y < 13 && depth[x][y+1] > value) { recursive_branch(x, y+1, max); branch = 1; }

        if (!branch) {
            for (attempt = 0; attempt < 8; attempt++) {
                // no possible directions
                if (tried == 7) break;
                
                // otherwise pick a direction and try it
                newdir = rand() & 0x7;
                if (x >= 10) {
                    switch (newdir) {
                    case 0: 
                        newdir = 0; break;
                    case 1:
                        newdir = 1; break;
                    default: newdir = 2;
                    }
                }
                else {
                    switch (newdir) {
                    case 0:
                    case 1:
                    case 2:
                        newdir = 0; break;
                    case 3:
                    case 4:
                    case 5:
                        newdir = 1; break;
                    default:
                        newdir = 2; break;
                    }
                }
                if (newdir == 0) {
                    if (y >= 13) break;
                    if (depth[x][y+1] == 0) {
                        depth[x][y+1] = value+1;
                        recursive_branch(x, y+1, max);
                        break;
                    }
                    else tried |= 1;
                }
                else if (newdir == 1) {
                    if (y <= 0) break;
                    if (depth[x][y-1] == 0) {
                        depth[x][y-1] = value+1;
                        recursive_branch(x, y-1, max);
                        break;
                    }
                    else tried |= 2;
                }
                else {
                    if (x >= 19) break;
                    if (depth[x+1][y] == 0) {
                        depth[x+1][y] = value+1;
                        recursive_branch(x+1, y, max);
                        break;
                    }
                    else tried |= 4;
                }
            }
        }
    }
    else {
        if      (x > 0  && depth[x-1][y] > value) { recursive_branch(x-1, y, max); branch = 1; }
        else if (y > 0  && depth[x][y-1] > value) { recursive_branch(x, y-1, max); branch = 1; }
        else if (y < 13 && depth[x][y+1] > value) { recursive_branch(x, y+1, max); branch = 1; }

        if (!branch) {
            for (attempt = 0; attempt < 8; attempt++) {
                // no possible directions
                if (tried == 7) break;

                // otherwise pick a direction and try it
                newdir = rand() & 0x7;
                if (x < 10) {
                    switch (newdir) {
                    case 0: 
                        newdir = 0; break;
                    case 1:
                        newdir = 1; break;
                    default: newdir = 2;
                    }
                }
                else {
                    switch (newdir) {
                    case 0:
                    case 1:
                    case 2:
                        newdir = 0; break;
                    case 3:
                    case 4:
                    case 5:
                        newdir = 1; break;
                    default:
                        newdir = 2; break;
                    }
                }
                if (newdir == 0) {
                    if (y >= 13) break;
                    if (depth[x][y+1] == 0) {
                        depth[x][y+1] = value+1;
                        recursive_branch(x, y+1, max);
                        break;
                    }
                    else tried |= 1;
                }
                else if (newdir == 1) {
                    if (y <= 0) break;
                    if (depth[x][y-1] == 0) {
                        depth[x][y-1] = value+1;
                        recursive_branch(x, y-1, max);
                        break;
                    }
                    else tried |= 2;
                }
                else {
                    if (x <= 0) break;
                    if (depth[x-1][y] == 0) {
                        depth[x-1][y] = value+1;
                        recursive_branch(x-1, y, max);
                        break;
                    }
                    else tried |= 4;
                }
            }
        }
    }
}

#define BRIGHTSTEP (16)

static void update_frame(int framenum, int audio, int max)
{
    static int init = 0;
    static struct framebuf *frames[2];
    
    struct framebuf *address;
    int x, y;
    int offset;
    uint16_t hue;
    uint8_t sat, val;
    static uint8_t value_map[20][14];
    
    // Allocate some frames
    if (!init) {
        frames[0] = framebuf_alloc();
        frames[1] = framebuf_alloc();
        init = 1;
        
        depth[ 8][7] = 1;
        depth[ 9][6] = 1;
        depth[ 9][7] = 1;
        depth[ 9][8] = 1;
        depth[10][6] = 1;
        depth[10][7] = 1;
        depth[10][8] = 1;
        depth[11][7] = 1;
    }
    
    address = frames[framenum & 1];
    
    // erase old paths
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 20; x++) {
            if (depth[x][y] > max) depth[x][y] = 0;
        }
    }

    // start two lightning bolts
    recursive_branch( 8, 6, max>>1);
    recursive_branch(11, 6, max>>1);

    // draw the lightning
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 20; x++) {
            if (depth[x][y] >= max-1) value_map[x][y] = 255;
            if (depth[x][y] != 0)     value_map[x][y] = 200;
            else                      value_map[x][y] = 0;
        }
    }

    int dx, dy;
    int mx, my;
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 20; x++) {
            if (value_map[x][y] < 200) {
                for (dy = -1; dy <= 1; dy++) {
                    for (dx = -3; dx <= 3; dx++) {
                        mx = x+dx;
                        my = y+dy;
                        if (mx >= 0 && mx < 20 && my >= 0 && my < 14 && value_map[mx][my] >= 200) {
                            if (value_map[x][y] < 189) value_map[x][y] += 15;
                        }
                    }
                }
            }
        }
    }

    // render output
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 20; x++) {
            if (value_map[x][y] < 200) {
                hue = 420 - (value_map[x][y]);
                sat = 255;
                val = value_map[x][y] >> 1;
            }
            else {
                hue = 220;
                sat = 128;
                val = 255;
            }
            address->data[x + (y<<5)] = hsv2pixel(hue, sat, val);
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

    printf("Hello Raiden\n\r");
    
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

        if (peak > 1) peak = peak - (peak >> 8) - 1;
        if (audio > peak) peak = audio;
        if (peak > 8000) peak = 8000;

        
        //----------------------------------------------------------------------------------------------
        // update the frames at a lower rate
        diff = nextframetime - rdcycles_32();
        if (diff < 0) {
            nextframetime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 20);

            int max = 1;
            if      (peak < 1000 ) max = 1;
            else if (peak < 2000 ) max = 6;
            else if (peak < 2400 ) max = 7;
            else if (peak < 2800 ) max = 9;
            else if (peak < 3000 ) max = 12;
            else if (peak < 3200 ) max = 15;
            else if (peak < 3600 ) max = 17;
            else if (peak < 4200 ) max = 19;
            else if (peak < 5000 ) max = 22;
            else                   max = 24;

            printf("%2d %d/%d\n\r", MISC->mic, peak, max);
            update_frame(framenum++, audio, max);
        }
    }
}
