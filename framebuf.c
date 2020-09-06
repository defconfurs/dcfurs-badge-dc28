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
framebuf_render(struct framebuf *frame)
{
    /* TODO: Sanity-check that the frame points into display memory? */
    framebuf_display_pointer = (uintptr_t)frame & 0xFFFF;
}
