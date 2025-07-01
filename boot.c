#include <stdint.h>
#include "vm_pages.h"

extern void kernel_main(void);
extern uint64_t __bss_start;
extern uint64_t __bss_end;

static void clear_bss(void) {
    uint64_t* bss_start = &__bss_start;
    uint64_t* bss_end = &__bss_end;
    
    while (bss_start < bss_end) {
        *bss_start++ = 0;
    }
}

static void invalidate_caches(void) {
    __asm__ volatile(
        "ic iallu\n"
        "dsb sy\n"
        "isb\n"
        ::: "memory"
    );
}

void setup_mmu(void) {
    uint64_t* ttbr0_l1 = (uint64_t*)0x1000;
    uint64_t* ttbr0_l2 = (uint64_t*)0x2000;
    uint64_t* ttbr0_l3 = (uint64_t*)0x3000;
    
    for (int i = 0; i < 512; i++) {
        ttbr0_l1[i] = 0;
        ttbr0_l2[i] = 0;
        ttbr0_l3[i] = 0;
    }
    
    ttbr0_l1[0] = (uint64_t)ttbr0_l2 | 0x3;
    ttbr0_l2[0] = (uint64_t)ttbr0_l3 | 0x3;
    
    for (int i = 0; i < 512; i++) {
        uint64_t addr = i * 0x1000;
        uint64_t flags = 0x403;
        
        if (addr < 0x200000) {
            flags |= 0x8000000000000000ULL;
        }
        
        ttbr0_l3[i] = addr | flags;
    }
    
    __asm__ volatile("dsb sy");
    
    uint64_t tcr = (16ULL << 0) |
                   (16ULL << 16) |
                   (1ULL << 8) |
                   (1ULL << 24) |
                   (2ULL << 12) |
                   (2ULL << 28);
    
    uint64_t mair = (0x00ULL << 0) |
                    (0x44ULL << 8) |
                    (0xFFULL << 16);
    
    __asm__ volatile("msr mair_el1, %0" :: "r"(mair));
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(ttbr0_l1));
    __asm__ volatile("msr ttbr1_el1, %0" :: "r"(0));
    
    invalidate_caches();
    
    __asm__ volatile(
        "tlbi vmalle1\n"
        "dsb sy\n"
        "isb\n"
        ::: "memory"
    );
    
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= (1 << 0) |
             (1 << 2) |
             (1 << 12);
    sctlr &= ~((1 << 1) | (1 << 19));
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    __asm__ volatile("isb");
}

void boot_main(void) {
    clear_bss();
    setup_mmu();
    set_page_table_base(0x1000);
    kernel_main();
    
    while(1) {
        __asm__ volatile("wfi");
    }
}
