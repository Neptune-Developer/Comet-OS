
ENTRY(_start)

SECTIONS
{
    . = 0x80000;

    .text : {
        *(.text.boot)
        *(.text)
    } > text

    .rodata : ALIGN(4096) {
        *(.rodata)
    } > rodata

    .data : ALIGN(4096) {
        *(.data)
    } > data

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    } > bss

    /DISCARD/ : {
        *(.comment)
        *(.ARM.exidx)
    }
}

MEMORY
{
    text (rx) : ORIGIN = 0x80000, LENGTH = 16M
    rodata (r) : ORIGIN = 0x810000, LENGTH = 1M
    data (rw) : ORIGIN = 0x820000, LENGTH = 1M
    bss (rw) : ORIGIN = 0x830000, LENGTH = 1M
}


