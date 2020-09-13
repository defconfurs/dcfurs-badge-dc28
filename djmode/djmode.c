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
#define DJ_MODE_CIRCLES     3
#define DJ_MODE_STARFIELD   4
#define DJ_MODE_MAX         4

#define DJ_RATIO_ONE        16  /* Count the beat in 1/16th time. */
#define DJ_RATIO_STROBE     (DJ_RATIO_ONE * 2)
#define DJ_RATIO_DOTS       (DJ_RATIO_ONE * 2)
#define DJ_RATIO_CIRCLES    DJ_RATIO_ONE
#define DJ_RATIO_LINES      (DJ_RATIO_ONE / 8)
#define DJ_RATIO_STARFIELD  DJ_RATIO_ONE
/* Changes modes only on a multiple of 8 beats. */
#define DJ_RATIO_SWITCH     (DJ_RATIO_ONE * 8)

#define HUE_ANGLE_WHITE     -1
#define HUE_ANGLE_BLUE      180
#define HUE_ANGLE_PINK      300

struct djstate {
    unsigned long beat_counter;
    unsigned long beat_period;
    struct timeval beat_next;
    unsigned int dj_mode;

    unsigned int strobe_angle;
    unsigned int clear_next;
    unsigned int boops[4];
    unsigned int last_boop;
    unsigned int boop_index;
    unsigned int dj_max_mode;
    unsigned int dj_duration;
    unsigned int last_tilt_timeout;
    unsigned int last_circle_color;
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

/* Decay the pixel intensity. */
static void
draw_decay(struct djstate *state, unsigned int rate)
{
    int pix;
    for (pix = 0; pix < (DISPLAY_VRES * DISPLAY_HWIDTH); pix++) {
        if (state->brightness_grid[pix] > rate) {
            state->brightness_grid[pix] -= rate;
        } else {
            state->brightness_grid[pix] = 0;
        }
    }
}

/* Calculate the time since the last beat, scaled into the range 0-255. */
static unsigned int
beat_ramp(struct djstate *state, struct timeval *now, unsigned int ratio)
{
    unsigned long period = (state->beat_period / DJ_RATIO_ONE);
    unsigned int  beats = (state->beat_counter % ratio);
    unsigned long elapsed;
    elapsed = (beats * period) + period - timerdiff(&state->beat_next, now);
    return ((elapsed << 8) + elapsed) / (period * ratio);
}

static void
draw_strobe(struct djstate *state, struct timeval *now, int elapsed)
{
    unsigned int brightness = 0xff - beat_ramp(state, now, DJ_RATIO_STROBE);
    int i;
    
    /* Rotate the color by a degree on every frame draw */
    for (i = 0; i < (DISPLAY_HWIDTH * DISPLAY_VRES); i++) {
        state->angle_grid[i] = state->strobe_angle;
        state->brightness_grid[i] = brightness;
    }
    state->strobe_angle++;
    if (state->strobe_angle >= 360) state->strobe_angle = 0;
}

static void
draw_dots(struct djstate *state, struct timeval *now, int elapsed)
{
    int i;

    if (elapsed) {
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
}

static void
draw_circles(struct djstate *state, struct timeval *now, int elapsed)
{
    /* If it's not a beat, just decay */
    if (!elapsed) {
        draw_decay(state, 2);
        return;
    }

    /* On the beat, draw circle-ish-things */
    unsigned int rr5 = 10*10;
    unsigned int rr4 = 6*6;
    unsigned int rr3 = 4*4;
    unsigned int rr2 = 2*2;
    int16_t color = ((state->beat_counter >> DJ_RATIO_CIRCLES) & 1) ? HUE_ANGLE_BLUE : HUE_ANGLE_PINK;
    int x, y;

    for (y = 0; y < DISPLAY_VRES; y++) {
        int rbits = rand();
        int ydist = y - (DISPLAY_VRES/2) + (rbits & 0x7) - 3;
        int yy = ydist*ydist;

        for (x = 0; x < DISPLAY_HRES; x++) {
            int rbits = rand();
            int xdist = x - (DISPLAY_HRES/2) + (rbits & 0x7) - 3;
            int xx = xdist * xdist;
            int pix = x + (y * DISPLAY_HWIDTH);

            state->angle_grid[pix] = color;
            if ((xx+yy) <= rr2) state->brightness_grid[pix] = 0xff;
            else if ((xx+yy) <= rr3) state->brightness_grid[pix] = 0x7f;
            else if ((xx+yy) <= rr4) state->brightness_grid[pix] = 0x3f;
            else if ((xx+yy) <= rr5) state->brightness_grid[pix] = 0x0F;
            else state->brightness_grid[pix] = 0;
        }
    }
}

static void
draw_lines(struct djstate *state, struct timeval *now, int elapsed)
{
    /* If it's not a beat, just decay */
    if (!elapsed) {
        draw_decay(state, 5);
        return;
    }

    /* On the beat, draw a new random line. */
    int rbits = rand();
    int x, y;
    int xoff = DISPLAY_VRES/4;
    int16_t color = (rbits & 0x10) ? HUE_ANGLE_BLUE : HUE_ANGLE_PINK;
    int direction = (rbits & 0x0F);
    rbits >>= 5;

    /* Horizontal lines */
    if (direction < 5) {
        y = rbits % DISPLAY_VRES;
        for (x = 0; x < DISPLAY_HRES; x++) {
            state->angle_grid[y * DISPLAY_HWIDTH + x] = color;
            state->brightness_grid[y * DISPLAY_HWIDTH + x] = 0xff;
        }
        return;
    }
    /* Vertical lines come in two flavors thanks to the hex pattern. */
    x = xoff + (rbits % (DISPLAY_HRES - xoff*2));
    if (x&1) {
        /* Odd columns slope to the right */
        int ymod3 = 0;
        for (y = 0; y < DISPLAY_VRES; y++) {
            if (ymod3++ == 2) {
                x--;
                ymod3 = 0;
            }
            else {
                state->angle_grid[y * DISPLAY_HWIDTH + x] = color;
                state->brightness_grid[y * DISPLAY_HWIDTH + x] = 0xff;
            }
        }
    }
    else {
        /* Even columns slope to the left */
        int ymod3 = 0;
        for (y = 0; y < DISPLAY_VRES; y++) {
            if (ymod3++ == 2) {
                x++;
                ymod3 = 0;
            }
            else {
                state->angle_grid[y * DISPLAY_HWIDTH + x] = color;
                state->brightness_grid[y * DISPLAY_HWIDTH + x] = 0xff;
            }
        }
    }
}

static void
draw_stars(struct djstate *state, struct timeval *now, int elapsed)
{
    /* If it's not a beat, just decay */
    if (!elapsed) {
        draw_decay(state, 2);
        return;
    }

    /* On the beat, draw stars */
    int pix;
    unsigned int rbits = rand();
    unsigned int rbitmask = (unsigned int)-1;
    int16_t color = (rbits & 1) ? HUE_ANGLE_BLUE : HUE_ANGLE_PINK;
    
    for (pix = 0; pix < (DISPLAY_HWIDTH * DISPLAY_VRES); pix++) {
        /* Draw stars at random locations. */
        if ((rbits & 0x3) == 0) {
            state->angle_grid[pix] = color;
            state->brightness_grid[pix] = 0xff;
        }

        /* Refill the entropy. */
        rbits >>= 2;
        rbitmask >>= 2;
        if (!rbits) {
            rbits = rand();
            rbitmask = (unsigned int)-1;
        }
    }
}

static void
beat_increment(struct djstate *state)
{
    state->beat_next.tv_usec += state->beat_period / DJ_RATIO_ONE;
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
        .beat_counter = (unsigned)-1,
        .beat_period = 500000, /* 120 bpm */
        .dj_mode = DJ_MODE_STROBE,
        .dj_duration = 2000,

        .strobe_angle = 0,
        .clear_next = 0,
        .boops = {0},
        .last_boop = 0,
        .boop_index = 0,
        .dj_max_mode = 4,
        .last_tilt_timeout = 20,
        .last_circle_color = 0,
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
                elapsed = elapsed && (state.beat_counter % DJ_RATIO_STROBE) == 0;
                draw_strobe(&state, &now, elapsed);
                break;
            case DJ_MODE_DOTS:
                elapsed = elapsed && (state.beat_counter % DJ_RATIO_DOTS) == 0;
                draw_dots(&state, &now, elapsed);
                break;
            case DJ_MODE_CIRCLES:
                elapsed = elapsed && (state.beat_counter % DJ_RATIO_CIRCLES) == 0;
                draw_circles(&state, &now, elapsed);
                break;
            case DJ_MODE_LINES:
                elapsed = elapsed && (state.beat_counter % DJ_RATIO_LINES) == 0;
                draw_lines(&state, &now, elapsed);
                break;
            case DJ_MODE_STARFIELD:
                elapsed = elapsed && (state.beat_counter % DJ_RATIO_STARFIELD) == 0;
                draw_stars(&state, &now, elapsed);
                break;
        }
        render(&state);

        /* Copy the microphone data to the LEDPWM register */
        int audio = MISC->mic;
        MISC->leds[0] = ((audio < 0) ? -audio : audio) & 0xff;
        MISC->leds[1] = 0;
        MISC->leds[2] = 0;

        /* Switch the states every so often, but only on multiples of 8 beats. */
        if (state.dj_duration) state.dj_duration--;
        else if ((state.beat_counter % DJ_RATIO_SWITCH) == 0) {
            state.dj_duration = 2000;
            state.dj_mode++;
            if (state.dj_mode > DJ_MODE_MAX) state.dj_mode = 0;
            printf("Switching DJ mode to %d\n", state.dj_mode);
        }
        
        /* TODO: This timekeeping sucks */
        usleep(10000);
    }
}