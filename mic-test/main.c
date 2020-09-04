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

static void update_frame_audio(int average, int peak) {
    (void) average;
    int address;
    address = 0x40020084;
    if      (peak <   80 ) address += 0x400*15; // 0
    else if (peak <  200 ) address += 0x400*16; // 1
    else if (peak <  500 ) address += 0x400*17; // 2
    else if (peak <  800 ) address += 0x400*18; // 3
    else if (peak < 1100 ) address += 0x400*19; // 4
    else if (peak < 1500 ) address += 0x400*20; // 5
    else if (peak < 1900 ) address += 0x400*21; // 6
    else if (peak < 2200 ) address += 0x400*22; // 7
    else if (peak < 2800 ) address += 0x400*23; // 8
    else                   address += 0x400*23; // 9
    
    *(uint16_t*)(0x40020000) = address & 0x7FFF;
}    

extern uint32_t *_binary_frames_bin_start;
extern uint32_t *_binary_frames_bin_end;

void main(void)
{
    int frame_countdown = 1000;
    int peak;
    int audio_abs;
    int audio_average;
    int long_average;

    printf("Starting mic-test\n\r");
    
    // TODO: need to update this for the proper location
    memcpy((uint32_t*)(0x40020004), (void*)(_binary_frames_bin_start), _binary_frames_bin_end - _binary_frames_bin_start);

    printf("copied frames\n\r");

    // And finally - the main loop.
    while (1) {
        audio_average = audio_average - (audio_average>>4) + *(int32_t*)(0x40000010);
        long_average = long_average - (long_average>>8) + audio_average;
        audio_abs = audio_average - (long_average>>8);
        if (audio_abs < 0) audio_abs = -audio_abs;

        if (peak > 1) peak--;
        if (audio_abs > peak) peak = audio_abs;
        
        if (frame_countdown-- == 0) {
            frame_countdown = 20000;
            update_frame_audio(audio_abs, peak);
        }

        /* If there are characters received, echo them back. */
        if (SERIAL->isr & 0x01) {
            uint8_t ch = SERIAL->rhr;

            _putchar(ch);

            if (ch == 0x20) {
                printf("%d - %d\n\r", audio_abs, peak);
            }
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
