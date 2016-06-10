/*
 * Copyright 2016, Phani and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_kernel_api.h>
#include <cos_thd_init.h>
#include "vcos_kernel_api.h"

extern struct cos_compinfo vkern_info;
extern struct cos_compinfo vmbooter_info; /* FIXME: because only limited args can be passed into SINV */

int
__vcos_captbl_op(long arg1, long arg2, long arg3, long arg4)
{
	printc("%s-%s:%d - enter\n", __FILE__, __func__, __LINE__);
	switch (arg1) {
		default:
			break;
	}
	return 0;
}

thdcap_t
vcos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data)
{ 
	printc("%s-%s:%d\n", __FILE__, __func__, __LINE__);
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_THDACTIVATE, (int)fn, (int)data, (int)comp, 0)) BUG();
	return 0;
//	thdcap_t vkthd = cos_thd_alloc(&vkern_info, comp, fn, data);
//	cos_cap_cpy(&vmbooter_info, vkthd, &vkern_info, vkthd);
//	return vkthd;
}

/* TODO: Will not support these for Grid computing project */
thdcap_t
vcos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp)
{ return cos_initthd_alloc(ci, comp); }

captblcap_t
vcos_captbl_alloc(struct cos_compinfo *ci)
{ return vcos_captbl_alloc(ci); }

pgtblcap_t
vcos_pgtbl_alloc(struct cos_compinfo *ci)
{ return cos_pgtbl_alloc(ci); }

compcap_t
vcos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry)
{ return cos_comp_alloc(ci, ctc, ptc, entry); }

int
vcos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, vaddr_t entry,
		   struct cos_compinfo *ci_resources)
{ return cos_compinfo_alloc(ci, heap_ptr, entry, ci_resources); }
/* -------------------------------------------------------*/

sinvcap_t
vcos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry)
{ return cos_sinv_alloc(srcci, dstcomp, entry); }

arcvcap_t
vcos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t compcap, arcvcap_t arcvcap)
{ return cos_arcv_alloc(ci, thdcap, tcapcap, compcap, arcvcap); }

asndcap_t
vcos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap)
{ return cos_asnd_alloc(ci, arcvcap, ctcap); }

hwcap_t
vcos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap)
{ return cos_hw_alloc(ci, bitmap); }

void *
vcos_page_bump_alloc(struct cos_compinfo *ci)
{ return cos_page_bump_alloc(ci); }

tcap_t
vcos_tcap_split(struct cos_compinfo *ci, tcap_t src, int pool)
{ return cos_tcap_split(ci, src, pool); }

#if 0
extern void* thd_alloc_inv(compcap_t comp, cos_thd_fn_t fn, void *data);
extern void* arcv_alloc_inv(thdcap_t thdcap, tcap_t tcapcap, compcap_t comp, arcvcap_t arcvcap);
extern void* tcap_split_inv(long stash);
extern void* asnd_alloc_inv(arcvcap_t arcvcap, captblcap_t cap, int junk);


thdcap_t
__vcos_thd_alloc(compcap_t comp, cos_thd_fn_t fn, void *data)
{
	thdcap_t sthd = cos_thd_alloc(&vkern_info, comp, fn, data);
	assert(sthd);

	// then proceed as normal
	thdcap_t dthd = sthd;
	cos_cap_cpy(&vmbooter_info, sthd, &vkern_info, sthd);

	return dthd;
}

static inline
int call_cap_mb(u32_t cap_no, struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data)
{
	int ret;

	/*
	 * Which stack should we use for this invocation?  Simple, use
	 * this stack, at the current sp.  This is essentially a
	 * function call into another component, with odd calling
	 * conventions.
	 */
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (comp), "S" (fn), "D" (data) \
		: "memory", "cc", "ecx", "edx");

	return ret;
}

thdcap_t
vcos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data)
{
	/*
	 * can't just call cos_thd_alloc in child because child does not have memory
	 * So we allocate in the vKernel and then move over to the virtualized
	 * component
	 */
	assert(ci);

	// sinv to method that calls cos_thd_alloc (or cos_thd_alloc itself?)
	// then that does an sinv back
	sinvcap_t ic = cos_sinv_alloc(&vkern_info, comp, (vaddr_t) thd_alloc_inv);
	assert(ic > 0);

	thdcap_t dthd = call_cap_mb(ic, ci, comp, fn, data);
	
	return dthd;
}

