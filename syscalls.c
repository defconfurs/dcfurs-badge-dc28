#include <errno.h>
#include <sys/time.h>
#include "badge.h"

static unsigned long long
clock_rdcycle(void)
{
    register uint32_t cyc_lo;
    register uint32_t cyc_hi;
    register uint32_t cyc_temp;
    asm volatile(
    "__clock_rdcycle_retry:"
    "    csrr %[hi],  mcycleh   \n"
    "    csrr %[lo],  mcycle    \n"
    "    csrr %[tmp], mcycleh   \n"
    "    bne  %[hi], %[tmp], __clock_rdcycle_retry\n" 
    : [lo]"=r"(cyc_lo), [hi]"=r"(cyc_hi), [tmp]"=r"(cyc_temp));
    return (unsigned long long)cyc_hi << 32 | cyc_lo;
}

int
clock_gettime(clockid_t clkid, struct timespec *ts)
{
    if ((clkid == CLOCK_REALTIME) || (clkid == CLOCK_MONOTONIC)) {
        unsigned long long cycles = clock_rdcycle();
        ts->tv_sec = cycles / CORE_CLOCK_FREQUENCY;
        ts->tv_nsec = (cycles % CORE_CLOCK_FREQUENCY) * 1000000000 / CORE_CLOCK_FREQUENCY;
        return 0;
    }
    errno = EINVAL;
    return -1;
}

/* Delay for a desired number of nanoseconds */
int
nanosleep(const struct timespec *delay, struct timespec *rem)
{
    unsigned long long start = clock_rdcycle();
    unsigned long long end = start;
    signed long long diff;

    end += (unsigned long long)delay->tv_sec * CORE_CLOCK_FREQUENCY;
    end += (unsigned long long)delay->tv_nsec * CORE_CLOCK_FREQUENCY / 1000000000;
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    
    /* Wait for the cycle count to reach the end target. */
    do { diff = end - clock_rdcycle(); } while (diff > 0);
    return 0;
}

/* Delay for a desired number of microseconds */
int
usleep(useconds_t usec)
{
    unsigned long long start = clock_rdcycle();
    unsigned long long end = start + (unsigned long long)usec * CORE_CLOCK_FREQUENCY / 1000000;
    signed long long diff;

    /* Wait for the cycle count to reach the end target. */
    do { diff = end - clock_rdcycle(); } while (diff > 0);
    return 0;
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
