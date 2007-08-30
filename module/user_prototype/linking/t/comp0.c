#include <cos_component.h>

extern int spd1_fn(void);
//extern int SS_ipc_client_marshal();

const char msg[] = "hello world!";

#define ITER 1
//#define ITER 1

int find_size(const char *m)
{
	int sz;

	for (sz = 0 ; msg[sz] != '\0' ; sz++) ;

	return sz;
}

static inline int call_spd1_fn(const char *m, int sz)
{
#ifdef NIL
	int i;
	int *sz_ptr = COS_FIRST_ARG;
	char *m_ptr = (char *)(sz_ptr + 1); /* plus 4 bytes */

	*sz_ptr = sz;
	for (i = 0 ; i < sz ; i++) {
		m_ptr[i] = m[i];
	}
#endif
	return spd1_fn();
}

int spd0_main(void)
{
	int i;
	int sz = find_size(msg);
	int ret = call_spd1_fn(msg, sz);
	int nothing;

	for (i = 0 ; i < ITER-1 ; i++) {
		call_spd1_fn(msg, sz);
	}

//	int ret = SS_ipc_client_marshal();
	//prevent_tail_call(nothing);
	return ret;
}
