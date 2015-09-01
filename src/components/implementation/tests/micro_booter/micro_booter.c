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

static void
thd_fn(void *d)
{
	printc("\tNew thread %d with argument %d\n", cos_thdid(), (int)d);
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	printc("Error, shouldn't get here!\n");
}

#define TEST_NTHDS 5
static void
test_thds(void)
{
	thdcap_t ts[TEST_NTHDS];
	int i;

	for (i = 0 ; i < TEST_NTHDS ; i++) {
		ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn, (void *)i);
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

volatile arcvcap_t rc_global;

static void
async_thd_fn(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rc_global;
	unsigned long a, b;
	int pending;

	printc("Asynchronous event thread handler.\n\t< rcving...\n");
	pending = cos_rcv(rc, &a, &b);
	printc("\t< pending %d\n\t< rcving...\n", pending);
	pending = cos_rcv(rc, &a, &b);
	printc("\t< pending %d\n\t< rcving...\n", pending);
	pending = cos_rcv(rc, &a, &b);
	printc("\t< Error: manually returning to snding thread.\n");
	cos_thd_switch(tc);
	printc("ERROR: in async thd *after* switching back to the snder.\n");
	while (1) ;
}

static void
test_async_endpoints(void)
{
	thdcap_t tc;
	arcvcap_t rc;
	asndcap_t sc;
	int ret, pending;
	unsigned long a, b;

	printc("Creating thread, and async end-points.\n");
	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void*)BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(tc);
	rc = cos_arcv_alloc(&booter_info, tc, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(rc);
	rc_global = rc;
	sc = cos_asnd_alloc(&booter_info, rc, booter_info.captbl_cap);
	assert(sc);
	printc("> sending\n");
	ret = cos_asnd(sc);
	if (ret) printc("asnd returned %d.\n", ret);
	printc("> Back in the asnder.\n> sending\n");
	ret = cos_asnd(sc);
	if (ret) printc("> asnd returned %d.\n", ret);
	printc("> Back in the asnder.\n> receiving to get notifications");
	pending = cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &a, &b);
	printc("> pending %d\n", pending);
	printc("Async end-point test successful.\nTest done.\n");
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
	test_async_endpoints();

	printc("\nMicro Booter done.\n");

	BUG();

	return;
}
