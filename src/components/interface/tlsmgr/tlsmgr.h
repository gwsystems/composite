#ifndef TLSMGR_H
#define TLSMGR_H

#include <cos_types.h>
#include <posix.h>

/* This magic number is the tls size defined in RK */
#define TLS_AREA_SIZE 36
#define TLS_NUM_PAGES (round_up_to_page(TLS_AREA_SIZE * MAX_NUM_THREADS) / PAGE_SIZE)
#define TLS_BASE_ADDR 0x70000000

void *tlsmgr_alloc(unsigned int dst_tid);

/*
 * Define function in header such that calling cos_thd_mod
 * can use one's own captbl. This way I can prevent having to
 * copy captbls into the tls manager component and just have the
 * invoking components do it themselves
 */
static void *
tlsmgr_alloc_and_set(void *area)
{
	void *addr;
	struct cos_defcompinfo *dci;
	struct cos_compinfo    *ci;
	unsigned int dst_thdcap;
	int tid;

	printc("tlsmgr_alloc_and_set\n");

	dci = cos_defcompinfo_curr_get();
	assert(dci);
	ci  = cos_compinfo_get(dci);
	assert(ci);

	dst_thdcap = sl_thd_curr()->aepinfo->thd;

	tid = cos_introspect(ci, dst_thdcap, THD_GET_TID);

	addr = tlsmgr_alloc(tid);
	assert(addr);

	if (area) memcpy(addr, area, TLS_AREA_SIZE);

	cos_thd_mod(ci, dst_thdcap, addr);

	return 0;
}

/* Used to overwrite libc syscall to set_thread_area */
static void
tls_thread_area_init(void)
{ posix_syscall_override((cos_syscall_t)tlsmgr_alloc_and_set, __NR_set_thread_area); }

#endif /* TLSMGR_H */
