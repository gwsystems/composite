#include <assert.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include "cl_types.h"
#include "cl_macros.h"

#include <cos_config.h>
#ifdef LINUX_HIGHEST_PRIORITY
#define HIGHEST_PRIO 1
#endif


enum {PRINT_NONE = 0, PRINT_HIGH, PRINT_NORMAL, PRINT_DEBUG} print_lvl = PRINT_DEBUG;

#ifdef FAULT_SIGNAL
void segv_handler(int signo, siginfo_t *si, void *context) {
        ucontext_t *uc = context;
        struct sigcontext *sc = (struct sigcontext *)&uc->uc_mcontext;

        printl(PRINT_HIGH, "Segfault: Faulting address %p, ip: %lx\n", si->si_addr, sc->eip);
        exit(-1);
}
#endif

#ifdef ALRM_SIGNAL
void alrm_handler(int signo, siginfo_t *si, void *context) {
        printl(PRINT_HIGH, "Alarm! Time to exit!\n");
        exit(-1);
}
#endif


void
call_getrlimit(int id, char *name)
{
        struct rlimit rl;

        if (getrlimit(id, &rl)) {
                perror("getrlimit: "); printl(PRINT_HIGH, "\n");
                exit(-1);
        }
        /* printl(PRINT_HIGH, "rlimit for %s is %d:%d (inf %d)\n",  */
        /*        name, (int)rl.rlim_cur, (int)rl.rlim_max, (int)RLIM_INFINITY); */
}

void
call_setrlimit(int id, rlim_t c, rlim_t m)
{
        struct rlimit rl;

        rl.rlim_cur = c;
        rl.rlim_max = m;
        if (setrlimit(id, &rl)) {
                perror("getrlimit: "); printl(PRINT_HIGH, "\n");
                exit(-1);
        }
}

void
set_curr_affinity(u32_t cpu)
{
        int ret;
        cpu_set_t s;
        CPU_ZERO(&s);
        assert(cpu <= NUM_CPU - 1);
        CPU_SET(cpu, &s);
        ret = sched_setaffinity(0, sizeof(cpu_set_t), &s);
        assert(ret == 0);

        return;
}

void
set_prio(void)
{
        struct sched_param sp;

        call_getrlimit(RLIMIT_CPU, "CPU");
#ifdef RLIMIT_RTTIME
        call_getrlimit(RLIMIT_RTTIME, "RTTIME");
#endif
        call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
        call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
        call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
        call_getrlimit(RLIMIT_NICE, "NICE");

        if (sched_getparam(0, &sp) < 0) {
                perror("getparam: ");
                printl(PRINT_HIGH, "\n");
        }
        sp.sched_priority = sched_get_priority_max(SCHED_RR);
        if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
                perror("setscheduler: "); printl(PRINT_HIGH, "\n");
                exit(-1);
        }
        if (sched_getparam(0, &sp) < 0) {
                perror("getparam: ");
                printl(PRINT_HIGH, "\n");
        }
        assert(sp.sched_priority == sched_get_priority_max(SCHED_RR));

        return;
}

void
set_smp_affinity()
{
        char cmd[64];
        /* everything done is the python script. */
        sprintf(cmd, "python set_smp_affinity.py %d %d", NUM_CPU, getpid());
        system(cmd);
}


void
setup_thread(void)
{
#ifdef FAULT_SIGNAL
        struct sigaction sa;

        sa.sa_sigaction = segv_handler;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);
#endif

        set_smp_affinity();

#ifdef HIGHEST_PRIO
        set_prio();
#endif
#ifdef ALRM_SIGNAL
        //printf("pid %d\n", getpid()); getchar();
        {
                struct sigaction saa;

                saa.sa_sigaction = alrm_handler;
                saa.sa_flags = SA_SIGINFO;
                sigaction(SIGALRM, &saa, NULL);
                alarm(30);
        }
        while (1) ;
#endif
}
