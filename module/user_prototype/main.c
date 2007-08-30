#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <thread.h>
#include <ipc.h>

#define ITER 1000
#define rdtscll(val) \
      __asm__ __volatile__("rdtsc" : "=A" (val))

/* spd0: app spd */

extern int fn(void);

/*
void print_space_prepended(unsigned int val, int space)
{
	int i;

	for (i = 0 ; i < space ; i++) {
		printf(" ");
	}
	printf("%d\n", val);

	return;
}

void rec_print_kern_call_totals(struct spd *spd, int space)
{
	int i;

	for (i = 1 ; i < spd->static_cap_tbl_size ; i++) {
		struct invocation_cap *sc = &spd->static_cap_tbl[i];

		print_space_prepended(sc->invocation_count, space);
		rec_print_kern_call_totals(sc->spd, space + 4);
	}

	return;
}

void rec_print_user_call_totals(struct spd *spd, int space)
{
	int i;

	for (i = 1 ; i < spd->static_cap_tbl_size ; i++) {
		struct invocation_cap *sc = &spd->static_cap_tbl[i];
		struct user_inv_cap *uic = sc->user_cap_data;

		print_space_prepended(uic->invocation_count, space);
		rec_print_user_call_totals(sc->spd, space + 4);
	}

	return;
}

void print_call_totals(struct thread *thd)
{
	struct spd *base;

	base = thd->stack_base[0].current_spd;

	printf("Total kernel invocations:\n");
	rec_print_kern_call_totals(base, 0);

	printf("Total user invocations:\n");
	rec_print_user_call_totals(base, 0);
}
*/
#define SAME_FILE //LINKED

#ifdef SAME_FILE
int spd0_main(void)
{
	unsigned long long start, end;
	int i;
	int (*fnptr)(void);

	//int val = 0;
	printf("Invocation tests:\n");
	printf("retval %d.\n", ipc_invoke_spd(3));
	printf("retval %d.\n", ipc_invoke_spd(2));
/* until we have separate user-level cap-tbls, this won't work */
//	printf("retval %d.\n", ipc_invoke_spd(1));
	printf("retval %d.\n", 	ipc_invoke_spd(2));
//	printf("retval %d.\n", 	ipc_invoke_spd(1));
	printf("retval %d.\n", 	ipc_invoke_spd(3, 1, 2));


	printf("\nTiming:\n");
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		ipc_invoke_spd(2);
	}
	rdtscll(end);
	printf("Invocation takes %lld.\n", (end-start)/ITER);

	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		fn();
	}
	rdtscll(end);
	printf("Function call takes %lld.\n", (end-start)/ITER);

	fnptr = fn;
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		(*fnptr)();
	}
	rdtscll(end);
	printf("Indirect function call takes %lld.\n", (end-start)/ITER);

	return 0;
}

/* spd1: truster(spd4) and trustee(spd0) */

int spd1_main(void)
//int spd1_main(int input)
{
	printf("spd1.\n");
	ipc_invoke_spd(1);
	ipc_invoke_spd(1);

	return 1;
	//return input+1;
}

/* spd2: trustee(spd0) */

int spd2_main(void)
//int spd2_main(int input)
{
	//printf("spd2.\n");
	return 2;// input+2;
}

/* spd3: trustee(spd0) */

int spd3_main(int arg1, int arg2, int arg3)
//int spd3_main(int input)
{
	printf("spd3: args %d, %d, %d.\n", arg1, arg2, arg3);
	return 3;// input+3;
}

/* spd4: trustee(spd1) */

int spd4_main(void)
//int spd4_main(int input)
{
	printf("spd4.\n");
	return 4;// input+4;
}

struct usr_inv_cap *get_user_static_cap_addr(unsigned int size)
{
	struct usr_inv_cap *u_tbl;

	u_tbl = malloc(sizeof(struct usr_inv_cap)*size);

	return u_tbl;
}

int main(void) 
{
	struct spd *spd, *new_spd, *orig_spd;
	struct thread *thd;

	thd_init();
	spd_init();
	ipc_init();


	spd = spd_alloc(3, get_user_static_cap_addr(3));
	if (!spd) printf("failure to allocate first spd.\n");
	orig_spd = spd;
	thd = thd_alloc(spd);

	new_spd = spd_alloc(1, get_user_static_cap_addr(1));
	if (!new_spd) printf("failure to allocate second spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd1_main, new_spd, 0);
/*
	new_spd = spd_alloc(0, get_user_static_cap_addr(0));
	if (!new_spd) printf("failure to allocate third spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd2_main, new_spd, 0);

	new_spd = spd_alloc(0, get_user_static_cap_addr(0));
	if (!new_spd) printf("failure to allocate fourth spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd3_main, new_spd, 0);

	spd = spd->static_cap_tbl[1].spd;
	new_spd = spd_alloc(0, get_user_static_cap_addr(0));
	if (!new_spd) printf("failure to allocate fifth spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd4_main, new_spd, 0);

	ipc_set_current_thread(thd);
	ipc_set_current_spd(orig_spd);

	spd0_main();
*/
	//print_call_totals(thd);

	return 0;
}

#else

extern int spd1_main(void);
extern int spd4_main(void);

int main(void)
{
	spd_t *spd, *new_spd, *orig_spd;
	thread_t *thd;

	thd_init();
	spd_init();
	ipc_init();

	spd = spd_alloc(3, get_user_static_cap_addr(3));
	if (!spd) printf("failure to allocate first spd.\n");
	orig_spd = spd;
	thd = thd_alloc(spd);

	new_spd = spd_alloc(1, get_user_static_cap_addr(1));
	if (!new_spd) printf("failure to allocate second spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd1_main, new_spd, 0);
/*
	new_spd = spd_alloc(0, get_user_static_cap_addr(0));
	if (!new_spd) printf("failure to allocate third spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd2_main, new_spd, 0);

	new_spd = spd_alloc(0, get_user_static_cap_addr(0));
	if (!new_spd) printf("failure to allocate fourth spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd3_main, new_spd, 0);
*/
	spd = spd->static_cap_tbl[1].spd;
	new_spd = spd_alloc(0, get_user_static_cap_addr(0));
	if (!new_spd) printf("failure to allocate fifth spd.\n");
	spd_add_static_cap(spd, (vaddr_t)spd4_main, new_spd, 0);

	ipc_set_current_thread(thd);
	ipc_set_current_spd(orig_spd);

	printf("%d.\n", spd1_main());

	print_call_totals(thd);

	return 0;
}
#endif

