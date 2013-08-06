#include "syscalls.h"

.text
.global mmap
.type	mmap,@function
mmap:
        movl    $__NR_mmap,%eax
        jmp     __cos_syscall
.Lende3:
.size mmap,.Lende3-mmap
