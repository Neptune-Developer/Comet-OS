#include <stdint.h>
#include "vm_pages.h"

extern void kernel_main(void);
extern uint64_t __bss_start;
extern uint64_t __bss_end;

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
        ttbr0_l3[i] = (i * 0x1000) | 0x3;
    }
    
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(ttbr0_l1));
    __asm__ volatile("msr ttbr1_el1, %0" :: "r"(ttbr0_l1));
    
    uint64_t tcr = (16ULL << 0) | (16ULL << 16) | (1ULL << 8) | (1ULL << 24);
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr));
    
    uint64_t mair = 0x44;
    __asm__ volatile("msr mair_el1, %0" :: "r"(mair));
    
    __asm__ volatile("isb");
    
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= 1;
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr));
    __asm__ volatile("isb");
}

void boot_main(void) {
    setup_mmu();
    kernel_main();
}
