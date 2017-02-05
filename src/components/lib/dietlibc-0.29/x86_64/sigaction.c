#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syscalls.h>

int __rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact, long nr);

static void restore_rt(void) {
  asm volatile ("syscall" : : "a" (__NR_rt_sigreturn));
}

int __libc_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int __libc_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  struct sigaction *newact = (struct sigaction *)act;
  if (act) {
	newact = alloca(sizeof(*newact));
	newact->sa_handler = act->sa_handler;
	newact->sa_flags = act->sa_flags | SA_RESTORER;
	newact->sa_restorer = &restore_rt;
	newact->sa_mask = act->sa_mask;
  }
  return __rt_sigaction(signum, newact, oldact, _NSIG/8);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
__attribute__((weak,alias("__libc_sigaction")));
