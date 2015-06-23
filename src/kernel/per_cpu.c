/**
 * Copyright 2012 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Qi Wang, interwq@gwu.edu, 2012
 */

#include "include/per_cpu.h"
#include "include/thd.h"

/* We need to access the current thread from ASM. Used in entry.S */
COS_SYSCALL struct thread *
cos_get_curr_thd_asm(void) { return cos_get_curr_thd(); }
