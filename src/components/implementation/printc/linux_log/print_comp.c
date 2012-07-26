#include <cos_component.h>

#include <stdio.h>
#include <string.h>

#include <printc.h>
//#include <cos_debug.h>

struct print_buffer {
	char foo[MAX_LEN];
	unsigned int index;
} CACHE_ALIGNED;

static struct print_buffer pbuf[MAX_NUM_CPU];

int print_str(int s1, int s2, int s3, int s4)
{
	int *p[4];
	unsigned int i, j, len = 0;
	char *s;

	s = &pbuf[cos_cpuid()].foo[pbuf[cos_cpuid()].index]; // the beginning of the buffer.

	for (i = 0; i < PARAMS_PER_INV; i++) {
		p[i] = (int *)&pbuf[cos_cpuid()].foo[pbuf[cos_cpuid()].index];
		pbuf[cos_cpuid()].index += CHAR_PER_INT;
	}

//	assert(pbuf[cos_cpuid()].index < MAX_LEN);

	*p[0] = s1;
	*p[1] = s2;
	*p[2] = s3;
	*p[3] = s4;

	for (j = 0; j < CHAR_PER_INV; j++) {
		if (s[j] == '\0') {
			len = pbuf[cos_cpuid()].index - CHAR_PER_INV + j;
			break;
		}
	}
	
	if (j == CHAR_PER_INV)
		goto pending_print;

	cos_print(pbuf[cos_cpuid()].foo, len);
	pbuf[cos_cpuid()].index = 0;

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

