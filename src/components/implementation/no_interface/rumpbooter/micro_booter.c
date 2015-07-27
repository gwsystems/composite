#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

int
prints(char *s)
{
    int len = strlen(s);
	  cos_print(s, len);
	  return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_print(s, ret);

	  return ret;
}

#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_thd_switch();} } while(0)
#endif

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

struct cos_compinfo booter_info;

#define TEST_NTHDS 5
static void
test_thds(void)
{
	thdcap_t ts[TEST_NTHDS];
	int i;

	for (i = 0 ; i < TEST_NTHDS ; i++) {
		ts[i] = cos_initthd_alloc(&booter_info, booter_info.comp_cap);
		assert(ts[i]);
		printc("switchto %d\n", (int)ts[i]);
		cos_thd_switch(ts[i]);
	}

	printc("test done\n");
}

static void
test_mem(void)
{
	char *p = cos_page_bump_alloc(&booter_info);

	assert(p);
	strcpy(p, "victory");

	printc("Page allocation: %s\n", p);
}

void
cos_init(void)
{
	printc("\nMicro Booter started.\n");

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_PM_BASE, COS_MEM_USER_PA_SZ,
			 BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	test_thds();
	test_mem();

	printc("\nMicro Booter done.\n");

	while (1) ;

	return;
}

void
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int first = 1;

	if (first) {
		first = 0;
		cos_init();
	} else {
		printc("\tin new thread; switching back\n");
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
		printc("\tWHYYYYYYY MOAR execution!?\n");
	}
}
