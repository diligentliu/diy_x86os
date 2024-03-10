#ifndef OS_MMU_H
#define OS_MMU_H

#include "comm/types.h"
#include "comm/cpu_instr.h"

#define PDE_CNT 1024
#define PDE_P   (1 << 0)
#define PTE_P   (1 << 0)
#define PDE_RW  (1 << 1)
#define PTE_RW  (1 << 1)
#define PDE_US  (1 << 2)
#define PTE_US  (1 << 2)

typedef union _pde_t {
	uint32_t val;
	struct {
		uint32_t present : 1;
		uint32_t read_write : 1;
		uint32_t user_supervisor : 1;
		uint32_t page_write_through : 1;
		uint32_t page_cache_disable : 1;
		uint32_t accessed : 1;
		uint32_t : 1;
		uint32_t ps : 1;
		uint32_t : 4;
		uint32_t page_table_base : 20;
	};
} pde_t;

typedef union _pte_t {
	uint32_t val;
	struct {
		uint32_t present : 1;
		uint32_t read_write : 1;
		uint32_t user_supervisor : 1;
		uint32_t page_write_through : 1;
		uint32_t page_cache_disable : 1;
		uint32_t accessed : 1;
		uint32_t dirty : 1;
		uint32_t pat : 1;
		uint32_t global : 1;
		uint32_t : 3;
		uint32_t page_base : 20;
	};
} pte_t;

static inline uint32_t pde_index(uint32_t vaddr) {
	return vaddr >> 22;
}

static inline uint32_t pte_index(uint32_t vaddr) {
	return (vaddr >> 12) & 0x3FF;
}

static inline uint32_t pde_paddr(pde_t *pde) {
	return pde->page_table_base << 12;
}

static inline uint32_t pte_paddr(pte_t *pte) {
	return pte->page_base << 12;
}

static inline void mmu_set_page_dir(pde_t *page_dir) {
	write_cr3((uint32_t) page_dir);
}

#endif //OS_MMU_H
