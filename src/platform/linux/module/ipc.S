/**
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007
 * Modified by: Qi Wang, interwq@gwu.edu, 2014
 */

/* null ptr deref */
#define ERROR_OUT \
	movl $0, %eax ; \
	movl (%eax), %eax

.text

kernel_ipc_syscall:
	ERROR_OUT

