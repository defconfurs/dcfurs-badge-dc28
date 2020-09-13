MEMORY
{
    ROM  (rwx) : ORIGIN = 0x00000000, LENGTH = 4K
    SRAM (rwx) : ORIGIN = 0x10000000, LENGTH = 4K
    FLASH (rx) : ORIGIN = 0x30100000, LENGTH = 64K
}

ENTRY(_entry)

SECTIONS
{
    /* Place read only data, and constants in FLASH */
    .flash : {
        . = ALIGN(4);
        _sflash = .;
        KEEP(*(.header))
        *(.rodata)      /* .rodata sections (constants, strings, etc) */
        *(.rodata*)     /* .rodata sections (constants, strings, etc) */
        *(.srodata)     /* .rodata sections (constants, strings, etc) */
        *(.srodata*)    /* .rodata sections (constants, strings, etc) */
        _eflash = .;

        /* Begin relocated data */
        . = ALIGN(4);
        _sreltext = .;
    } > FLASH

    /* Store the program code in flash to be relocated. */
    .text : AT( _sreltext ) {
        . = ALIGN(4);
        _stext = .;
        *(.entry)           /* BIOS entry point */
        *(.text)            /* .text sections (code) */
        *(.text*)           /* .text sections (code) */
        . = ALIGN(4);
        _etext = .;
    } > ROM
    _sreltext_offset = _sreltext - ORIGIN(FLASH);

    /* Store initialized .data in ROM to be relocated. */
    _sreldata = _sreltext + _etext - ORIGIN(ROM);
    .data : AT( _sreldata ) {
        _sdata = .;         /* Mark the start of initialized data */
        . = ALIGN(4);
        *(.data)            /* .data sections (static and global variables) */
        *(.data*)           /* .data sections (static and global variables) */
        *(.sdata)           /* .data sections (static and global variables) */
        *(.sdata*)          /* .data sections (static and global variables) */
        . = ALIGN(4);
        _edata = .;         /* Mark the end of initialized data */
    } > SRAM
    _sreldata_offset = _sreldata - ORIGIN(FLASH);

    /* Uninitialized data sections to be zero-filled */
    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    /* Initial stack should start at the end of SRAM. */
    _stack_start = ORIGIN(SRAM) + LENGTH(SRAM) - 4;
}
