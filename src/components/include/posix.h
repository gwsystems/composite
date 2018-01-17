#ifndef POSIX_H
#define POSIX_H

#include <syscall.h>

#define SYSCALL_NUM_MAX 378
typedef long (*cos_syscall_t)(long a, long b, long c, long d, long e, long f);

/*
 * override a system call using this API. 
 * Note: override before the first occurance of the syscall. 
 *
 * @return: (TODO) errno if error. 
 */
int posix_syscall_override(cos_syscall_t fn, int syscall_num);

#endif /* POSIX_H */
