#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

extern void fb_print(const char* str, uint32_t fg, uint32_t bg);
extern void fb_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg);
extern void fb_newline(void);

static char* itoa(int value, char* buffer, int base) {
    char* p = buffer;
    char* p1, *p2;
    unsigned long ud = value;
    int divisor = 10;
    
    if (base == 16) {
        divisor = 16;
    } else if (base == 8) {
        divisor = 8;
    } else if (base == 2) {
        divisor = 2;
    }
    
    if (base == 10 && value < 0) {
        *p++ = '-';
        buffer++;
        ud = -value;
    }
    
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);
    
    *p = 0;
    
    p1 = buffer;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
    
    return buffer;
}

static char* utoa(unsigned int value, char* buffer, int base) {
    char* p = buffer;
    char* p1, *p2;
    unsigned long ud = value;
    int divisor = 10;
    
    if (base == 16) {
        divisor = 16;
    } else if (base == 8) {
        divisor = 8;
    } else if (base == 2) {
        divisor = 2;
    }
    
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);
    
    *p = 0;
    
    p1 = buffer;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
    
    return buffer;
}

static char* ltoa(long value, char* buffer, int base) {
    char* p = buffer;
    char* p1, *p2;
    unsigned long ud = value;
    int divisor = 10;
    
    if (base == 16) {
        divisor = 16;
    } else if (base == 8) {
        divisor = 8;
    } else if (base == 2) {
        divisor = 2;
    }
    
    if (base == 10 && value < 0) {
        *p++ = '-';
        buffer++;
        ud = -value;
    }
    
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);
    
    *p = 0;
    
    p1 = buffer;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
    
    return buffer;
}

static char* ultoa(unsigned long value, char* buffer, int base) {
    char* p = buffer;
    char* p1, *p2;
    unsigned long ud = value;
    int divisor = 10;
    
    if (base == 16) {
        divisor = 16;
    } else if (base == 8) {
        divisor = 8;
    } else if (base == 2) {
        divisor = 2;
    }
    
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);
    
    *p = 0;
    
    p1 = buffer;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
    
    return buffer;
}

static int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static void print_padding(char pad_char, int count) {
    for (int i = 0; i < count; i++) {
        char c = pad_char;
        fb_print(&c, 0xFFFFFF, 0x000000);
    }
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[64];
    const char* p = format;
    
    while (*p) {
        if (*p == '%') {
            p++;
            int width = 0;
            int precision = -1;
            int left_align = 0;
            int zero_pad = 0;
            int long_flag = 0;
            int unsigned_flag = 0;
            
            if (*p == '-') {
                left_align = 1;
                p++;
            }
            
            if (*p == '0') {
                zero_pad = 1;
                p++;
            }
            
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }
            
            if (*p == '.') {
                p++;
                precision = 0;
                while (*p >= '0' && *p <= '9') {
                    precision = precision * 10 + (*p - '0');
                    p++;
                }
            }
            
            if (*p == 'l') {
                long_flag = 1;
                p++;
            }
            
            switch (*p) {
                case 'd':
                case 'i': {
                    int value = long_flag ? va_arg(args, long) : va_arg(args, int);
                    itoa(value, buffer, 10);
                    int len = strlen(buffer);
                    if (!left_align && width > len) {
                        print_padding(zero_pad ? '0' : ' ', width - len);
                    }
                    fb_print(buffer, 0xFFFFFF, 0x000000);
                    if (left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    break;
                }
                case 'u': {
                    unsigned int value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
                    utoa(value, buffer, 10);
                    int len = strlen(buffer);
                    if (!left_align && width > len) {
                        print_padding(zero_pad ? '0' : ' ', width - len);
                    }
                    fb_print(buffer, 0xFFFFFF, 0x000000);
                    if (left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    break;
                }
                case 'x': {
                    unsigned int value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
                    utoa(value, buffer, 16);
                    int len = strlen(buffer);
                    if (!left_align && width > len) {
                        print_padding(zero_pad ? '0' : ' ', width - len);
                    }
                    fb_print(buffer, 0xFFFFFF, 0x000000);
                    if (left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    break;
                }
                case 'X': {
                    unsigned int value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
                    utoa(value, buffer, 16);
                    for (int i = 0; buffer[i]; i++) {
                        if (buffer[i] >= 'a' && buffer[i] <= 'f') {
                            buffer[i] = buffer[i] - 'a' + 'A';
                        }
                    }
                    int len = strlen(buffer);
                    if (!left_align && width > len) {
                        print_padding(zero_pad ? '0' : ' ', width - len);
                    }
                    fb_print(buffer, 0xFFFFFF, 0x000000);
                    if (left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    break;
                }
                case 'o': {
                    unsigned int value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
                    utoa(value, buffer, 8);
                    int len = strlen(buffer);
                    if (!left_align && width > len) {
                        print_padding(zero_pad ? '0' : ' ', width - len);
                    }
                    fb_print(buffer, 0xFFFFFF, 0x000000);
                    if (left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    break;
                }
                case 'b': {
                    unsigned int value = long_flag ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
                    utoa(value, buffer, 2);
                    int len = strlen(buffer);
                    if (!left_align && width > len) {
                        print_padding(zero_pad ? '0' : ' ', width - len);
                    }
                    fb_print(buffer, 0xFFFFFF, 0x000000);
                    if (left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    break;
                }
                case 'p': {
                    void* ptr = va_arg(args, void*);
                    fb_print("0x", 0xFFFFFF, 0x000000);
                    ultoa((unsigned long)ptr, buffer, 16);
                    fb_print(buffer, 0xFFFFFF, 0x000000);
                    break;
                }
                case 'c': {
                    char c = va_arg(args, int);
                    char str[2] = {c, 0};
                    fb_print(str, 0xFFFFFF, 0x000000);
                    break;
                }
                case 's': {
                    char* str = va_arg(args, char*);
                    if (!str) str = "(null)";
                    int len = strlen(str);
                    if (precision >= 0 && len > precision) {
                        len = precision;
                    }
                    if (!left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    for (int i = 0; i < len; i++) {
                        char c = str[i];
                        fb_print(&c, 0xFFFFFF, 0x000000);
                    }
                    if (left_align && width > len) {
                        print_padding(' ', width - len);
                    }
                    break;
                }
                case 'n': {
                    int* ptr = va_arg(args, int*);
                    *ptr = 0;
                    break;
                }
                case '%': {
                    char c = '%';
                    fb_print(&c, 0xFFFFFF, 0x000000);
                    break;
                }
                default: {
                    char c = *p;
                    fb_print(&c, 0xFFFFFF, 0x000000);
                    break;
                }
            }
        } else {
            if (*p == '\n') {
                fb_newline();
            } else {
                char c = *p;
                fb_print(&c, 0xFFFFFF, 0x000000);
            }
        }
        p++;
    }
    
    va_end(args);
}
