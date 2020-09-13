#include <stdlib.h>
#include "badge.h"

union framelist {
    struct framebuf buffer; /* When the block is in use by a user */
    union framelist *next;  /* When the block is stored in the free list */
};

/* Some Constants from the linker file. */
extern union framelist _frame_heap_start;
extern union framelist _frame_heap_end;

/* Highest-addressed frame that has been allocated so far */
static union framelist *frame_heap_top = &_frame_heap_start;

/* Linked list of unallocated frames. */
static union framelist *frame_free_list = NULL;

/* Display pointer register */
volatile uint32_t __attribute__((section(".display-pointer"))) framebuf_display_pointer = 0;

/* Simple fixed-size frame allocator */
struct framebuf *
framebuf_alloc(void)
{
    /* Allocate from the free list if there's anything in it. */
    if (frame_free_list) {
        union framelist *entry = frame_free_list;
        frame_free_list = frame_free_list->next;
        return &entry->buffer;
    }
    
    /* Get more memory from the heap, if the freelist is empty. */
    if ((frame_heap_top + 1) <= &_frame_heap_end) {
        union framelist *entry = frame_heap_top++;
        return &entry->buffer;
    }
    
    /* Otherwise, memory is full: there is no help, only fail. */
    return NULL;
}

/* Return frame buffers back to the freelist. */
void
framebuf_free(struct framebuf *frame)
{
    union framelist *entry = (union framelist *)frame;

    /* TODO: Sanity-check that the frame points into display memory? */
    if (frame) {
        entry->next = frame_free_list;
        frame_free_list = entry;
    }
}

/* Display a frame buffer */
void
framebuf_render(const struct framebuf *frame)
{
    /* TODO: Sanity-check that the frame points into display memory? */
    framebuf_display_pointer = (uintptr_t)frame & 0xFFFF;
}

/* Hopefully the fastest we can convert HSV->RGB */
static unsigned int
mod360(unsigned int n)
{
    unsigned int q;
    unsigned int r;
    unsigned int t;

    q = n - (n >> 2) - (n >> 5) - (n >> 7);
    q += (q >> 12);
    q += (q >> 24);
    q >>= 8;

    t = (q << 6) + (q << 3);
    r = n - (t << 2) - t;
    return (r > 359) ? (r - 360) : r;
}

static unsigned int
div60(unsigned int n)
{
    unsigned int q; /* quotient guess */
    unsigned int r; /* remainder */

    q = (n >> 1) + (n >> 5);
    q += (q >> 8);
    q += (q >> 16);
    q >>= 5;
    r = n - (q << 6) + (q << 2);
    return (r > 59) ? q : q+1;
}

#define hsv_slope(_val_, _angle_) div60((_val_) * (_angle_))

uint16_t
hsv2pixel(unsigned int hue, uint8_t sat, uint8_t val)
{
    uint8_t r = 0, g = 0, b = 0;
    uint8_t valsat = (val * sat + 0xff) >> 8;
    uint8_t white = (val - valsat);

hsv_try_again:
    if (hue < 60) {
        r = val;
        g = white + hsv_slope(valsat, hue);
        b = white;
    }
    else if (hue < 120) {
        r = white + hsv_slope(valsat, 120 - hue);
        g = val;
        b = white;
    }
    else if (hue < 180) {
        r = white;
        g = val;
        b = white + hsv_slope(valsat, hue - 120);
    }
    else if (hue < 240) {
        r = white;
        g = white + hsv_slope(valsat, 240 - hue);
        b = val;
    }
    else if (hue < 300) {
        r = white + hsv_slope(valsat, hue - 240);
        g = white;
        b = val;
    }
    else if (hue < 360) {
        r = val;
        g = white;
        b = white + hsv_slope(valsat, 360 - hue);
    }
    else {
        hue = mod360(hue);
        goto hsv_try_again;
    }
    return ((r & 0xF8) << 8) + ((g & 0xFC) << 3) + ((b & 0xF8) >> 3);
}
