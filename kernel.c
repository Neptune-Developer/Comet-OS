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

#define FB_INFO_ADDR 0x8000
#define DEFAULT_FB_BASE 0xA0000000
#define DEFAULT_FB_WIDTH 1024
#define DEFAULT_FB_HEIGHT 768

struct fb_info {
    uint64_t base_addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t size;
};

static struct fb_info fb;

static void mmio_write(uint64_t addr, uint32_t value) {
   *(volatile uint32_t*)addr = value;
}

static uint32_t mmio_read(uint64_t addr) {
   return *(volatile uint32_t*)addr;
}

static void uart_init(void) {
   vm_map(UART_BASE, UART_BASE, PROT_READ | PROT_WRITE);
   
   mmio_write(UART_CR, 0);
   mmio_write(UART_IBRD, 26);
   mmio_write(UART_FBRD, 3);
   mmio_write(UART_LCRH, 0x70);
   mmio_write(UART_CR, 0x301);
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

static void uart_puthex(uint64_t val) {
    char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putchar(hex[(val >> i) & 0xF]);
    }
}

static void uart_putdec(uint64_t val) {
    char num_str[32];
    int i = 0;
    if (val == 0) {
        uart_putchar('0');
        return;
    }
    while (val > 0) {
        num_str[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        uart_putchar(num_str[j]);
    }
}

static int detect_rpi_fb(void) {
    volatile uint32_t* mailbox = (volatile uint32_t*)0x3F00B880;
    static volatile uint32_t __attribute__((aligned(16))) msg[32];
    
    msg[0] = 32 * 4;
    msg[1] = 0;
    msg[2] = 0x40003;
    msg[3] = 8;
    msg[4] = 0;
    msg[5] = 1920;
    msg[6] = 1080;
    msg[7] = 0;
    
    while (mailbox[6] & 0x80000000);
    mailbox[8] = ((uint64_t)msg & ~0xF) | 8;
    while (!(mailbox[6] & 0x40000000));
    
    if (msg[1] == 0x80000000 && msg[5] > 0 && msg[6] > 0) {
        fb.width = msg[5];
        fb.height = msg[6];
        return 0;
    }
    return -1;
}

static int detect_uefi_fb(void) {
    volatile uint64_t* gop_base = (volatile uint64_t*)0x10000;
    if (*gop_base != 0) {
        fb.base_addr = *gop_base;
        fb.width = *(uint32_t*)(gop_base + 1);
        fb.height = *(uint32_t*)(gop_base + 2);
        if (fb.width > 0 && fb.height > 0) {
            return 0;
        }
    }
    return -1;
}

static int fb_detect(void) {
    fb.bpp = 32;
    
    if (detect_rpi_fb() == 0) {
        fb.base_addr = 0x3C000000;
        fb.pitch = fb.width * 4;
        fb.size = fb.pitch * fb.height;
        uart_puts("rpi fb: ");
        uart_putdec(fb.width);
        uart_puts("x");
        uart_putdec(fb.height);
        uart_puts("\n");
        return 0;
    }
    
    if (detect_uefi_fb() == 0) {
        fb.pitch = fb.width * 4;
        fb.size = fb.pitch * fb.height;
        uart_puts("uefi fb: ");
        uart_putdec(fb.width);
        uart_puts("x");
        uart_putdec(fb.height);
        uart_puts(" @ ");
        uart_puthex(fb.base_addr);
        uart_puts("\n");
        return 0;
    }
    
    fb.base_addr = DEFAULT_FB_BASE;
    fb.width = DEFAULT_FB_WIDTH;
    fb.height = DEFAULT_FB_HEIGHT;
    fb.pitch = fb.width * 4;
    fb.size = fb.pitch * fb.height;
    
    uart_puts("fallback fb: ");
    uart_putdec(fb.width);
    uart_puts("x");
    uart_putdec(fb.height);
    uart_puts("\n");
    
    return -1;
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
   for (uint64_t offset = 0; offset < fb.size; offset += PAGE_SIZE) {
       vm_map(fb.base_addr + offset, fb.base_addr + offset, PROT_READ | PROT_WRITE);
   }
}

static void fb_putpixel(uint32_t x, uint32_t y, uint32_t color) {
   if (x >= fb.width || y >= fb.height) return;
   
   volatile uint32_t* fbptr = (volatile uint32_t*)fb.base_addr;
   fbptr[y * fb.width + x] = color;
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
   volatile uint32_t* fbptr = (volatile uint32_t*)fb.base_addr;
   for (uint32_t i = 0; i < fb.width * fb.height; i++) {
       fbptr[i] = color;
   }
}

void kernel_main(void) {
   vm_init();
   set_page_table_base(0x1000);
   
   uart_init();
   uart_puts("Comet OS - ARM64 Kernel Starting\n");
   
   fb_detect();
   fb_init();
   fb_clear(0x001122);
   
   uart_puts("Framebuffer initialized\n");
   
   fb_puts("Comet OS", 100, 100, 0x00FFFF, 0x001122);
   
   uart_puts("Display output complete\n");
   uart_puts("Free pages: ");
   
   uint64_t free = get_free_pages();
   uart_putdec(free);
   uart_puts("\n");
   
   uart_puts("Comet OS - Boot Complete\n");
   
   while(1) {
       asm volatile("wfi");
   }
}
