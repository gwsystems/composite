#include "syscalls.h"
.text
.type   __restore_rt,@function
.global __restore_rt

__restore_rt:
	popl %eax
	mov $__NR_sigreturn,%eax
	int $0x80

.Lend:
.size    __restore_rt,..Lend-__restore_rt
