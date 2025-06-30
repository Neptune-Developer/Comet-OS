#include <stdint.h>
#include <stddef.h>
#include "vm_pages.h"

#define MEMORY_LOCATION 0x100000

void kernel_main(void) {
    vm_init();
    
    uint64_t page = alloc_page();
    vm_map(0x200000, page, PROT_READ | PROT_WRITE);
    
    volatile char* mem = (volatile char*)0x200000;
    mem[0] = 'H';
    mem[1] = 'I';
    mem[2] = '\0';
    
    while(1) {
        __asm__ volatile("hlt");
    }
}
