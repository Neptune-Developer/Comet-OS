#pragma once

#include <stdint.h>

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

void vm_init(void);
uint64_t alloc_page(void);
void free_page(uint64_t phys_addr);
int vm_map(uint64_t virt_addr, uint64_t phys_addr, uint32_t prot);
int vm_unmap(uint64_t virt_addr);
int vm_protect(uint64_t virt_addr, uint32_t prot);
uint64_t get_free_pages(void);
uint64_t get_total_pages(void);
