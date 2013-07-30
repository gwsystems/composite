#include "syscalls.h"

.text
.global mmap
.type	mmap,@function
mmap:
        movl    $__NR_mmap,%eax
        jmp     cos_syscalls(,%eax,4)
.Lende3:
.size mmap,.Lende3-mmap
