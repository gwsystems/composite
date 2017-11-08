//#include <cos_component.h>
#include <cos_kernel_api.h>
#include <pong.h>
#include <cos_types.h>
#include <cobj_format.h>

struct user_cap {
	void (*invfn)(void);
	int entryfn, invcount, capnum;
} __attribute__((packed));

enum
{
	BOOT_SINV_CAP = round_up_to_pow2(BOOT_CAPTBL_FREE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
	PONG_SINV_CAP = round_up_to_pow2(BOOT_SINV_CAP + CAP32B_IDSZ, CAPMAX_ENTRY_SZ)
};

void cos_init(void)
{
	printc("Welcome to the ping component\n");
//	printc("cpu %ld, thd %d from ping\n",cos_cpuid(), cos_get_thd_id());
//	struct user_cap *cap = (vaddr_t) 0x44c05010;

//	cap[0] = (struct user_cap) {
//		.invfn = (void *) 0x44c004c0
//	};	

//	printc("actually call() with mapped in caps: %x \n", (vaddr_t)cap[0].invfn);
	call();			/* get stack */
	printc("returned from call()\n");
	return;
}
