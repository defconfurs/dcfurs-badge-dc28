#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "badge.h"

#define DJ_MODE_STROBE      0
#define DJ_MODE_DOTS        1
#define DJ_MODE_LINES       2
#define DJ_MODE_STARFIELD   3
#define DJ_MODE_CIRCLES     4

#define HUE_ANGLE_WHITE     -1
#define HUE_ANGLE_BLUE      180
#define HUE_ANGLE_PINK      300

struct djstate {
    unsigned long beat_counter;
    unsigned long beat_period;
    struct timeval beat_next;
    unsigned int dj_mode;

    unsigned int interval;
    unsigned int rows;
    unsigned int columns;
    unsigned int throb_x;
    unsigned int throb_y;
    unsigned int strobe_angle;
    unsigned int pink_angle;
    unsigned int blue_angle;
    uint32_t     last_beat;
    unsigned int clear_next;
    unsigned int ticks_per_beat;
    unsigned int boops[4];
    unsigned int last_boop;
    unsigned int boop_index;
    unsigned int dj_max_mode;
    unsigned int dj_duration;
    unsigned int last_tilt_timeout;
    unsigned int last_circle_color;
    unsigned int dj_ticks;
    unsigned int next_boop_timeout;

    int16_t angle_grid[DISPLAY_HWIDTH * DISPLAY_VRES];
    uint8_t brightness_grid[DISPLAY_HWIDTH * DISPLAY_VRES];
};

static signed long
timerdiff(struct timeval *a, struct timeval *b)
{
    signed long high = (a->tv_sec - b->tv_sec) * 1000000;
    return high + a->tv_usec - b->tv_usec;
}

static void
render(struct djstate *state)
{
    static int init = 0;
    static int index = 0;
    static struct framebuf *frames[2];

    struct framebuf *address;
    int x, y;

    if (!init) {
        frames[0] = framebuf_alloc();
        frames[1] = framebuf_alloc();
        init = 1;
    }

    /* Render the frame */
    address = frames[index++ & 1];
    for (x = 0; x < DISPLAY_HRES; x++) {
        for (y = 0; y < DISPLAY_VRES; y++) {
            unsigned int i = x + (y * DISPLAY_HWIDTH);
            unsigned int val = state->brightness_grid[i];
            if (state->angle_grid[i] < 0) {
                /* Use negative to mean white */
                address->data[i] = ((val & 0xF8) << 8) + ((val & 0xFC) << 3) + ((val & 0xF8) >> 3);
            }
            else {
                /* Otherwise, convert from HSV coordinates */
                address->data[i] = hsv2pixel(state->angle_grid[i], 0xff, val);
            }
        }
    }
    framebuf_render(address);
}

static void
draw_strobe(struct djstate *state, struct timeval *now, int elapsed)
{
    unsigned long diff;
    unsigned int brightness;
    int i;

    /* Set the brightness to decay after every other beat */
    diff = timerdiff(&state->beat_next, now);
    diff += state->beat_period * (state->beat_counter & 1);
    brightness = (diff << 8) / (2 * state->beat_period);
    
    /* Rotate the color by a degree on every frame draw */
    for (i = 0; i < (DISPLAY_HWIDTH * DISPLAY_VRES); i++) {
        state->angle_grid[i] = state->strobe_angle;
        state->brightness_grid[i] = brightness;
    }
    state->strobe_angle++;
    if (state->strobe_angle >= 360) state->strobe_angle = 0;

    /* Draw it */
    render(state);
}

