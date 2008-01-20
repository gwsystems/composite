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

#include <asm_ipc_defs.h>

#define STR_SIZE 1024

char *header = 
".text\n"
".globl c_cap_stub\n"
".align 16\n"
"ocap_stub:\n\t"
"pushl %%ebp\n\t"
"pushl %%ebx\n\t" 
"pushl %%ecx\n\t" 
"pushl %%edx\n\t" 
"movl %%esp, %%ebp\n\t" 
"movl $after_inv, %%ecx\n\t" 
"movl %d(%%eax), %%eax\n\t" 
"shll $0x10,%%eax /* because of hijacking, composite assumes shifted syscall #s */\n\t"
"sysenter\n" 
"after_inv:\n\t" 
"popl %%edx\n\t" 
"popl %%ecx\n\t" 
"popl %%ebx\n\t" 
"popl %%ebp\n\n"; 

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

/* would be nice to fit this into 1 cache line */
char *fn_string = 
".globl %s\n"
".align 16\n"
"%s:\n\t"
/* get the cap table */
"movl $ST_user_caps, %%eax\n\t"
"/* eax now holds the **usr_inv_cap */\n\t"
"/* get the specific *usr_inv_cap we are interested in */\n\t"
"addl $%d, %%eax\n\t"
/* are we ST, hardcoded as 0? */
/*"cmpl $0, %d(%%eax)\n\t"*/
/*"jne 1f\n\t"*/
/* static branch predict will go here */
/* invocation count inc */
"/* If we would overflow the invocation count, don't count */\n\t"
"cmpl $(~0), %d(%%eax)\n\t"
"je 1f\n\t"
"/* Static branch prediction will go here: incriment invocation cnt */\n\t"
"incl %d(%%eax)\n" /* why is this 4 cycles? how aren't we using the parallelism? */
/*"jmp *%d(%%eax)\n"*/
"1: \n\t"
/*"pushl $ST_inv_stk\n\t"*/
"/* Call the invocation fn; either direct inv, or stub as set by kernel */\n\t"
"jmp *%d(%%eax)\n";
/* 
 * Note that in either case, %eax holds the ptr to the usr_inv_cap:
 * useful as that entry holds the kernel version of the capability.
 */

char *footer =
".data\n"
".align 32\n"
".globl ST_user_caps\n"
"ST_user_caps:\n"
".rep %d\n"
".long 4\n"
".endr\n"
"ST_user_caps_end:\n"


/* 4 4K stacks */
/*".globl cos_static_stack\n"
".align 32\n"
"cos_static_stack:\n"
".rep 4096\n"
".long 4\n"
".endr\n"

".section .inv_stk\n"
".globl ST_invocation_stack\n"
"ST_invocation_stack:\n"
".long\n"
".globl ST_inv_stk_size\n"
"ST_inv_stk_size:\n"
".long\n"
*/
;


static inline void create_stanza(char *output, int len, char *template, 
				 char *fn_name, int cap_num)
{
	int ret;

	ret = snprintf(output, len, template, fn_name, fn_name, 
		       cap_num*SIZEOFUSERCAP, /*INVFN,*/ INVOCATIONCNT, INVOCATIONCNT, /*ENTRYFN,*/ INVFN);

	if (ret == len) {
		fprintf(stderr, "Function name %s too long: string overrun.\n", fn_name);
		exit(-1);
	}

	return;
}

int main(int argc, char *argv[])
{
	char *product;
	char *delim = ",";
	char *tok, *fns;
	unsigned int cap_no = 1;
	int len, ret;
	char h[STR_SIZE];

	if (argc != 2 && argc != 1) {
		printf("Usage: %s <nothing OR string of comma-separated functions in trusted service's API>", argv[0]);
		return -1;
	}

	ret = snprintf(h, STR_SIZE, header, CAPNUM);
	if (ret == STR_SIZE) {
		fprintf(stderr, "Did not allocate enough room for header.\n");
		exit(-1);
	}
	printf("%s", h);

	if (argc == 2) {
		/* conservative amount of space for fn names: ~500 chars */
		len = strlen(fn_string)+STR_SIZE;
		product = malloc(len);
		fns = argv[1];
		
		/* NOTE: strtok not thread safe... */
		tok = strtok(fns, delim);
		
		do {
			create_stanza(product, len, fn_string, tok, cap_no);
			printf("%s\n\n", product);
			
			tok = strtok(NULL, delim);
			cap_no++;
		} while (tok != '\0');
	}

	/* 
	 * Make the static capability table.  cap_no because we need
	 * an entry per static capability made.  /4 because we are
	 * repeating the occurance of 4 bytes, not one (see footer).
	 */
	printf(footer, ((cap_no)*SIZEOFUSERCAP)/4);

	return 0;
}
