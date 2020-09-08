#include <stdint.h>
#include <string.h>
#include <stdio.h>
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

    if      (peak <   80 ) framebuf_render(&voice_0_png);
    else if (peak <  200 ) framebuf_render(&voice_1_png);
    else if (peak <  500 ) framebuf_render(&voice_2_png);
    else if (peak <  800 ) framebuf_render(&voice_2_png);
    else if (peak < 1100 ) framebuf_render(&voice_3_png);
    else if (peak < 1400 ) framebuf_render(&voice_4_png);
    else if (peak < 1700 ) framebuf_render(&voice_5_png);
    else if (peak < 2000 ) framebuf_render(&voice_6_png);
    else if (peak < 2300 ) framebuf_render(&voice_7_png);
    else                  framebuf_render(&voice_8_png);
}

void main(void)
{
    int frame_countdown = 1000;
    int peak = 0;
    int audio_abs = 0;
    int long_average = 0;
    int audio_average = 0;
    int audio = 0;

    printf("Starting Mic Test\n");

    // And finally - the main loop.
    while (1) {
        audio_abs = *(int32_t*)(0x40000010);
        if (audio_abs < 0) audio_abs = -audio_abs;
        
        long_average = long_average - (long_average>>8) + audio_abs;
        audio_average = audio_average - (audio_average >> 4) + (audio_abs - (long_average>>8));
        audio = (audio_abs - (long_average>>8)) << 10;
        if (audio < 0) audio = 0;

        //if (peak > 4096) peak = 0;
        if (peak > 1) peak--;
        if (audio > peak) peak = audio;
        
        if (frame_countdown-- == 0) {
            frame_countdown = 10000;
            update_frame_audio(audio_abs, peak);
            printf("%d - %d\n\r", audio_abs, peak);
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
