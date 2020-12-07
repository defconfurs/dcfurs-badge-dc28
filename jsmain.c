#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "badge.h"

/* Provided by the JSON-to-animation compiler. */
extern const struct frame_schedule schedule[];
extern const char *json_name;

int
main(void)
{
    const struct frame_schedule *state = schedule;

    printf("Starting %s Animation\n", json_name);

    for (;;) {
        /* Render the frame for the desired interval */
        framebuf_render(state->frame);
        usleep(state->interval);

        /* Jump to the next frame. */
        state++;
        if (state->frame == NULL) {
            state = schedule;
        }
    }
}