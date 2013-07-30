#include "../../include/cos_component.h"
#include "../include/cos_syscalls.h"

__attribute__((weak)) int default_syscall(void);
cos_syscall_t cos_syscalls[SYSCALLS_NUM] = {[0 ... SYSCALLS_NUM - 1] default_syscall};

/*
CCTOR static void
cos_syscalls_init()
{
        int i;
        for(i = 0; i < SYSCALLS_NUM; i++)
        {
                cos_syscalls[i] = default_syscall;
        }

        return;
}
*/

void
libc_syscall_override(cos_syscall_t fn, int syscallnum)
{
        cos_syscalls[syscallnum] = fn;
        return;
}

/**
 * make it weak so we can have an overriden function in the component to call printc
 */
__attribute__((weak)) int
default_syscall()
{
        return *(int*)NULL;
}
