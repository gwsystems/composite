#include <stdio.h>
#include <stddef.h>

#include <thread.h>
#include <spd.h>
#include <ipc.h>

int main(void)
{
	int regs_offset = offsetof(struct thread, regs);

	printf("#define THD_REGS %d\n", regs_offset);
}
