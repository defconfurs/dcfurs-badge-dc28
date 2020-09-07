MEMORY
{
    ROM   (rx)     : ORIGIN = 0x00000000, LENGTH = 0x01000  /* 4kB */
    STACK (rwx)    : ORIGIN = 0x10000000, LENGTH = 0x01000  /* 4kB */ 
    SPRAM (rwx)    : ORIGIN = 0x20000000, LENGTH = 0x10000  /* 64kB */
    FRAMEBUF (rwx) : ORIGIN = 0x40020000, LENGTH = 0x10000  /* 64kB */
    FLASH (rwx)    : ORIGIN = 0x30000000, LENGTH = 0x800000 /* 8MB */
}

ENTRY(_start)

SECTIONS
{
    /* Place the animation header in flash. */
    .flash : {
        . = ALIGN(4);
        _sflash = .;
        KEEP(*(.header))
        _eflash = .;

        /* Relocate program code and frame data into flash */
        . = ALIGN(4);
        _rel_start = .;
    } > FLASH

    /* Place program code into SPRAM */
    .text : AT(_rel_start) {
        . = ALIGN(4);
        _stext = .;
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
        . = ALIGN(4);
        _edata = .;
    } > SPRAM

    /* Place zero-filled data into SPRAM */
    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)             /* .bss sections (zero-initialied variables) */
        *(.bss*)            /* .bss sections (zero-initialied variables) */
        *(.sbss)            /* .bss sections (zero-initialied variables) */
        *(.sbss*)           /* .bss sections (zero-initialied variables) */
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;

        . = ALIGN(4);
        _end = .;
    } > SPRAM

    /* Generate symbols for the heap */
    _heap_start = _ebss;
    _heap_end = ORIGIN(SPRAM) + LENGTH(SPRAM);

    /* Generate the size of relocated program data. */
    _rel_data_offset = _rel_start - ORIGIN(FLASH);
    _rel_data_length = _edata - ORIGIN(SPRAM);

    /* Place pre-rendered frames into the FRAMEBUF region. */
    .frames : AT(_rel_start + _rel_data_length) {
        . = ALIGN(4);   /* Skip the first word - it's reserved for the frame pointer */
        _sframedata = .;
        KEEP(*(.display-pointer))   /* Frame address register */
        *(.frames)                  /* Frame data to be preloaded  */
        . = ALIGN(4);
        _eframedata = .;
    } > FRAMEBUF

    /* Generate the size of relocated frame data. */
    _rel_frame_offset = _rel_start + _rel_data_length - ORIGIN(FLASH);
    _rel_frame_length = _eframedata - ORIGIN(FRAMEBUF);

    /* Generate the size and location of the frame heap */
    _frame_heap_start = _eframedata;
    _frame_heap_end   = ORIGIN(FRAMEBUF) + LENGTH(FRAMEBUF);
}
