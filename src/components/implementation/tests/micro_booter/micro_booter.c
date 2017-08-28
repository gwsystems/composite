#include "micro_booter.h"

struct cos_compinfo booter_info;
thdcap_t            termthd; /* switch to this to shutdown */
unsigned long       tls_test[TEST_NTHDS];

struct cobj_header *h;

#include <llprint.h>

void call(void) { }

/* For Div-by-zero test */
int num = 1, den = 0;

void
term_fn(void *d)
{
	SPIN();
}

void
print_symbs(void)
{
	unsigned int i;

	for (i = 0 ; i < h->nsymb ; i++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(h, i);
		assert(symb);

		switch (symb->type) {
		case COBJ_SYMB_COMP_INFO:
			printc("cobj: comp info %s: addr %x\n", symb->name, symb->vaddr);
			break;
		case COBJ_SYMB_COMP_PLT:
			/* Otherwise known as ST_user_caps. */
			printc("cobj: capability array %s: addr %x\n", symb->name, symb->vaddr);
			break;
		case COBJ_SYMB_EXPORTED:
			printc("cobj: exported symbol %s: nsymb %d, addr %x\n", symb->name, i, symb->vaddr);
			break;
		case COBJ_SYMB_UNDEF:
			printc("cobj: undefined symbol %s: nsymb %d, usercap offset %d\n", symb->name, i, symb->user_caps_offset);
			break;
		default:
			printc("boot: Unknown symbol type %d\n", symb->type);
			break;
		}
	}
}

void
cos_init(void)
{
	int cycs;

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	termthd = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL);
	assert(termthd);

    h = (struct cobj_header *)cos_comp_info.cos_poly[0];
    print_symbs();

	cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%d cycles per microsecond\n", cycs);

	PRINTC("\nMicro Booter started.\n");
	test_run_mb();
	PRINTC("\nMicro Booter done.\n");

	cos_thd_switch(termthd);

	return;
}
