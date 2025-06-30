#include <stdint.h>
#include <stddef.h>
#include "vm_pages.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define VM_MAX_PAGES 262144

struct page {
    uint64_t phys_addr;
    uint32_t flags;
    uint32_t ref_count;
};

struct vm_area {
    uint64_t start;
    uint64_t end;
    uint32_t prot;
    struct vm_area* next;
};

static struct page pages[VM_MAX_PAGES];
static uint32_t page_bitmap[VM_MAX_PAGES / 32];
static struct vm_area* vm_areas = NULL;

static uint64_t total_pages = 0;
static uint64_t free_pages = 0;

void vm_init(void) {
    for (int i = 0; i < VM_MAX_PAGES; i++) {
        pages[i].phys_addr = i * PAGE_SIZE;
        pages[i].flags = 0;
        pages[i].ref_count = 0;
    }
    
    for (int i = 0; i < VM_MAX_PAGES / 32; i++) {
        page_bitmap[i] = 0;
    }
    
    total_pages = VM_MAX_PAGES;
    free_pages = VM_MAX_PAGES;
}

uint64_t alloc_page(void) {
    for (int i = 0; i < VM_MAX_PAGES / 32; i++) {
        if (page_bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                if (!(page_bitmap[i] & (1 << j))) {
                    page_bitmap[i] |= (1 << j);
                    int page_idx = i * 32 + j;
                    pages[page_idx].ref_count = 1;
                    free_pages--;
                    return pages[page_idx].phys_addr;
                }
            }
        }
    }
    return 0;
}

void free_page(uint64_t phys_addr) {
    int page_idx = phys_addr / PAGE_SIZE;
    if (page_idx >= VM_MAX_PAGES) return;
    
    pages[page_idx].ref_count--;
    if (pages[page_idx].ref_count == 0) {
        int bitmap_idx = page_idx / 32;
        int bit_idx = page_idx % 32;
        page_bitmap[bitmap_idx] &= ~(1 << bit_idx);
        free_pages++;
    }
}

int vm_map(uint64_t virt_addr, uint64_t phys_addr, uint32_t prot) {
    struct vm_area* area = (struct vm_area*)alloc_page();
    if (!area) return -1;
    
    area->start = virt_addr;
    area->end = virt_addr + PAGE_SIZE;
    area->prot = prot;
    area->next = vm_areas;
    vm_areas = area;
    
    return 0;
}

int vm_unmap(uint64_t virt_addr) {
    struct vm_area** curr = &vm_areas;
    while (*curr) {
        if ((*curr)->start == virt_addr) {
            struct vm_area* to_free = *curr;
            *curr = (*curr)->next;
            free_page((uint64_t)to_free);
            return 0;
        }
        curr = &(*curr)->next;
    }
    return -1;
}

int vm_protect(uint64_t virt_addr, uint32_t prot) {
    struct vm_area* curr = vm_areas;
    while (curr) {
        if (curr->start <= virt_addr && virt_addr < curr->end) {
            curr->prot = prot;
            return 0;
        }
        curr = curr->next;
    }
    return -1;
}

uint64_t get_free_pages(void) {
    return free_pages;
}

uint64_t get_total_pages(void) {
    return total_pages;
}
