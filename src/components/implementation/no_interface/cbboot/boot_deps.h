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
#include <quarantine.h>

COS_VECT_CREATE_STATIC(spd_sect_cbufs);
COS_VECT_CREATE_STATIC(spd_sect_cbufs_header);

/* Need static storage for tracking cbufs to avoid dynamic allocation
 * before boot_deps_map_sect finishes. Each spd has probably 12 or so
 * sections, so one page of cbuf_t (1024 cbufs) should be enough to boot
 * about 80 components. */
#define CBUFS_PER_PAGE (PAGE_SIZE / sizeof(cbuf_t))
#define SECT_CBUF_PAGES (1)
struct cbid_caddr {
	cbuf_t cbid;
	void *caddr;
};
static struct cbid_caddr all_spd_sect_cbufs[CBUFS_PER_PAGE * SECT_CBUF_PAGES];
static unsigned int all_cbufs_index = 0;

static spdid_t some_spd = 0;

static void
boot_deps_init(void)
{
	cos_vect_init_static(&spd_sect_cbufs);
	cos_vect_init_static(&spd_sect_cbufs_header);
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
	flags |= 2; /* no valloc */

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
		if (dsrc != (mman_alias_page(b_spd, (vaddr_t)caddr, b_spd, dsrc, MAPPING_RW))) BUG();
		dsrc += PAGE_SIZE;
		caddr += PAGE_SIZE;
	}
	if (dest != (cbuf_map_at(b_spd, cbm.cbid, spdid, dest | flags))) BUG();
	if (!some_spd) some_spd = spdid;
}

static void
boot_deps_save_hp(spdid_t spdid, void *hp)
{
	cinfo_add_heap_pointer(cos_spd_id(), spdid, hp);
}

static void
boot_deps_run(void) {
	printc("copying %d\n", some_spd);
	spdid_t new_spd = quarantine_fork(cos_spd_id(), some_spd);
	printc("forked %d to %d\n", some_spd, new_spd);
	return; }

/* The rest really belongs in a separate source file, but the boot_deps
 * header can't be included easily (yet) except in booter.c. */

/* prototypes for booter.c functions */
static int 
boot_spd_set_symbs(struct cobj_header *h, spdid_t spdid, struct cos_component_information *ci);
static int boot_spd_caps(struct cobj_header *h, spdid_t spdid);
static int boot_spd_thd(spdid_t spdid);

/* quarantine if */
spdid_t
quarantine_fork(spdid_t spdid, spdid_t source)
{
	spdid_t d_spd = 0;
	struct cbid_caddr *sect_cbufs;
	struct cobj_header *h;
	struct cobj_sect *sect;
	vaddr_t init_daddr;
	long tot = 0;
	int j;

	sect_cbufs = cos_vect_lookup(&spd_sect_cbufs, source);
	h = cos_vect_lookup(&spd_sect_cbufs_header, source);
	if (!sect_cbufs || !h) BUG(); //goto done;
	
	/* The following, copied partly from booter.c,  */
	if ((d_spd = cos_spd_cntl(COS_SPD_CREATE, 0, 0, 0)) == 0) BUG();
	sect = cobj_sect_get(h, 0);
	init_daddr = sect->vaddr;
	if (cos_spd_cntl(COS_SPD_LOCATION, d_spd, sect->vaddr, SERVICE_SIZE)) BUG();

	for (j = 0 ; j < (int)h->nsect ; j++) {
		tot += cobj_sect_size(h, j);
	}
	if (tot > SERVICE_SIZE) {
		if (cos_vas_cntl(COS_VAS_SPD_EXPAND, d_spd, sect->vaddr + SERVICE_SIZE, 
				 3 * round_up_to_pgd_page(1))) {
			printc("cbboot: could not expand VAS for component %d\n", d_spd);
			BUG();
		}
	}

	vaddr_t prev_map = 0;
	for (j = 0 ; j < (int)h->nsect ; j++) {
		vaddr_t d_addr;
		struct cbid_caddr cbm;
		cbuf_t cbid;
		int flags;
		int left;

		sect = cobj_sect_get(h, j);
		d_addr = sect->vaddr;
		cbm = sect_cbufs[j];
		cbid = cbm.cbid;

		left       = cobj_sect_size(h, j);
		/* previous section overlaps with this one, don't remap! */
		if (round_to_page(d_addr) == prev_map) {
			left -= (prev_map + PAGE_SIZE - d_addr);
			d_addr = prev_map + PAGE_SIZE;
		}
		if (left > 0) {
			left = round_up_to_page(left);
			prev_map = d_addr;

			if (sect->flags & COBJ_SECT_WRITE) flags = MAPPING_RW;
			else flags = MAPPING_READ;
			flags |= 2; /* no valloc */

			if (d_addr != (cbuf_map_at(cos_spd_id(), cbid, d_spd, d_addr | flags))) BUG();
			if (sect->flags & COBJ_SECT_CINFO) {
				/* fixup cinfo page */
				struct cos_component_information *ci = cbm.caddr;
				ci->cos_this_spd_id = d_spd;
				boot_spd_set_symbs(h, d_spd, ci);
				boot_deps_save_hp(d_spd, ci->cos_heap_ptr);
			}
			prev_map += left - PAGE_SIZE;
			d_addr += left;
		}

	}

	if (cos_spd_cntl(COS_SPD_ACTIVATE, d_spd, h->ncap, 0)) BUG();
	if (boot_spd_caps(h, d_spd)) BUG();

	/* inform servers about fork */
	if (tot > SERVICE_SIZE) tot = SERVICE_SIZE + 3 * round_up_to_pgd_page(1) - tot;
	else tot = SERVICE_SIZE - tot;
	mman_fork_spd(cos_spd_id(), source, d_spd, prev_map + PAGE_SIZE, tot);

	/* deal with threads */
	if (h->flags & COBJ_INIT_THD) boot_spd_thd(d_spd);

done:
	return d_spd;
}

