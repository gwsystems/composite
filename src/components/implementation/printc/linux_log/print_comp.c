#include <cos_component.h>

#include <stdio.h>
#include <string.h>

#include <printc.h>
#define assert(x) do { int y; if (!(x)) y = *(int*)NULL; } while(0)


/* The following implementation haven't been tested yet. */

/* struct print_buffer { */
/* 	char buf[MAX_LEN]; */
/* 	unsigned int index; */
/* } CACHE_ALIGNED; */

/* static struct print_buffer pbuf[NUM_CPU]; */

/* int print_str(int s1, int s2, int s3, int s4) */
/* { */
/* 	int *p; */
/* 	unsigned int j, len = 0, *index_ptr; */
/* 	char *s, *buf_ptr; */
/* 	struct print_buffer *pbuf_ptr = &pbuf[cos_cpuid()]; */

/* 	index_ptr = &(pbuf_ptr->index); */
/* 	buf_ptr = pbuf_ptr->buf; */
/* 	s = &buf_ptr[*index_ptr]; // the beginning of the buffer. */

/*  	p = (int *)&buf_ptr[*index_ptr]; */
/* 	(*index_ptr) += CHAR_PER_INV; */
/* 	if (unlikely(*index_ptr >= MAX_LEN)) {  */
/* 		cos_print("BUG", 4);  */
/* 		while (1); */
/* 		assert(0);  */
/* 	} */

/* 	/\* set the values in the array. *\/ */
/* 	p[0] = s1; */
/* 	p[1] = s2; */
/* 	p[2] = s3; */
/* 	p[3] = s4; */

/* 	for (j = 0; j < CHAR_PER_INV; j++) { */
/* 		if (s[j] == '\0') { */
/* 			len = *index_ptr - CHAR_PER_INV + j; */
/* 			break; */
/* 		} */
/* 	} */
	
/* 	if (j == CHAR_PER_INV) return 1; */

/* 	assert(len < MAX_LEN); */
/* 	assert(s[len] == '\0'); */
/* //	cos_print(":", 1); */
/* 	cos_print(buf_ptr, len); */
/* 	*index_ptr = 0; */

/* 	return 0; */
/* } */


static char foo[MAX_LEN];
int
print_str(char *s, unsigned int len)
{
	char n = *((char*)NULL);
	return (int)n;
}

union pstr {
	int  i[4];
	char c[16];
};

int
print_char(int len, int a, int b, int c)
{
	int maxlen = sizeof(int) * 3 + 1;
	union pstr str;

	if (len > maxlen-1) return -1;
	str.i[0] = a;
	str.i[1] = b;
	str.i[2] = c;
	str.c[len] = '\0';

	memcpy(foo, str.c, len+1);
	cos_print(foo, len);

	return 0;
}

void print_null(void)
{
	return;
}
