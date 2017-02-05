#include <signal.h>

int __rt_sigqueueinfo(int pid, int sig, siginfo_t *info);

int sigqueueinfo(int pid, int sig, siginfo_t *info) {
  return __rt_sigqueueinfo(pid, sig, info);
}
