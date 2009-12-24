/*
  Copyright (C) 2001, 2002 Thomas M. Ogrisegg

  This is free software. You can redistribute and
  modify it under the terms of the GNU General Public
  Public License.

  This file is part of the hppa-Port of dietlibc

  mmap.S
    Wrapper for the mmap(2) Systemcall
*/

#include "syscalls.h"

.level 1.1
.globl mmap
.type mmap,@function
mmap:
	/* Just load the arguments from the
	   stack into %r21 and %r22  */
	ldw -56(%sr0, %sp), %r21 ; 0x78
	ldw -52(%sr0, %sp), %r22 ; 0x74
	b __unified_syscall
	ldi __NR_mmap, %r20
