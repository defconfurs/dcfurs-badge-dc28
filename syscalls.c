#include <errno.h>
#include "badge.h"

void __attribute__((noreturn))
_exit(int code)
{
    VTABLE->bios_bootexit(code);
    while (1) { /* nop*/ }
}

/* Support a heap, if someone really wants to use it */
extern char _heap_start;
extern char _heap_end;
static char *_sbrk_top = &_heap_start;
void *
_sbrk(intptr_t diff)
{
    char *sbrk_prev = _sbrk_top;
    char *sbrk_next = _sbrk_top + diff;

    if ((sbrk_next > &_heap_end) || (sbrk_next < &_heap_start)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    _sbrk_top = sbrk_next;
    return sbrk_prev;
}

/* Animation Header Structure */
extern void _start();
extern const char _rel_data_offset;
extern const char _rel_data_length;
extern const char _rel_frame_offset;
extern const char _rel_frame_length;
const struct boot_header __attribute__((section(".header"))) _boot_header = {
    .tag = BOOT_HDR_TAG,
    .flags = 0, /* Reserved for future use */
    .entry = (uintptr_t)&_start,
    /* Loadable segment data. */
    .data_size = (uintptr_t)&_rel_data_length,
    .data_start = (uintptr_t)&_rel_data_offset,
    .frame_size = (uintptr_t)&_rel_frame_length,
    .frame_start = (uintptr_t)&_rel_frame_offset,
    /* TODO: Animation Name - will require some macro sillyness. */
};
