typedef unsigned int vaddr_t;
#define CACHE_LINE (32)
#define CACHE_ALIGNED __attribute__ ((aligned (CACHE_LINE)))
#define HALF_CACHE_ALIGN __attribute__ ((aligned (CACHE_LINE/2)))
struct usr_inv_cap {
	vaddr_t invocation_fn, service_entry_inst;
	unsigned int invocation_count, cap_no;
} HALF_CACHE_ALIGN;
