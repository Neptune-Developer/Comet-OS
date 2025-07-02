.section .text.boot
.globl _start

_start:
    mrs x0, mpidr_el1
    and x0, x0, #3
    cbnz x0, halt

    ldr x0, =stack_top
    mov sp, x0

    ldr x0, =__bss_start
    ldr x1, =__bss_end
    sub x1, x1, x0
    cbz x1, skip_bss
clear_bss:
    str xzr, [x0], #8
    subs x1, x1, #8
    b.ne clear_bss

skip_bss:
    ldr x0, =page_table_l0
    msr ttbr0_el1, x0
    msr ttbr1_el1, x0

    ldr x0, =0x00000000804
    msr tcr_el1, x0

    ldr x0, =0x000000000044ff04
    msr mair_el1, x0

    mrs x0, sctlr_el1
    orr x0, x0, #1
    orr x0, x0, #(1 << 2)
    orr x0, x0, #(1 << 12)
    msr sctlr_el1, x0
    isb

    bl kernel_main

halt:
    wfi
    b halt

.section .data
.align 12
page_table_l0:
    .space 4096

.section .bss
.align 16
stack_bottom:
    .space 16384
stack_top:
