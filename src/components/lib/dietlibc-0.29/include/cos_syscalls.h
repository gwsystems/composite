#ifndef COS_SYSCALLS_H
#define COS_SYSCALLS_H

#include "../i386/syscalls.h"

#define SYSCALLS_NUM 288

typedef int (*cos_syscall_t)(void);

void libc_syscall_override(cos_syscall_t func, int syscallnum);

#endif
