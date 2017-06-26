#include <print.h>
#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

#include <mem_mgr.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cobj_format.h>
#include <cbuf.h>

/* 
 * Abstraction layer around 1) synchronization, 2) scheduling and
 * thread creation, and 3) memory operations.  
 */

/* synchronization... */
#define LOCK()   if (sched_component_take(cos_spd_id())) BUG();
#define UNLOCK() if (sched_component_release(cos_spd_id())) BUG();

/* scheduling/thread operations... */
#define __sched_create_thread_default sched_create_thread_default

#include <cinfo.h>
#include <cos_vect.h>
#include <quarantine_impl.h>

COS_VECT_CREATE_STATIC(spd_sect_cbufs);
COS_VECT_CREATE_STATIC(spd_sect_cbufs_header);

/* SLAB? */
struct cbid_caddr all_spd_sect_cbufs[CBUFS_PER_PAGE * SECT_CBUF_PAGES];
unsigned int all_cbufs_index = 0;

//#define TEST_QUARANTINE

#if defined(TEST_QUARANTINE)
static spdid_t some_spd = 0;
#endif

static void
boot_deps_init(void)
{
	cos_vect_init_static(&spd_sect_cbufs);
	cos_vect_init_static(&spd_sect_cbufs_header);
	cos_vect_init_static(&bthds);
}

static void
boot_deps_map_sect(spdid_t spdid, void *src_start, vaddr_t dest_start, int pages, struct cobj_header *h, unsigned int sect_id)
{
	struct cbid_caddr *sect_cbufs;
	char *caddr;
	spdid_t b_spd;
	vaddr_t dsrc, dest;
	struct cobj_sect *sect;
	int flags;
	struct cbid_caddr cbm = { .cbid = 0, .caddr = NULL};
	
	dsrc = (vaddr_t)src_start; 
	dest = dest_start;
	sect = cobj_sect_get(h, sect_id);

	if (sect->flags & COBJ_SECT_WRITE) flags = MAPPING_RW;
	else flags = MAPPING_READ;
	flags |= MAPPING_NO_VALLOC;

	assert(pages > 0);
	cbm.caddr = cbuf_alloc_ext(pages * PAGE_SIZE, &cbm.cbid, CBUF_EXACTSZ);
	assert(cbm.caddr);

	sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, spdid);
	if (!sect_cbufs) {
		sect_cbufs = &all_spd_sect_cbufs[all_cbufs_index];
		all_cbufs_index += h->nsect;
		if (cos_vect_add_id(&spd_sect_cbufs, sect_cbufs, spdid) < 0) BUG();
		if (cos_vect_add_id(&spd_sect_cbufs_header, h, spdid) < 0) BUG();
	}

	assert(sect_cbufs);
	assert(sect_id < h->nsect);
	sect_cbufs[sect_id] = cbm;

	b_spd = cos_spd_id();
	caddr = cbm.caddr;
	while (pages-- > 0) {
		/* might be better to memcpy, but after populate */
		if (dsrc != (mman_alias_page(b_spd, (vaddr_t)caddr, b_spd, dsrc, MAPPING_RW))) BUG();
		dsrc += PAGE_SIZE;
		caddr += PAGE_SIZE;
	}
	if (dest != (cbuf_map_at(b_spd, cbm.cbid, spdid, dest | flags))) BUG();
#if defined(TEST_QUARANTINE)
	if (!some_spd) some_spd = spdid;
#endif
}

static void
boot_deps_save_hp(spdid_t spdid, void *hp)
{
	cinfo_add_heap_pointer(cos_spd_id(), spdid, hp);
}
void __boot_deps_save_hp(spdid_t spdid, void *hp) {
	return boot_deps_save_hp(spdid, hp);
}

static void
boot_deps_run(void) {
#if defined(TEST_QUARANTINE)
	printc("copying %d\n", some_spd);
	spdid_t new_spd = quarantine_fork(cos_spd_id(), some_spd);
	printc("forked %d to %d\n", some_spd, new_spd);
#endif
	return; }

/* hack to get access to the functions in booter.c */
static int boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
int __boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci)
{
	return boot_spd_set_symbs(h, spdid, ci);
}

static int boot_spd_caps(struct cobj_header *h, spdid_t spdid);
int __boot_spd_caps(struct cobj_header *h, spdid_t spdid)
{
	return boot_spd_caps(h, spdid);
}

static int boot_spd_thd(spdid_t spdid);
int __boot_spd_thd(spdid_t spdid) {
	return boot_spd_thd(spdid); 
}

