#include <cos_component.h>

#include <stdio.h>
#include <string.h>

#include <printc.h>
#define assert(x) do { int y; if (!(x)) y = *(int*)NULL; } while(0)

struct print_buffer {
	char foo[MAX_LEN];
	unsigned int index;
} CACHE_ALIGNED;

static struct print_buffer pbuf[MAX_NUM_CPU];

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

	/* cos_print("Commit: ", 8); */
	cos_print(buf_ptr, len);
	*index_ptr = 0;

	return 0;

pending_print:
	return 1;
}

void print_null(void)
{
	return;
}

int main(void)
{
	return 0;
}

/* Old implementation below */
/* int print_str(char *s, unsigned int len) */
/* { */
/* //	char *ptr; */
/* //	int l = len; */
/* 	if (!COS_IN_ARGREG(s) || !COS_IN_ARGREG(s + len)) { */
/* 		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s); */
/* 		cos_print(foo, 0); */
/* 		return -1; */
/* 	} */
/* 	s[len+1] = '\0'; */

/* 	cos_print(s, len); */
/* 	return 0; */
/* } */

