#include <cos_component.h>
#include <cos_config.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 512

#include <printc.h>
#define assert(x) do { int y; if (!(x)) y = *(int*)NULL; } while(0)
#define REDEFINE_ASSERT
#include <cringbuf.h>

struct cringbuf sharedbuf;

struct print_buffer {
	char foo[MAX_LEN];
	unsigned int index;
} CACHE_ALIGNED;

static struct print_buffer pbuf[MAX_NUM_CPU];

static int print_init(void)
{
	static int first = 1;
	char *addr, *start;
	unsigned long i, sz;

	if (!first) return 0;
	first = 0;

	sz = cos_trans_cntl(COS_TRANS_MAP_SZ, 0, 0, 0);
	if (sz > (8*1024*1024)) return -1;

	addr = start = cos_get_vas_page();
	if (!start) return -2;
	for (i = PAGE_SIZE ; i < sz ; i += PAGE_SIZE) {
		char *next_addr = cos_get_vas_page();
		if ((((unsigned long)next_addr) - (unsigned long)addr) != PAGE_SIZE) return -3;
		addr = next_addr;
	}

	for (i = 0, addr = start ; i < sz ; i += PAGE_SIZE, addr += PAGE_SIZE) {
		if (cos_trans_cntl(COS_TRANS_MAP, 0, (unsigned long)addr, i)) return -4;
	}
	cringbuf_init(&sharedbuf, start, sz);

	return 0;
}

int print_str(int s1, int s2, int s3, int s4)
{
	int *p[4];
	unsigned int i, j, len = 0, *index_ptr;
	char *s, *buf_ptr;
	struct print_buffer *pbuf_ptr = &pbuf[cos_cpuid()];

	index_ptr = &(pbuf_ptr->index);
	buf_ptr = pbuf_ptr->foo;
	s = &buf_ptr[*index_ptr]; // the beginning of the buffer.

	for (i = 0; i < PARAMS_PER_INV; i++) {
		p[i] = (int *)&buf_ptr[*index_ptr];
		(*index_ptr) += CHAR_PER_INT;
	}

	if (unlikely(*index_ptr >= MAX_LEN)) { cos_print("BUG", 3); assert(0); }

	*p[0] = s1;
	*p[1] = s2;
	*p[2] = s3;
	*p[3] = s4;

	for (j = 0; j < CHAR_PER_INV; j++) {
		if (s[j] == '\0') {
			len = *index_ptr - CHAR_PER_INV + j;
			break;
		}
	}
	
	if (j == CHAR_PER_INV)
		goto pending_print;

#ifdef COS_PRINT_SHELL
	assert(!print_init()); 
	if (sharedbuf.b) {
		int amnt;

		amnt = cringbuf_produce(&sharedbuf, pbuf[cos_cpuid()].foo, len);
		assert(amnt >= 0);
		cos_trans_cntl(COS_TRANS_TRIGGER, 0, 0, 0);
	}
#endif

#ifdef COS_PRINT_DMESG
	cos_print(pbuf[cos_cpuid()].foo, len);
	pbuf[cos_cpuid()].index = 0;
#endif
	return 0;

pending_print:
	return 1;
/* Old implementation below */
/* 	if (!COS_IN_ARGREG(s) || !COS_IN_ARGREG(s + len)) { */
/* 		static char foo[MAX_LEN]; */
/* 		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s); */
/* 		cos_print(foo, 0); */
/* 		return -1; */
/* 	} */
/* 	s[len+1] = '\0'; */
/* #ifdef COS_PRINT_SHELL */
/* 	assert(!print_init());  */
/* 	if (sharedbuf.b) { */
/* 		int amnt; */

/* 		amnt = cringbuf_produce(&sharedbuf, s, len); */
/* 		assert(amnt >= 0); */
/* 		cos_trans_cntl(COS_TRANS_TRIGGER, 0, 0, 0); */
/* 	} */
/* #endif */

/* #ifdef COS_PRINT_DMESG */
/* 	cos_print(s, len); */
/* #endif */
/* 	return 0; */
}

void print_null(void)
{
	return;
}
