.section .text.boot
.global _bootloader_start

_bootloader_start:
    mrs x0, mpidr_el1
    and x0, x0, #3
    cbnz x0, park_core

    mov x0, #0x80000
    mov sp, x0

    mrs x0, currentel
    and x0, x0, #12
    cmp x0, #12
    b.eq el3_entry
    cmp x0, #8
    b.eq el2_entry
    b el1_entry

el3_entry:
    mov x0, #0b01001
    msr scr_el3, x0

    mov x0, #0x30d00800
    msr sctlr_el2, x0

    mov x0, #0x80000000
    msr hcr_el2, x0

    mov x0, #0x33ff
    msr cptr_el2, x0

    msr hstr_el2, xzr

    mov x0, #0x00000000
    msr cnthctl_el2, x0

    msr cntvoff_el2, xzr

    mov x0, #0x045
    msr spsr_el3, x0

    adr x0, el2_entry
    msr elr_el3, x0

    eret

el2_entry:
    mrs x0, cnthctl_el2
    orr x0, x0, #3
    msr cnthctl_el2, x0

    msr cntvoff_el2, xzr

    mov x0, #0x80000000
    msr hcr_el2, x0

    mov x0, #0x0800
    movk x0, #0x30d0, lsl #16
    msr sctlr_el1, x0

    mov x0, #0x00000000
    msr spsr_el2, x0

    mov x0, #0x005
    msr spsr_el2, x0

    adr x0, el1_entry
    msr elr_el2, x0

    eret

el1_entry:
    ldr x0, =kernel_base
    br x0

park_core:
    wfi
    b park_core

load_kernel:
    ldr x0, =0x80000
    ldr x1, =kernel_image
    ldr x2, =kernel_size
    cbz x2, kernel_loaded

copy_loop:
    ldr x3, [x1], #8
    str x3, [x0], #8
    subs x2, x2, #8
    b.gt copy_loop

kernel_loaded:
    ret

.section .data
kernel_base:
    .quad 0x80000

kernel_image:
    .incbin "kernel.bin"
kernel_image_end:

kernel_size:
    .quad kernel_image_end - kernel_image

.section .rodata
boot_message:
    .ascii "ARM64 Bootloader v1.0\n\0"

.section .bss
.align 3
stack_bottom:
    .space 0x4000
stack_top:
