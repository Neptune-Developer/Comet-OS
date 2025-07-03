#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <vm_pages.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define VM_MAX_PAGES 262144
#define VM_AREA_POOL_SIZE 1024
#define RESERVED_PAGES 1024

#define PTE_VALID       (1ULL << 0)
#define PTE_TABLE       (1ULL << 1)
#define PTE_AF          (1ULL << 10)
#define PTE_SH_INNER    (3ULL << 8)
#define PTE_AP_RW_EL1   (0ULL << 6)
#define PTE_AP_RO_EL1   (2ULL << 6)
#define PTE_AP_RW_EL0   (1ULL << 6)
#define PTE_AP_RO_EL0   (3ULL << 6)
#define PTE_PXN         (1ULL << 53)
#define PTE_UXN         (1ULL << 54)

#define ATTR_INDEX_NORMAL_WB_WA  2
#define PTE_ATTR_INDX(idx) ((uint64_t)(idx) << 2)
#define PTE_ADDR_MASK 0x0000FFFFFFFFF000ULL

#define PFN_TO_PADDR(pfn) ((pfn) << PAGE_SHIFT)
#define PADDR_TO_PFN(paddr) ((paddr) >> PAGE_SHIFT)

// W^X protection flags
#define VM_AREA_WAS_EXEC (1 << 0)
#define VM_AREA_WAS_WRITE (1 << 1)

struct page {
    uint64_t phys_addr;
    uint32_t flags;
    uint32_t ref_count;
};

struct vm_area {
    uint64_t start;
    uint64_t end;
    uint32_t prot;
    uint32_t wx_flags;  // track w^x history
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

// external kernel_panic declaration
extern void kernel_panic(const char* error);

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

static void set_bit(uint64_t pfn) {
    int bitmap_idx = pfn / 32;
    int bit_idx = pfn % 32;
    page_bitmap[bitmap_idx] |= (1 << bit_idx);
}

static void clear_bit(uint64_t pfn) {
    int bitmap_idx = pfn / 32;
    int bit_idx = pfn % 32;
    page_bitmap[bitmap_idx] &= ~(1 << bit_idx);
}

static bool is_bit_set(uint64_t pfn) {
    int bitmap_idx = pfn / 32;
    int bit_idx = pfn % 32;
    return (page_bitmap[bitmap_idx] & (1 << bit_idx)) != 0;
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

static uint64_t* get_or_alloc_page_table(uint64_t* parent_entry, bool allocate) {
    if (*parent_entry & PTE_VALID) {
        if (*parent_entry & PTE_TABLE) {
            return (uint64_t*)(*parent_entry & PTE_ADDR_MASK);
        } else {
            return NULL;
        }
    } else if (allocate) {
        uint64_t new_table = alloc_page();
        if (new_table == 0) return NULL;
        
        zero_page(new_table);
        *parent_entry = new_table | PTE_VALID | PTE_TABLE | PTE_AF | PTE_SH_INNER;
        memory_barrier();
        
        return (uint64_t*)new_table;
    }
    return NULL;
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
        set_bit(i);
        pages[i].ref_count = 1;
    }
    
    for (int i = 0; i < VM_AREA_POOL_SIZE; i++) {
        vm_area_pool[i].in_use = 0;
        vm_area_pool[i].next = NULL;
        vm_area_pool[i].wx_flags = 0;
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
    
    for (uint64_t i = RESERVED_PAGES; i < VM_MAX_PAGES; i++) {
        if (!is_bit_set(i)) {
            set_bit(i);
            pages[i].ref_count = 1;
            pages[i].flags = 0;
            free_pages--;
            
            uint64_t phys_addr = pages[i].phys_addr;
            zero_page(phys_addr);
            
            return phys_addr;
        }
    }
    return 0;
}

uint64_t alloc_pages(int count) {
    if (count <= 0 || count > free_pages) {
        return 0;
    }
    
    for (int start = RESERVED_PAGES; start <= VM_MAX_PAGES - count; start++) {
        int consecutive = 0;
        
        for (int i = start; i < start + count; i++) {
            if (is_bit_set(i)) {
                break;
            }
            consecutive++;
        }
        
        if (consecutive == count) {
            for (int i = start; i < start + count; i++) {
                set_bit(i);
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

void free_page(uint64_t phys_addr) {
    if (phys_addr == 0) return;
    
    uint64_t pfn = PADDR_TO_PFN(phys_addr);
    if (pfn >= VM_MAX_PAGES || pfn < RESERVED_PAGES) return;
    
    if (pages[pfn].ref_count > 0) {
        pages[pfn].ref_count--;
        if (pages[pfn].ref_count == 0) {
            clear_bit(pfn);
            pages[pfn].flags = 0;
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

void set_page_table_base(uint64_t base) {
    page_table_base = (uint64_t*)base;
}

int vm_map(uint64_t virt_addr, uint64_t phys_addr, uint32_t prot) {
    if (!page_table_base) return -1;
    if (virt_addr & 0xFFF || phys_addr & 0xFFF) return -1;
    
    struct vm_area* area = alloc_vm_area();
    if (!area) return -1;
    
    area->start = virt_addr;
    area->end = virt_addr + PAGE_SIZE;
    area->prot = prot;
    area->wx_flags = 0;
    
    // track initial state
    if (prot & PROT_EXEC) {
        area->wx_flags |= VM_AREA_WAS_EXEC;
    }
    if (prot & PROT_WRITE) {
        area->wx_flags |= VM_AREA_WAS_WRITE;
    }
    
    area->next = vm_areas;
    vm_areas = area;
    
    uint64_t l0_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t l1_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t l2_idx = (virt_addr >> 21) & 0x1FF;
    uint64_t l3_idx = (virt_addr >> 12) & 0x1FF;
    
    uint64_t* l0_table = page_table_base;
    uint64_t* l1_table = get_or_alloc_page_table(&l0_table[l0_idx], true);
    if (!l1_table) {
        free_vm_area(area);
        return -1;
    }
    
    uint64_t* l2_table = get_or_alloc_page_table(&l1_table[l1_idx], true);
    if (!l2_table) {
        free_vm_area(area);
        return -1;
    }
    
    uint64_t* l3_table = get_or_alloc_page_table(&l2_table[l2_idx], true);
    if (!l3_table) {
        free_vm_area(area);
        return -1;
    }
    
    uint64_t pte_flags = PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTR_INDX(ATTR_INDEX_NORMAL_WB_WA);
    
    if (!(prot & PROT_EXEC)) {
        pte_flags |= PTE_UXN | PTE_PXN;
    }
    
    if (prot & PROT_WRITE) {
        if (prot & PROT_USER) {
            pte_flags |= PTE_AP_RW_EL0;
        } else {
            pte_flags |= PTE_AP_RW_EL1;
        }
    } else if (prot & PROT_READ) {
        if (prot & PROT_USER) {
            pte_flags |= PTE_AP_RO_EL0;
        } else {
            pte_flags |= PTE_AP_RO_EL1;
        }
    }
    
    l3_table[l3_idx] = phys_addr | pte_flags;
    tlb_invalidate_page(virt_addr);
    
    return 0;
}

int vm_unmap(uint64_t virt_addr) {
    if (!page_table_base) return -1;
    if (virt_addr & 0xFFF) return -1;
    
    uint64_t l0_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t l1_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t l2_idx = (virt_addr >> 21) & 0x1FF;
    uint64_t l3_idx = (virt_addr >> 12) & 0x1FF;
    
    uint64_t* l0_table = page_table_base;
    uint64_t* l1_table = get_or_alloc_page_table(&l0_table[l0_idx], false);
    if (!l1_table) return -1;
    
    uint64_t* l2_table = get_or_alloc_page_table(&l1_table[l1_idx], false);
    if (!l2_table) return -1;
    
    uint64_t* l3_table = get_or_alloc_page_table(&l2_table[l2_idx], false);
    if (!l3_table) return -1;
    
    l3_table[l3_idx] = 0;
    tlb_invalidate_page(virt_addr);
    
    struct vm_area** current = &vm_areas;
    while (*current) {
        if ((*current)->start <= virt_addr && virt_addr < (*current)->end) {
            struct vm_area* to_free = *current;
            *current = (*current)->next;
            free_vm_area(to_free);
            break;
        }
        current = &(*current)->next;
    }
    
    return 0;
}

int vm_protect(uint64_t virt_addr, uint32_t prot) {
    if (!page_table_base) return -1;
    if (virt_addr & 0xFFF) return -1;
    
    // find vm_area first to check w^x violation
    struct vm_area* area = vm_areas;
    while (area) {
        if (area->start <= virt_addr && virt_addr < area->end) {
            break;
        }
        area = area->next;
    }
    
    if (!area) return -1;
    
    // w^x security check
    if (prot & PROT_EXEC) {
        if (area->wx_flags & VM_AREA_WAS_WRITE) {
            kernel_panic("[SECURITY] CANNOT TURN READ/WRITABLE MEM INTO EXECUTABLE");
        }
    }
    
    uint64_t l0_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t l1_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t l2_idx = (virt_addr >> 21) & 0x1FF;
    uint64_t l3_idx = (virt_addr >> 12) & 0x1FF;
    
    uint64_t* l0_table = page_table_base;
    uint64_t* l1_table = get_or_alloc_page_table(&l0_table[l0_idx], false);
    if (!l1_table) return -1;
    
    uint64_t* l2_table = get_or_alloc_page_table(&l1_table[l1_idx], false);
    if (!l2_table) return -1;
    
    uint64_t* l3_table = get_or_alloc_page_table(&l2_table[l2_idx], false);
    if (!l3_table) return -1;
    
    uint64_t entry = l3_table[l3_idx];
    if (!(entry & PTE_VALID)) return -1;
    
    uint64_t phys_addr = entry & PTE_ADDR_MASK;
    uint64_t pte_flags = PTE_VALID | PTE_AF | PTE_SH_INNER | PTE_ATTR_INDX(ATTR_INDEX_NORMAL_WB_WA);
    
    if (!(prot & PROT_EXEC)) {
        pte_flags |= PTE_UXN | PTE_PXN;
    }
    
    if (prot & PROT_WRITE) {
        if (prot & PROT_USER) {
            pte_flags |= PTE_AP_RW_EL0;
        } else {
            pte_flags |= PTE_AP_RW_EL1;
        }
    } else if (prot & PROT_READ) {
        if (prot & PROT_USER) {
            pte_flags |= PTE_AP_RO_EL0;
        } else {
            pte_flags |= PTE_AP_RO_EL1;
        }
    }
    
    l3_table[l3_idx] = phys_addr | pte_flags;
    tlb_invalidate_page(virt_addr);
    
    // update area state and track w^x history
    area->prot = prot;
    if (prot & PROT_WRITE) {
        area->wx_flags |= VM_AREA_WAS_WRITE;
    }
    if (prot & PROT_EXEC) {
        area->wx_flags |= VM_AREA_WAS_EXEC;
    }
    
    return 0;
}

uint64_t get_free_pages(void) {
    return free_pages;
}

uint64_t get_total_pages(void) {
    return total_pages;
}
