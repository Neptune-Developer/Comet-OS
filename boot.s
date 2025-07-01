.section .text.boot
.global _start

_start:
    mrs x0, mpidr_el1
    and x0, x0, #3
    cbnz x0, halt

    ldr x0, =__bss_start
    ldr x1, =__bss_end
clear_bss:
    cmp x0, x1
    b.ge bss_cleared
    str xzr, [x0], #8
    b clear_bss

bss_cleared:
    ldr x0, =0x8000
    mov sp, x0

    mrs x0, currentel
    and x0, x0, #12
    cmp x0, #8
    b.ne halt

    mrs x0, sctlr_el1
    bic x0, x0, #1
    msr sctlr_el1, x0

    ldr x0, =page_tables
    msr ttbr0_el1, x0
    msr ttbr1_el1, xzr

    mov x0, #0x44
    msr mair_el1, x0

    mov x0, #0x803510
    msr tcr_el1, x0

    isb

    bl setup_initial_page_tables

    mrs x0, sctlr_el1
    orr x0, x0, #1
    orr x0, x0, #4
    orr x0, x0, #0x1000
    msr sctlr_el1, x0
    isb

    bl boot_main

halt:
    wfi
    b halt

setup_initial_page_tables:
    ldr x0, =page_tables
    ldr x1, =page_tables_end
clear_tables:
    cmp x0, x1
    b.ge tables_cleared
    str xzr, [x0], #8
    b clear_tables

tables_cleared:
    ldr x0, =page_tables
    ldr x1, =page_tables
    add x1, x1, #0x1000
    orr x1, x1, #0x3
    str x1, [x0]

    ldr x0, =page_tables
    add x0, x0, #0x1000
    ldr x1, =page_tables
    add x1, x1, #0x2000
    orr x1, x1, #0x3
    str x1, [x0]

    ldr x0, =page_tables
    add x0, x0, #0x2000
    mov x1, #0
    mov x2, #512
map_loop:
    cbz x2, map_done
    orr x3, x1, #0x403
    cmp x1, #0x200000
    b.lt no_exec
    orr x3, x3, #0x8000000000000000
no_exec:
    str x3, [x0], #8
    add x1, x1, #0x1000
    sub x2, x2, #1
    b map_loop

map_done:
    tlbi vmalle1
    dsb sy
    isb
    ret

.section .data
.align 12
page_tables:
    .space 0x3000
page_tables_end:

.section .bss
.align 3
__bss_start:
.space 0x1000
__bss_end:
