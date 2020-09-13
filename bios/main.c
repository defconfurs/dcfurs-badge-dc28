#include <stdint.h>
#include <string.h>
#include <string.h>
#include <setjmp.h>

#include "badge.h"

void _putchar(char ch)
{
    uint32_t status;

    /* Cook line endings to play nicely with terminals. */
    static char prev = '\0';
    if ((ch == '\n') && (prev != '\r')) _putchar('\r');
    prev = ch;

    /* Output the character */
    do {
        status = SERIAL->isr;
        if ((status & SERIAL_INT_DTR_ACTIVE) == 0) return; /* USB Host is not connected */
    } while ((status & SERIAL_INT_TXFIFO_EMPTY) == 0);
    
    SERIAL->txfifo = ch;
}

/* Use setjmp/longjmp when returning from an animation. */
static jmp_buf bootjmp;

static const struct boot_header *
bootaddr(int slot)
{
    const uintptr_t bootsz = 64 * 1024;
    uintptr_t userdata = 0x30000000 + (1024 * 1024);    /* User data starts 1MB into flash. */
    uintptr_t animation = userdata + ((slot+1) * bootsz);   /* Animations are spaced 64kB apart. */
    return (const struct boot_header *)animation;
}

void bootexit(int code)
{
    longjmp(bootjmp, code);
}

void bootload(int slot)
{
    const uintptr_t bootsz = 64 * 1024;
    uintptr_t prog_target = 0x20000000;
    uintptr_t frame_target = 0x40020000;
    const struct boot_header *hdr = bootaddr(slot);

    /* For the animation to be valid - the tag must match, and the entry point should be sane */
    if (hdr->tag != BOOT_HDR_TAG) {
        bios_printf("Bad animation found at slot %d\n", slot);
        bios_printf("\ttag = 0x%08x, start=%d, size = %d\n", hdr->tag, hdr->data_start, hdr->data_size);
        bootexit(1);
    }

    /* Copy the animation program into SPRAM. */
    memcpy((void *)prog_target,  (uint8_t *)hdr + hdr->data_start,  hdr->data_size);

    /* Copy the frame data into the framebuffer */
    memcpy((void *)frame_target, (uint8_t *)hdr + hdr->frame_start, hdr->frame_size);
    
    /* Execute it */
    asm volatile(
        "jalr %[target] \n" /* Jump to the target address */
        "j _entry       \n" /* We should never get here, reboot if we do. */
        :: [target]"r"(hdr->entry));
}

static int
check_bootslot(int slot)
{
    const uintptr_t bootsz = 64 * 1024;
    uintptr_t target = 0x20000000;
    const struct boot_header *hdr = bootaddr(slot);

    /* For the animation to be valid - the tag must match, and the entry point should be sane */
    if (hdr->tag != BOOT_HDR_TAG) return 0;
    if ((hdr->data_start + hdr->data_size) > bootsz) return 0;
    if ((hdr->frame_start + hdr->frame_size) > bootsz) return 0;
    if (hdr->entry < target) return 0;
    if (hdr->entry > target + hdr->data_size) return 0;

    /* Otherwise, the animation is valid */
    return 1;
}

int main(void)
{
    int slot;
    int count;

    MISC->leds[0] = 0;
    MISC->leds[1] = 255;
    MISC->leds[3] = 0;

    /* Enable interrupts */
    MISC->i_status = 0xF;
    MISC->i_enable = 0xA;
    SERIAL->ier = SERIAL_INT_RXDATA_READY;

    /* Count the number of animations present. */
    count = 0;
    while (check_bootslot(count)) count++;

    /* Run animations forever. */
    slot = 0;
    for (;;) {
        int retcode = setjmp(bootjmp);
        if (retcode == 0) {
            /* Boot the next animation */
            MISC->leds[0] = 0;
            MISC->leds[1] = 1;
            MISC->leds[3] = 0;
            bootload(slot);
        }

        bios_printf("Animation at slot %d exitied with code=%d\n", slot, retcode);
        if (retcode == 1) {
            /* Seek the animation forward. */
            if (slot < count-1) slot++;
            else slot = 0;
        }
        else {
            /* Seek the animation backawrd. */
            if (slot != 0) slot--;
            else slot = count - 1;
        }
    }
}
