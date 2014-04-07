/**
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007
 * Modified by: Qi Wang, interwq@gwu.edu, 2014
 */

/* To optimize IPC performance, we put sysenter entry, dispatcher and
 * IPC path in a single page. Now we have everything declared in
 * inv.c as inline asm. */

.text
