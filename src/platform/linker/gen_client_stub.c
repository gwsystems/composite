/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

#include <consts.h>

#define STR_SIZE 1024 * 4
#define FN_NAME_SZ 256
#define UCAP_EXT "_cos_ucap"

/*
 * FIXME: This is all wrong.  Really.
 *
 * We should really do this:
 *
 * 1) Load the address of the specific capability, or of the specific
 *    counter, or of the specific fn ptr into eax
 * 2) perform specific operations on these memory addresses directly.
 * 3) call function ptr directly
 *
 * This will include adding labels into the data in the footer, so
 * that we can link the specific text for capability access to the
 * data.
 */

char *fn_string = ".text\n"
                  ".globl %s\n"
                  ".align 16\n"
                  "%s:\n\t"
                  /* get the cap table */
                  /* "movl $ST_user_caps, %%eax\n\t" */
                  /* "/\* eax now holds the **usr_inv_cap *\/\n\t" */
                  /* "/\* get the specific *usr_inv_cap we are interested in *\/\n\t" */
                  /* "addl $%d, %%eax\n\t" */
                  "movl $%s, %%eax\n\t"
                  /*"movl (%%eax), %%eax\n\t"
                    "ret\n\t"*/
                  /* are we ST, hardcoded as 0? */
                  /*"cmpl $0, %d(%%eax)\n\t"*/
                  /*"jne 1f\n\t"*/
                  /* static branch predict will go here */
                  /* Comment out the inv cnt for multicore performance. */
                  /* invocation count inc */
                  /* If we would overflow the invocation count, don't count */
                  /* "cmpl $(~0), %d(%%eax)\n\t" */
                  /* "je 1f\n\t" */
                  /* "/\* Static branch prediction will go here: incriment invocation cnt *\/\n\t" */
                  /* "incl %d(%%eax)\n" */
                  /* Inv cnt removed for multicore performance. */
                  // The following approach works too and avoids the branch...but has the same cost.
                  /*"incl %d(%%eax)\n\t" */ /* why is this 4 cycles? how aren't we using the parallelism? */
                  /*"andl $0x7FFFFFFF, %d(%%eax)\n\t"*/
                  /*"jmp *%d(%%eax)\n"*/
                  "1: \n\t"
                  /*"pushl $ST_inv_stk\n\t"*/
                  "/* Call the invocation fn; either direct inv, or stub as set by kernel */\n\t"
                  "jmp *%d(%%eax)\n";
/*
 * Note that in either case, %eax holds the ptr to the usr_inv_cap:
 * useful as that entry holds the kernel version of the capability.
 */

/*

fn:
  movl $fn_user_cap_addr, %eax
  cmpl $(~0), invocation_cnt_offset(%eax)
  je full
  incl invocation_cnt_offset(%eax)
full:
  jmp *fn_ptr_offset(%eax)

*/

char *footer1 = ".section .kmem\n"
                ".align 4096\n"
                ".globl ST_user_caps\n"
                "ST_user_caps:\n\t"
                ".rep " UCAP_SZ_STR "\n\t"
                ".long 0\n\t"
                ".endr\n"; /* take up a whole cap slot for cap 0 */

char *footer2 = ".align 16\n"
                "ST_user_caps_end:\n\t"
                ".long 0\n";

char *cap_data = ".align 16\n"
                 ".globl %s\n"
                 "%s:\n\t"
                 ".rep " UCAP_SZ_STR "\n\t"
                 ".long 0\n\t"
                 ".endr\n";

static char *
string_to_token(char *output, char *str, int token, int maxlen)
{
	char *end;
	int   len;

	end = strchr(str, token);
	if (NULL == end) {
		strcpy(output, str);
		return NULL;
	}

	len = end - str;
	if (len >= maxlen) {
		fprintf(stderr, "function name starting at %s too long\n", str);
		exit(-1);
	}
	strncpy(output, str, len);
	output[len] = '\0';
	return end + 1;
}

static inline void
create_stanza(char *output, int len, char *fn_name, int cap_num)
{
	int  ret;
	char ucap_name[FN_NAME_SZ];

	sprintf(ucap_name, "%s" UCAP_EXT, fn_name);
	ret = snprintf(output, len, fn_string, fn_name, fn_name, ucap_name /*cap_num*SIZEOFUSERCAP*/,
	               /* INVOCATIONCNT, INVOCATIONCNT, */ /*ENTRYFN,*/ INVFN);

	if (ret == len) {
		fprintf(stderr, "Function name %s too long: string overrun.\n", fn_name);
		exit(-1);
	}

	return;
}

static inline void
create_cap_data(char *output, int len, char *name)
{
	int ret;

	ret = snprintf(output, len, cap_data, name, name);
	if (ret == len) {
		fprintf(stderr, "Function name %s too long: string overrun in cap data production\n", name);
		exit(-1);
	}
}

int
main(int argc, char *argv[])
{
	char *       product;
	char *       fns;
	unsigned int cap_no = 1;
	int          len;

	if (argc != 2 && argc != 1) {
		printf("Usage: %s <nothing OR string of comma-separated functions in trusted service's API>", argv[0]);
		return -1;
	}

	if (argc == 2) {
		char  fn_name[FN_NAME_SZ];
		char *orig_fns;

		/* conservative amount of space for fn names: ~500 chars */
		len     = strlen(fn_string) + STR_SIZE;
		product = malloc(len);
		assert(product);
		orig_fns = fns = argv[1];

		while (NULL != fns) {
			fns = string_to_token(fn_name, fns, ',', FN_NAME_SZ);
			create_stanza(product, len, fn_name, cap_no);
			printf("%s\n", product);
			cap_no++;
		}
		fns = orig_fns;
		printf("%s", footer1);
		while (NULL != fns) {
			char new_name[FN_NAME_SZ] = "\n";

			fns = string_to_token(fn_name, fns, ',', FN_NAME_SZ);
			sprintf(new_name, "%s" UCAP_EXT, fn_name);
			create_cap_data(product, len, new_name);
			printf("%s\n", product);
		}
		printf("%s", footer2);
	} else {
		printf("%s", footer1);
		printf("%s", footer2);
	}

	/*
	 * Make the static capability table.  cap_no because we need
	 * an entry per static capability made.  /4 because we are
	 * repeating the occurance of 4 bytes, not one (see footer).
	 */
	//	printf(footer, ((cap_no)*SIZEOFUSERCAP)/4);

	return 0;
}