static void
draw_dots(struct djstate *state, struct timeval *now, int elapsed)
{
    int i;

    /* On every other beat, make a big change */
    if (elapsed && (state->beat_counter & 1)) {
        unsigned int rbits = rand();
        unsigned int rval = rbits & 0x3;
        int16_t color;
        
        /* Reset the color to a random choice */
        switch (rbits & 0x7) {
            case 0:
            case 1:
            case 2:
                color = HUE_ANGLE_BLUE;
                break;
            case 3:
            case 4:
            case 5:
                color = HUE_ANGLE_PINK;
                break;
            case 6:
            case 7:
            default:
                color = HUE_ANGLE_WHITE;
                break;
        }
        for (i = 0; i < (DISPLAY_HWIDTH * DISPLAY_VRES); i++) {
            state->angle_grid[i] = color;
        }

        /* Reset the display to full intensity */
        memset(&state->brightness_grid, 0xff, sizeof(state->brightness_grid));
    }
    else {
        /* Decay the display intensity */
        for (i = 0; i < (DISPLAY_HWIDTH * DISPLAY_VRES); i++) {
            if (state->angle_grid[i] < 1) {
                state->brightness_grid[i] = 0xff;
            }
            else if (state->brightness_grid[i] >= 8) {
                state->brightness_grid[i] -= 8;
            } else {
                state->brightness_grid[i] = 0;
            }
        }

        /* Inject some random pixels */
        for (i = 0; i < 8; i++) {
            unsigned int rbits = rand();
            int pix = (rbits & 0x1FF);
            if (pix < (DISPLAY_HWIDTH * DISPLAY_VRES)) {
                state->angle_grid[pix] = (rbits & 0x200) ? HUE_ANGLE_BLUE : HUE_ANGLE_PINK;
                state->brightness_grid[pix] = (rbits >> 10) & 0xff;
            }
        }
    }

    /* Draw it */
    render(state);
}

static void
beat_increment(struct djstate *state)
{
    state->beat_next.tv_usec += state->beat_period;
    if (state->beat_next.tv_usec > 1000000) {
        state->beat_next.tv_usec -= 1000000;
        state->beat_next.tv_sec++;
    }
    state->beat_counter++;
}

int
main(void)
{
    struct djstate state = {
        .beat_counter = 0,
        .beat_period = 1000000, /* 60 bpm */
        .dj_mode = DJ_MODE_DOTS,
        .dj_duration = 25000,

        .interval = 10,
        .rows = DISPLAY_VRES,
        .columns = DISPLAY_VRES,
        .throb_x = 9,
        .throb_y = 2,
        .strobe_angle = 0,
        .pink_angle = 300,
        .blue_angle = 180,
        .last_beat = 0,
        .clear_next = 0,
        .ticks_per_beat = 800,
        .boops = {0},
        .last_boop = 0,
        .boop_index = 0,
        .dj_max_mode = 4,
        .dj_duration = 25000,
        .last_tilt_timeout = 20,
        .last_circle_color = 0,
        .dj_ticks = 0,
        .next_boop_timeout = 1500
    };

    /* Seed the PRNG */
    register uint32_t seed;
    asm volatile ("csrr %0, mcycle" : "=r"(seed));
    srand(seed);

    printf("Starting DJ Mode\n");

    /* Set the beat period. */
    gettimeofday(&state.beat_next, NULL);
    beat_increment(&state);

    while (1) {
        /* Timekeep the beat */
        int elapsed = 0;
        struct timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec < state.beat_next.tv_sec) elapsed = 0;
        else if (now.tv_sec > state.beat_next.tv_sec) elapsed = 1;
        else if (now.tv_usec > state.beat_next.tv_usec) elapsed = 1;
        if (elapsed) beat_increment(&state);

        /* Draw the next frame */
        switch (state.dj_mode) {
            case DJ_MODE_STROBE:
                draw_strobe(&state, &now, elapsed);
                break;
            case DJ_MODE_DOTS:
                draw_dots(&state, &now, elapsed);
                break;
        }
        /* Copy the microphone data to the LEDPWM register */
        //int audio = MISC->mic;
        //MISC->leds[0] = ((audio < 0) ? -audio : audio) & 0xff;
        //MISC->leds[1] = 0;
        //MISC->leds[2] = 0;

        /* Switch the states every so often. */
        if (state.dj_duration) state.dj_duration--;
        else {
            state.dj_duration = 25000;
            state.dj_mode = (state.dj_mode + 1) & 1;
        }
        
        /* TODO: This timekeeping sucks */
        usleep(10000);
    }
}