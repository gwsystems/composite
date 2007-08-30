#include <alloca.h>
#include <signal.h>
#include "i386/syscalls.h"

int __rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact, long nr);

void __restore_rt(void);

#define RESTORE(name, syscall) \
asm                                             \
  (                                             \
   ".text\n"                                    \
   "    .align 8\n"                             \
   "__" #name ":\n"                             \
   "    popl %eax\n"                            \
   "    movl $" #syscall ", %eax\n"             \
   "    int  $0x80"                             \
   );

//RESTORE (restore_rt, __NR_rt_sigreturn)
//RESTORE (restore, __NR_sigreturn)

extern void restore_rt (void) asm ("__restore_rt");
extern void restore (void) asm ("__restore");

int __libc_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int __libc_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  struct sigaction *newact = (struct sigaction *)act;
  if (act) {
    newact = alloca(sizeof(*newact));
    newact->sa_handler = act->sa_handler;
    newact->sa_flags = act->sa_flags | SA_RESTORER;
    newact->sa_restorer = __restore_rt;
    newact->sa_mask = act->sa_mask;
  }
  return __rt_sigaction(signum, newact, oldact, _NSIG/8);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
__attribute__((weak,alias("__libc_sigaction")));

