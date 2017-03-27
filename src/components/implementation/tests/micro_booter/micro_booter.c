#include "micro_booter.h"

struct cos_compinfo booter_info;
thdcap_t termthd; 		/* switch to this to shutdown */
unsigned long tls_test[TEST_NTHDS];

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

int
prints(char *s)
{
	int len = strlen(s);

	cos_llprint(s, len);

	return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	return 0;
}

/* For Div-by-zero test */
int num = 1, den = 0;

/* The termination thread */
void
term_fn(void *d)
{
	/* Clear the LCD to red color */
	LCD_Clear(0xF800);
	SPIN();
}

u32_t comp1_stack[MAX_STACK_SZ];
u32_t Stack_Mem[ALL_STACK_SZ];

/* The naked function to find a stack */
void __attribute__((naked)) kentryfn(void)
{
	/* Simply find a stack to get into */
	__asm__ __volatile__("ldr r0,=Stack_Mem \n\t"
						"add r1,#0x1000 \n\t"
						"mul r2,r1,r4 \n\t"
						"add r0,r0,r2 \n\t"
						"mov sp,r0 \n\t"
						"mov r0,#0x00 \n\t"/* Upcall-thread-create */
						"mov r1,r6 \n\t"
						"mov r2,r7 \n\t"
						"mov r3,r8 \n\t"
						"b cos_upcall_fn \n\t"
						"b . \n\t"
						:::"memory","cc");
	while(1);
}

/* The temporary stack used for returning */
unsigned long const SVC_Stack[16]={0,   /* R0 */
		                     0,   /* R1 */
							 0,   /* R2 */
							 0,   /* R3 */
							 0,   /* R12 */
							 0xFFFFFFFD, /* LR, return to thread mode */
							 kentryfn, /* Kernel function entry */
							 0x01000200, /* xPSR */
							 0,0,0,0,0,0,0,0 /* Padding for safe */
							 };

/* The parameter is totally useless here */
void
cos_init(void* init)
{
	int cycs;
	/* TODO:remove temporary code for testing call overhead */
	/*long long from;
	long long to;

	rdtscll(from);

	for(cycs=0;cycs<10000;cycs++)
		call_cap_asm(0,0,0,0,0,0);
	rdtscll(to);
	to=to-from;

	char str[100];

	sprintf(str,"INT(Total:%d/Iter:%d ):%d\n",
			(int)to, (int)10000, (int)(to / 10000));

	LCD_ShowString(10,40,260,32,12,str);

	while(1);*/

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE+0x10000, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	termthd = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL);
	assert(termthd);
	/* TODO:remove temporary code to test thread switch */
	/* cos_thd_switch(termthd); */

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
	printc("\t%d cycles per microsecond\n", cycs);

	PRINTC("\nMicro Booter started.\n");
	test_run_mb();
	PRINTC("\nMicro Booter done.\n");

	/* We don;t return. instead, switch to the termination thread */
	cos_thd_switch(termthd);

	return;
}
