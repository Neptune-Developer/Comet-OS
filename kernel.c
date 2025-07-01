#include <stdint.h>
#include <stddef.h>
#include "vm_pages.h"

#define UART_BASE 0x09000000
#define UART_DR   (UART_BASE + 0x00)
#define UART_FR   (UART_BASE + 0x18)
#define UART_IBRD (UART_BASE + 0x24)
#define UART_FBRD (UART_BASE + 0x28)
#define UART_LCRH (UART_BASE + 0x2C)
#define UART_CR   (UART_BASE + 0x30)

#define FB_BASE 0xA0000000
#define FB_WIDTH 1024
#define FB_HEIGHT 768
#define FB_PITCH (FB_WIDTH * 4)

static void mmio_write(uint64_t addr, uint32_t value) {
    *(volatile uint32_t*)addr = value;
}

static uint32_t mmio_read(uint64_t addr) {
    return *(volatile uint32_t*)addr;
}

static void uart_init(void) {
    uint64_t uart_page = alloc_page();
    if (uart_page) {
        vm_map(UART_BASE, uart_page, PROT_READ | PROT_WRITE);
        
        mmio_write(UART_CR, 0);
        mmio_write(UART_IBRD, 26);
        mmio_write(UART_FBRD, 3);
        mmio_write(UART_LCRH, 0x70);
        mmio_write(UART_CR, 0x301);
    }
}

static void uart_putchar(char c) {
    while (mmio_read(UART_FR) & (1 << 5));
    mmio_write(UART_DR, c);
}

static void uart_puts(const char* str) {
    while (*str) {
        if (*str == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(*str++);
    }
}

static uint8_t font_8x16[][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3E,0x63,0x60,0x60,0x60,0x60,0x60,0x60,0x63,0x3E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3E,0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x3E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x63,0x63,0x63,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7F,0x60,0x60,0x60,0x7E,0x60,0x60,0x60,0x60,0x7F,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3E,0x63,0x60,0x30,0x1E,0x03,0x03,0x06,0x63,0x3E,0x00,0x00,0x00,0x00}
};

static int char_to_font_index(char c) {
    switch(c) {
        case 'C': return 1;
        case 'O': return 2;
        case 'M': return 3;
        case 'E': return 4;
        case 'T': return 5;
        case 'S': return 6;
        case ' ': return 0;
        default: return 0;
    }
}

static void fb_init(void) {
    for (uint64_t offset = 0; offset < FB_WIDTH * FB_HEIGHT * 4; offset += PAGE_SIZE) {
        uint64_t fb_page = alloc_page();
        if (fb_page) {
            vm_map(FB_BASE + offset, fb_page, PROT_READ | PROT_WRITE);
        }
    }
}

static void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= FB_WIDTH || y >= FB_HEIGHT) return;
    
    volatile uint32_t* fb = (volatile uint32_t*)FB_BASE;
    fb[y * FB_WIDTH + x] = color;
}

static void fb_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    int font_idx = char_to_font_index(c);
    uint8_t* glyph = font_8x16[font_idx];
    
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            uint32_t color = (glyph[row] & (1 << (7 - col))) ? fg : bg;
            fb_putpixel(x + col, y + row, color);
        }
    }
}

static void fb_puts(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    uint32_t cur_x = x;
    while (*str) {
        fb_putchar(*str, cur_x, y, fg, bg);
        cur_x += 8;
        str++;
    }
}

static void fb_clear(uint32_t color) {
    volatile uint32_t* fb = (volatile uint32_t*)FB_BASE;
    for (uint32_t i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb[i] = color;
    }
}

static void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        asm volatile("nop");
    }
}

void kernel_main(void) {
    vm_init();
    set_page_table_base(0x1000);
    
    uart_init();
    uart_puts("Comet OS - ARM64 Kernel Starting\n");
    
    fb_init();
    fb_clear(0x001122);
    
    uart_puts("Framebuffer initialized\n");
    
    fb_puts("Comet OS", 100, 100, 0x00FFFF, 0x001122);
    
    uart_puts("Display output complete\n");
    uart_puts("Free pages: ");
    
    uint64_t free = get_free_pages();
    char num_str[32];
    int i = 0;
    if (free == 0) {
        num_str[i++] = '0';
    } else {
        while (free > 0) {
            num_str[i++] = '0' + (free % 10);
            free /= 10;
        }
        for (int j = 0; j < i / 2; j++) {
            char temp = num_str[j];
            num_str[j] = num_str[i - 1 - j];
            num_str[i - 1 - j] = temp;
        }
    }
    num_str[i] = '\0';
    uart_puts(num_str);
    uart_puts("\n");
    
    uart_puts("Comet OS - Boot Complete\n");
    
    while(1) {
        asm volatile("wfi");
    }
}
