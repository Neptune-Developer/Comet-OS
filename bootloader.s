.section .text.bootloader
.globl _bootloader_start

_bootloader_start:
    mov x20, x0
    mov x21, x1
    mov x22, x2
    mov x23, x3

    mrs x0, currentel
    lsr x0, x0, #2
    cmp x0, #3
    b.eq el3_entry
    cmp x0, #2
    b.eq el2_entry
    b el1_entry

el3_entry:
    msr scr_el3, xzr
    
    ldr x0, =0x3c9
    msr spsr_el3, x0
    adr x0, el2_entry
    msr elr_el3, x0
    eret

el2_entry:
    msr cptr_el2, xzr
    msr hstr_el2, xzr
    
    mov x0, #(1 << 31)
    msr hcr_el2, x0
    
    mrs x0, cnthctl_el2
    orr x0, x0, #3
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr
    
    mov x0, #0x3c5
    msr spsr_el2, x0
    adr x0, el1_entry
    msr elr_el2, x0
    eret

el1_entry:
    mrs x1, mpidr_el1
    and x1, x1, #3
    cbnz x1, halt_secondary
    
    ldr x1, =bootloader_stack_top
    mov sp, x1
    
    ldr x0, =kernel_load_addr
    ldr x1, =kernel_size
    bl load_kernel
    
    mov x0, x20
    mov x1, x21  
    mov x2, x22
    mov x3, x23
    
    ldr x4, =kernel_entry_point
    br x4

load_kernel:
    ldr x2, =kernel_blob_start
    mov x3, #0
copy_loop:
    cmp x3, x1
    b.ge copy_done
    ldr x4, [x2, x3]
    str x4, [x0, x3]
    add x3, x3, #8
    b copy_loop
copy_done:
    ret

halt_secondary:
    wfi
    b halt_secondary

.section .data
kernel_load_addr:
    .quad 0x80000
kernel_entry_point:
    .quad 0x80000
kernel_size:
    .quad 0x100000  // 1mb max

.section .rodata
kernel_blob_start:
    .incbin "kernel.bin"

.section .bss
.align 16
bootloader_stack_bottom:
    .space 8192
bootloader_stack_top:
