#include <cos_dcb.h>
#include <cos_kernel_api.h>
#include <../interface/capmgr/memmgr.h>

static unsigned long free_off[NUM_CPU] CACHE_ALIGNED = { 0 };
static struct cos_dcb_info *dcb_off[NUM_CPU] CACHE_ALIGNED = { NULL }, *initdcb[NUM_CPU] CACHE_ALIGNED = { NULL };

void
cos_dcb_info_init(void)
{
	dcb_off[cos_cpuid()] = initdcb[cos_cpuid()] = (struct cos_dcb_info *)memmgr_initdcbpage_retrieve();
	assert(initdcb[cos_cpuid()]);

	dcb_off[cos_cpuid()]++;
	free_off[cos_cpuid()] = 1;
}

void
cos_dcb_info_alloc(void)
{
	dcb_off[cos_cpuid()] = (struct cos_dcb_info *)memmgr_dcbpage_allocn(1);
	assert(dcb_off[cos_cpuid()]);

	free_off[cos_cpuid()] = 0;
}

struct cos_dcb_info *
cos_dcb_info_assign(void)
{
	unsigned long curr_off = 0;

	curr_off = ps_faa(&free_off[cos_cpuid()], 1);
	if (curr_off >= COS_DCB_PERPG_MAX) {
		cos_dcb_info_alloc();
		curr_off = ps_faa(&free_off[cos_cpuid()], 1);
	}

	return (dcb_off[cos_cpuid()] + curr_off);
}

struct cos_dcb_info *
cos_dcb_info_init_get(void)
{
	return initdcb[cos_cpuid()];
}
