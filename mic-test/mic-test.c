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

/* Pre-generated frames */
extern const struct framebuf voice_0_png;
extern const struct framebuf voice_1_png;
extern const struct framebuf voice_2_png;
extern const struct framebuf voice_3_png;
extern const struct framebuf voice_4_png;
extern const struct framebuf voice_5_png;
extern const struct framebuf voice_6_png;
extern const struct framebuf voice_7_png;
extern const struct framebuf voice_8_png;

static void
update_frame_audio(int average, int peak)
{
    (void) average;

    if      (peak <  500 ) framebuf_render(&voice_0_png);
    else if (peak < 1000 ) framebuf_render(&voice_1_png);
    else if (peak < 1300 ) framebuf_render(&voice_2_png);
    else if (peak < 1400 ) framebuf_render(&voice_2_png);
    else if (peak < 1500 ) framebuf_render(&voice_3_png);
    else if (peak < 1600 ) framebuf_render(&voice_4_png);
    else if (peak < 1700 ) framebuf_render(&voice_5_png);
    else if (peak < 1900 ) framebuf_render(&voice_6_png);
    else if (peak < 2200 ) framebuf_render(&voice_7_png);
    else                   framebuf_render(&voice_8_png);
}

void main(void)
{
    int frame_countdown = 1000;
    int peak = 0;
    int audio_abs = 0;
    int long_average = 0;
    //int audio_average = 0;
    int audio = 0;
    uint32_t nexttime = 0;
    signed int diff;

    printf("Starting Mic Test\n");
    
    // And finally - the main loop.
    while (1) {
        //----------------------------------------------------------------------------------------------
        // try to get some form of deterministic timing... kinda
        do { diff = nexttime - rdcycles_32(); } while (diff > 0);
        nexttime = rdcycles_32() + (CORE_CLOCK_FREQUENCY / 8000);
        //----------------------------------------------------------------------------------------------
        
        audio_abs = MISC->mic;
        if (audio_abs < 0) audio_abs = -audio_abs;
        
        long_average = long_average - (long_average>>5) + audio_abs;
        //audio_average = audio_average - (audio_average >> 4) + (audio_abs - (long_average>>8));
        audio = (audio_abs << 5) - (long_average);
        if (audio < 0) audio = 0;

        if (peak > 4096) peak = 4096;
        if (peak > 1) peak--;
        if (audio > peak) peak = audio;

        frame_countdown++;
        if (frame_countdown % 80 == 0) {
            update_frame_audio(audio_abs, peak);
        }
        if (frame_countdown % 800 == 0) {
            printf("%d - %d   %d %d\n\r", audio_abs, peak, MISC->button, MISC->i_status);
        }

        /* If there are characters received, echo them back. */
        if (SERIAL->isr & SERIAL_INT_RXDATA_READY) {
            uint8_t ch = SERIAL->rxfifo;

            _putchar(ch);

            if (ch == 0x20) {
                printf("%d - %d\n\r", audio_abs, peak);
            }
        }
    }
}
