MEMORY
{
    ROM   (rx)  : ORIGIN = 0x00000000, LENGTH = 0x01000  /* 4kB */
    STACK (rw)  : ORIGIN = 0x10000000, LENGTH = 0x01000  /* 4kB */ 
    SPRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x10000  /* 64kB */
    FLASH (rwx) : ORIGIN = 0x30000000, LENGTH = 0x800000 /* 8MB */
}

ENTRY(entry)

SECTIONS
{
    /* Place program code directly into SPRAM */
    .text : {
        . = ALIGN(4);
        _stext = .;
        *(.entry)           /* initial code entry point */
        *(.text)            /* .text sections (code) */
        *(.text*)           /* .text sections (code) */
        *(.rodata)          /* .rodata sections (constants, strings, etc) */
        *(.rodata*)         /* .rodata sections (constants, strings, etc) */
        *(.srodata)         /* .rodata sections (constants, strings, etc) */
        *(.srodata*)        /* .rodata sections (constants, strings, etc) */
        _etext = .;

        . = ALIGN(4);
        _sdata = .;
        __global_pointer$ = . + 2048; /* global data pointer */
        *(.data)            /* .data sections (static and global variables) */
        *(.data*)           /* .data sections (static and global variables) */
        *(.sdata)           /* .data sections (static and global variables) */
        *(.sdata*)          /* .data sections (static and global variables) */
        _edata = .;

        . = ALIGN(4);
        _sbss = .;
        *(.bss)             /* .bss sections (zero-initialied variables) */
        *(.bss*)            /* .bss sections (zero-initialied variables) */
        *(.sbss)            /* .bss sections (zero-initialied variables) */
        *(.sbss*)           /* .bss sections (zero-initialied variables) */
        *(COMMON)
        _ebss = .;

        . = ALIGN(4);
        _end = .;
    } > SPRAM

    /* Initial stack should start at the end of SRAM. */
    _stack_start = ORIGIN(STACK) + LENGTH(STACK) - 8; /* added word at end for animation number */
}
