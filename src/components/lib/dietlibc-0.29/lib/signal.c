#include <signal.h>

sighandler_t signal(int signum, sighandler_t action) {
  struct sigaction sa,oa;
  sa.sa_handler=action;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask,signum);
  sa.sa_flags=SA_NODEFER;
  if (sigaction(signum,&sa,&oa))
    return SIG_ERR;
  return oa.sa_handler;
}