/* TODO: Will not support these for Grid computing project */
thdcap_t
vcos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp)
{ return cos_initthd_alloc(ci, comp); }

captblcap_t
vcos_captbl_alloc(struct cos_compinfo *ci)
{ return vcos_captbl_alloc(ci); }

pgtblcap_t
vcos_pgtbl_alloc(struct cos_compinfo *ci)
{ return cos_pgtbl_alloc(ci); }

compcap_t
vcos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry)
{ return cos_comp_alloc(ci, ctc, ptc, entry); }

int
vcos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, vaddr_t entry,
		   struct cos_compinfo *ci_resources)
{ return cos_compinfo_alloc(ci, heap_ptr, entry, ci_resources); }
/* -------------------------------------------------------*/

sinvcap_t
vcos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry)
{ return cos_sinv_alloc(srcci, dstcomp, entry); }

arcvcap_t
__vcos_arcv_alloc(long stash1, compcap_t comp, arcvcap_t arcvcap)
{
	thdcap_t thdcap   = stash1 >> 16;
	tcap_t tcapcap    = stash1 & 0x0000FFFF;

	//printc("after stashing: %d\n", comp);
	arcvcap_t cap = cos_arcv_alloc(&vkern_info, thdcap, tcapcap, comp, arcvcap);
	assert(cap);

	cos_cap_cpy(&vmbooter_info, cap, &vkern_info, cap);

	return cap;
}

static inline
int call_cap_mb_5(u32_t cap_no, thdcap_t thdcap, tcap_t tcapcap, compcap_t comp, arcvcap_t arcvcap)
{
	int ret;

	long stash1;
	stash1 = 0;
	stash1 = thdcap << 16 | tcapcap;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (stash1), "S" (comp), "D" (arcvcap) \
		: "memory", "cc", "ecx", "edx");

	return ret;
}

arcvcap_t
vcos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t comp, arcvcap_t arcvcap)
{ 
	assert(ci && thdcap && comp);
	sinvcap_t ic = cos_sinv_alloc(&vkern_info, comp, (vaddr_t) arcv_alloc_inv);
	assert(ic > 0);
	
	arcvcap_t cap = call_cap_mb_5(ic, thdcap, tcapcap, comp, arcvcap);
	return cap;
}

asndcap_t
__vcos_asnd_alloc(arcvcap_t arcvcap, captblcap_t cap, int junk)
{
	asndcap_t ret = cos_asnd_alloc(&vkern_info, arcvcap, cap);
	assert(ret);

	// move this to the vmbooter info (init doesn't reallocate memory)
	cos_cap_cpy(&vmbooter_info, ret, &vkern_info, ret);
	return ret;
}

static inline
int call_cap_mb_asnd(u32_t cap_no, arcvcap_t arcvcap, captblcap_t cap)
{
	int ret;
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (arcvcap), "S" (cap), "D" (0) \
		: "memory", "cc", "ecx", "edx");
	
	return ret;
}

asndcap_t
vcos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap)
{
	assert(ci && arcvcap && ctcap); // sure, why not
	sinvcap_t ic = cos_sinv_alloc(&vkern_info, vkern_info.comp_cap, (vaddr_t) asnd_alloc_inv);
	assert(ic > 0);
	
	tcap_t cap = call_cap_mb_asnd(ic, arcvcap, ctcap);
	return cap;
}

hwcap_t
vcos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap)
{ return cos_hw_alloc(ci, bitmap); }

void *
vcos_page_bump_alloc(struct cos_compinfo *ci)
{ return cos_page_bump_alloc(ci); }

tcap_t
__vcos_tcap_split(long stash, tcap_t src)
{
	tcap_t cap = cos_tcap_split(&vkern_info, src);
	assert(cap);
	cos_cap_cpy(&vmbooter_info, cap, &vkern_info, cap);
	
	return cap;
}

static inline
int call_cap_mb_split(u32_t cap_no, tcap_t src)
{
	int ret;

	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (src) \
		: "memory", "cc", "ecx", "edx");
	
	return ret;
}

tcap_t
vcos_tcap_split(struct cos_compinfo *ci, tcap_t src)
{
	assert(ci && src); // sure, why not
	sinvcap_t ic = cos_sinv_alloc(&vkern_info, vkern_info.comp_cap, (vaddr_t) tcap_split_inv);
	assert(ic > 0);
	
	tcap_t cap = call_cap_mb_split(ic, src);
	return cap;
}
#endif
