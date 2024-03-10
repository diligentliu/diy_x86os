#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "cpu/mmu.h"

static addr_alloc_t paddr_alloc;

static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE)));

static void addr_alloc_init(addr_alloc_t *addr_alloc, uint8_t *bits,
							uint32_t start, uint32_t size, uint32_t page_size) {
	addr_alloc->start = start;
	addr_alloc->size = size;
	addr_alloc->page_size = page_size;
	mutex_init(&addr_alloc->mutex);
	bitmap_init(&addr_alloc->bitmap, bits, size / page_size, 0);
}

static uint32_t addr_alloc_page(addr_alloc_t *addr_alloc, int page_count) {
	uint32_t addr = 0;

	mutex_lock(&addr_alloc->mutex);
	int index = bitmap_alloc_nbits(&addr_alloc->bitmap, 0, page_count);
	if (index >= 0) {
		addr = addr_alloc->start + index * addr_alloc->page_size;
	}
	mutex_unlock(&addr_alloc->mutex);
	return addr;
}

static void addr_free_page(addr_alloc_t *addr_alloc, uint32_t addr, int page_count) {
	uint32_t page_index = (addr - addr_alloc->start) / addr_alloc->page_size;
	mutex_lock(&addr_alloc->mutex);
	bitmap_set_bit(&addr_alloc->bitmap, page_index, page_count, 0);
	mutex_unlock(&addr_alloc->mutex);
}

void show_mem_info(boot_info_t *boot_info) {
	log_printf("memory region");
	for (int i = 0; i < boot_info->ram_region_count; ++i) {
		log_printf("[%d]: 0x%x - 0x%x", i,
				   boot_info->ram_region_cfg[i].start, boot_info->ram_region_cfg[i].size);
	}
	log_printf("\r\n");
}

static uint32_t total_mem_size(boot_info_t *boot_info) {
	uint32_t size = 0;
	for (int i = 0; i < boot_info->ram_region_count; ++i) {
		size += boot_info->ram_region_cfg[i].size;
	}
	return size;
}

pte_t *mmu_get_pte(pde_t *page_dir, uint32_t vaddr, int alloc) {
	pde_t *pde = &page_dir[pde_index(vaddr)];
	pte_t *pte;
	if (pde->present) {
		pte =  (pte_t *) pde_paddr(pde);
	} else {
		if (!alloc) {
			return (pte_t *) 0;
		}
		uint32_t pg_paddr = addr_alloc_page(&paddr_alloc, 1);
		if (pg_paddr == 0) {
			return (pte_t *) 0;
		}
		pte = (pte_t *) pg_paddr;
		kernel_memset(pte, 0, MEM_PAGE_SIZE);
		pde->val = (uint32_t) pte | PDE_P | PDE_RW | PDE_US;
	}
	return pte + pte_index(vaddr);
}

int memory_create_map(pde_t *page_dir, uint32_t vstart, uint32_t pstart, int page_count, uint32_t perm) {
	for (int i = 0; i < page_count; ++i) {
		pte_t *pte = mmu_get_pte(page_dir, vstart, 1);
		if (pte == (pte_t *) 0) {
			return -1;
		}

		ASSERT(pte->present == 0);
		pte->val = (uint32_t) page_dir | perm | PTE_P;
		vstart += MEM_PAGE_SIZE;
		pstart += MEM_PAGE_SIZE;
	}
	return 0;
}

void create_kernel_table() {
	extern uint8_t kernel_base[], s_text[], e_text[], s_data[];
	static memory_map_t kernel_map[] = {
			{kernel_base, s_text, kernel_base, PTE_RW},
			{s_text, e_text, s_text, 0},
			{s_data, (void *) MEM_EBDA_START, s_data, PTE_RW},
	};

	for (int i = 0; i < sizeof(kernel_map) / sizeof(memory_map_t); ++i) {
		memory_map_t *map = kernel_map + i;
		uint32_t vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);
		uint32_t vend = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
		uint32_t pstart = down2((uint32_t)map->pstart, MEM_PAGE_SIZE);
		int page_count = (vend - vstart) / MEM_PAGE_SIZE;

		memory_create_map(kernel_page_dir, vstart, pstart, page_count, map->perm);
	}
}

void memory_init(boot_info_t *boot_info) {
	extern uint8_t* kernel_end;
	log_printf("memory init");

	show_mem_info(boot_info);

	uint8_t *mem_free = (uint8_t *)&kernel_end;

	uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;
	mem_up1MB_free = down2(mem_up1MB_free, MEM_PAGE_SIZE);
	log_printf("free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free);

	addr_alloc_init(&paddr_alloc, mem_free, MEM_EXT_START, mem_up1MB_free, MEM_PAGE_SIZE);
	mem_free += bitmap_byte_count(paddr_alloc.bitmap.bit_count);
	ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

	create_kernel_table();
	mmu_set_page_dir(kernel_page_dir);
}