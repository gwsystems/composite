#define _GNU_SOURCE
#include <string.h>
#include <signal.h>

const char* strsignal(int sig) {
  if (sig<=SIGRTMAX)
    return sys_siglist[sig];
  else
    return "(unknown signal)";
}
