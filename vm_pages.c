#include <stdint.h>
#include <stddef.h>
#include "vm_pages.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define VM_MAX_PAGES 262144
#define VM_AREA_POOL_SIZE 1024
#define RESERVED_PAGES 256

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
    uint8_t in_use;
};

static struct page pages[VM_MAX_PAGES];
static uint32_t page_bitmap[VM_MAX_PAGES / 32];
static struct vm_area vm_area_pool[VM_AREA_POOL_SIZE];
static struct vm_area* vm_areas = NULL;
static struct vm_area* free_vm_areas = NULL;

static uint64_t total_pages = 0;
static uint64_t free_pages = 0;
static uint32_t next_free_area = 0;

static uint64_t* page_table_base = (uint64_t*)0x1000;

static inline void tlb_invalidate_page(uint64_t virt_addr) {
    __asm__ volatile(
        "dsb sy\n"
        "tlbi vaae1, %0\n"
        "dsb sy\n"
        "isb\n"
        : : "r" (virt_addr >> 12)
    );
}

static inline void memory_barrier(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline void zero_page(uint64_t phys_addr) {
    volatile uint64_t* ptr = (volatile uint64_t*)phys_addr;
    for (int i = 0; i < PAGE_SIZE / 8; i++) {
        ptr[i] = 0;
    }
}

static struct vm_area* alloc_vm_area(void) {
    if (free_vm_areas) {
        struct vm_area* area = free_vm_areas;
        free_vm_areas = area->next;
        area->in_use = 1;
        return area;
    }
    
    if (next_free_area < VM_AREA_POOL_SIZE) {
        struct vm_area* area = &vm_area_pool[next_free_area++];
        area->in_use = 1;
        return area;
    }
    
    return NULL;
}

static void free_vm_area(struct vm_area* area) {
    if (!area) return;
    
    area->in_use = 0;
    area->next = free_vm_areas;
    free_vm_areas = area;
}

void vm_init(void) {
    for (int i = 0; i < VM_MAX_PAGES; i++) {
        pages[i].phys_addr = i * PAGE_SIZE;
        pages[i].flags = 0;
        pages[i].ref_count = 0;
    }
    
    for (int i = 0; i < VM_MAX_PAGES / 32; i++) {
        page_bitmap[i] = 0;
    }
    
    for (int i = 0; i < RESERVED_PAGES; i++) {
        int bitmap_idx = i / 32;
        int bit_idx = i % 32;
        page_bitmap[bitmap_idx] |= (1 << bit_idx);
        pages[i].ref_count = 1;
    }
    
    for (int i = 0; i < VM_AREA_POOL_SIZE; i++) {
        vm_area_pool[i].in_use = 0;
        vm_area_pool[i].next = NULL;
    }
    
    total_pages = VM_MAX_PAGES;
    free_pages = VM_MAX_PAGES - RESERVED_PAGES;
    
    if (page_table_base) {
        for (int i = 0; i < 512; i++) {
            page_table_base[i] = 0;
        }
        memory_barrier();
    }
}

uint64_t alloc_page(void) {
    if (free_pages == 0) {
        return 0;
    }
    
    for (int i = RESERVED_PAGES / 32; i < VM_MAX_PAGES / 32; i++) {
        if (page_bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                int page_idx = i * 32 + j;
                
                if (page_idx < RESERVED_PAGES) continue;
                
                if (!(page_bitmap[i] & (1 << j))) {
                    page_bitmap[i] |= (1 << j);
                    pages[page_idx].ref_count = 1;
                    pages[page_idx].flags = 0;
                    free_pages--;
                    
                    uint64_t phys_addr = pages[page_idx].phys_addr;
                    zero_page(phys_addr);
                    
                    return phys_addr;
                }
            }
        }
    }
    return 0;
}

void free_page(uint64_t phys_addr) {
    if (phys_addr == 0) return;
    
    int page_idx = phys_addr / PAGE_SIZE;
    if (page_idx >= VM_MAX_PAGES || page_idx < RESERVED_PAGES) return;
    
    if (pages[page_idx].ref_count > 0) {
        pages[page_idx].ref_count--;
        if (pages[page_idx].ref_count == 0) {
            int bitmap_idx = page_idx / 32;
            int bit_idx = page_idx % 32;
            page_bitmap[bitmap_idx] &= ~(1 << bit_idx);
            pages[page_idx].flags = 0;
            free_pages++;
            
            zero_page(phys_addr);
        }
    }
}

void free_pages(uint64_t phys_addr, int count) {
    for (int i = 0; i < count; i++) {
        free_page(phys_addr + (i * PAGE_SIZE));
    }
}

static uint64_t* get_or_alloc_page_table(uint64_t* parent_entry) {
    if (*parent_entry & 1) {
        return (uint64_t*)(*parent_entry & 0xFFFFFFFFFFFFF000ULL);
    }
    
    uint64_t new_table = alloc_page();
    if (new_table == 0) return NULL;
    
    *parent_entry = new_table | 0x03;
    memory_barrier();
    
    return (uint64_t*)new_table;
}

int vm_map(uint64_t virt_addr, uint64_t phys_addr, uint32_t prot) {
    if (!page_table_base) return -1;
    if (virt_addr & 0xFFF || phys_addr & 0xFFF) return -1;
    
    struct vm_area* area = alloc_vm_area();
    if (!area) return -1;
    
    area->start = virt_addr;
    area->end = virt_addr + PAGE_SIZE;
    area->prot = prot;
    area->next = vm_areas;
    vm_areas = area;
    
    uint64_t l1_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t l2_idx = (virt_addr >> 21) & 0x1FF;
    uint64_t l3_idx = (virt_addr >> 12) & 0x1FF;
    
    uint64_t* l2_table = get_or_alloc_page_table(&page_table_base[l1_idx]);
    if (!l2_table) {
        free_vm_area(area);
        return -1;
    }
    
    uint64_t* l3_table = get_or_alloc_page_table(&l2_table[l2

uint64_t alloc_pages(int count) {
    if (count <= 0 || count > free_pages) {
        return 0;
    }
    
    for (int start = RESERVED_PAGES; start <= VM_MAX_PAGES - count; start++) {
        int consecutive = 0;
        
        for (int i = start; i < start + count; i++) {
            int bitmap_idx = i / 32;
            int bit_idx = i % 32;
            
            if (page_bitmap[bitmap_idx] & (1 << bit_idx)) {
                break;
            }
            consecutive++;
        }
        
        if (consecutive == count) {
            for (int i = start; i < start + count; i++) {
                int bitmap_idx = i / 32;
                int bit_idx = i % 32;
                page_bitmap[bitmap_idx] |= (1 << bit_idx);
                pages[i].ref_count = 1;
                pages[i].flags = 0;
                zero_page(pages[i].phys_addr);
            }
            free_pages -= count;
            return pages[start].phys_addr;
        }
    }
    
    return 0;
}
