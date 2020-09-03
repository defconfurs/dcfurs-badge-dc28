.section .entry
.globl entry
.type entry,@function

# Global symbols from the linker file.
.global _etext
.global _sdata
.global _edata
.global _sbss
.global _ebss
.global _heap_start
.global _stack_start
.global _start

entry:
    # initialize the register file
    addi x1, zero, 0
    la x2, _stack_start
    addi x3, zero, 0
    addi x4, zero, 0
    addi x5, zero, 0
    addi x6, zero, 0
    addi x7, zero, 0
    addi x8, zero, 0
    addi x9, zero, 0
    addi x10, zero, 0
    addi x11, zero, 0
    addi x12, zero, 0
    addi x13, zero, 0
    addi x14, zero, 0
    addi x15, zero, 0
    addi x16, zero, 0
    addi x17, zero, 0
    addi x18, zero, 0
    addi x19, zero, 0
    addi x20, zero, 0
    addi x21, zero, 0
    addi x22, zero, 0
    addi x23, zero, 0
    addi x24, zero, 0
    addi x25, zero, 0
    addi x26, zero, 0
    addi x27, zero, 0
    addi x28, zero, 0
    addi x29, zero, 0
    addi x30, zero, 0
    addi x31, zero, 0

    # Zero-fill the .bss section.
    la a0, _sbss
    la a1, _ebss
    beq a0, a1, bss_zfill_skip
bss_zfill_loop:
    sw x0, 0(a0)
    addi a0,a0,4
    bne a0, a1, bss_zfill_loop
bss_zfill_skip:

    # C-Runtime is ready to start executing.
    j _start
