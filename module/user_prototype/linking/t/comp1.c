#include <cos_component.h>

const char comp1str[] = "Hello World from component 1!\n";

extern int spd2_fn(void);

extern int print(void);
extern int print_vals(int a, int b, int c);

static inline int find_size(const char *m)
{
	int sz;

	for (sz = 0 ; m[sz] != '\0' ; sz++) ;

	return sz;
}

static inline int call_print(const char *m)
{
	int sz;
	int *sz_ptr = COS_FIRST_ARG;
	char *m_ptr = (char *)(sz_ptr + 1); // plus 4 bytes 

	sz = find_size(comp1str);
	*sz_ptr = sz;
	cos_memcpy(m_ptr, comp1str, sz);

	return print();
}

int blah = 0;
int spd1_fn(void *data, int thd_id)
{
	static int first = 1;

	if (first) {
		first = 0;
		print_vals(1,2,3);
	}

	return spd2_fn();
}
