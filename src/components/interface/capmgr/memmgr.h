#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_kernel_api.h>
#include <cos_types.h>

vaddr_t memmgr_va2pa(vaddr_t vaddr);
vaddr_t memmgr_pa2va_map(paddr_t pa, unsigned int len);

vaddr_t memmgr_heap_page_alloc(void);
vaddr_t memmgr_heap_page_allocn(unsigned int num_pages);

int memmgr_shared_page_alloc(vaddr_t *pgaddr);
int memmgr_shared_page_allocn(int num_pages, vaddr_t *pgaddr);
int memmgr_shared_page_map(int id, vaddr_t *pgaddr);

/* This magic number is double the tls size defined in RK */
#define TLS_AREA_SIZE 32
#define TLS_NUM_PAGES (round_up_to_page(TLS_AREA_SIZE * MAX_NUM_THREADS) / PAGE_SIZE)
#define TLS_BASE_ADDR 0x70000000

void *memmgr_tls_alloc(unsigned int dst_tid);

/*
 * Define function in header such that calling cos_thd_mod
 * can use one's own captbl. This way I can prevent having to
 * copy captbls into the tls manager component and just have the
 * invoking components do it themselves
 */
static void *
memmgr_tls_alloc_and_set(void *area)
{
	printc("memmgr_tls_alloc_and_set spinning\n");
	while (1);
	//void *addr;
	//struct cos_defcompinfo *dci;
	//struct cos_compinfo    *ci;
	//unsigned int dst_thdcap;
	//int tid;

	//dci = cos_defcompinfo_curr_get();
	//assert(dci);
	//ci  = cos_compinfo_get(dci);
	//assert(ci);

	//dst_thdcap = sl_thd_curr()->aepinfo->thd;

	//tid = cos_introspect(ci, dst_thdcap, THD_GET_TID);

	//addr = tlsmgr_alloc(tid);
	//assert(addr);

	//if (area) memcpy(addr, area, TLS_AREA_SIZE);

	//cos_thd_mod(ci, dst_thdcap, addr);

	return 0;
}

/*
 * Used to overwrite libc syscall to set_thread_area
 * TODO make this a default thing in all components that need libposix
 */
//static void
//tls_thread_area_init(void)
//{ posix_syscall_override((cos_syscall_t)tlsmgr_alloc_and_set, __NR_set_thread_area); }

#endif /* MEMMGR_H */
