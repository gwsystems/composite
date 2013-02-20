#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define LINUX_TEST
#include "../../translator_ioctl.h"
#include "../../../../../kernel/include/shared/cos_types.h"
#include "../../../../../components/include/cringbuf.h"
#include "../../../../../kernel/include/shared/cos_config.h"

#define PROC_FILE "/proc/translator"
#define MAP_SIZE  COS_PRINT_MEM_SZ //(4096 * 256)
#define PRINT_CHUNK_SZ (4096*4)

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

struct cringbuf sharedbuf;

#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
static void call_getrlimit(int id, char *name)
{
	struct rlimit rl;

	if (getrlimit(id, &rl)) {
//		perror("getrlimit: "); printl(PRINT_HIGH, "\n");
		exit(-1);
	}		
//	printl(PRINT_HIGH, "rlimit for %s is %d:%d (inf %d)\n", 
//	       name, (int)rl.rlim_cur, (int)rl.rlim_max, (int)RLIM_INFINITY);
}

static void call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		perror("getrlimit: "); //printl(PRINT_HIGH, "\n");
		exit(-1);
	}		
}

void set_prio(void)
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
//		printl(PRINT_HIGH, "\n");
	}
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: "); //printl(PRINT_HIGH, "\n");
		exit(-1);
	}
	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		//printl(PRINT_HIGH, "\n");
	}
	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR));

	return;
}
#define ITER 1024
unsigned int meas[ITER];

int main(int argc, char **argv)
{
	int fd, i;
	void *a;
	char c;
	int channel;
	unsigned long long c_t, sum = 0, avg, dev = 0;

	if (argc > 1) return -1;
	set_prio();
	channel = COS_TRANS_SERVICE_PRINT;

	fd = open(PROC_FILE, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	trans_ioctl_set_channel(fd, channel);
	trans_ioctl_set_direction(fd, COS_TRANS_DIR_CTOL);
	a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (MAP_FAILED == a) {
		perror("mmap");
		exit(-1);
	}
	cringbuf_init(&sharedbuf, a, MAP_SIZE);
	
	for (i = 0; i < ITER; i++) {
		unsigned long long t, cost;
		/* wait for an event... */
//		printf("a\n");
		read(fd, &c, 1);
		rdtscll(t);
//		printf("b\n");

		cringbuf_consume(&sharedbuf, (char *)&c_t, 8);
//		t = (unsigned long) t;
//		c_t = (unsigned long) c_t;
		cost = t - c_t;
//		trans_ioctl_set_channel(fd, (unsigned long)diff);
//		printf("%d: s %llu, e %llu, cost %llu\n", i, c_t, t, cost);
		meas[i] = cost;
		sum += cost;
//		write(1, "up\n", 4);

		/* do { */
		/* 	amnt = cringbuf_consume(&sharedbuf, buf, PRINT_CHUNK_SZ); */
		/* 	write(1 , buf, amnt); /\* write to stdout *\/ */
		/* } while (amnt); */
	}
	avg = sum / ITER;
	for (i = 0 ; i < ITER ; i++) {
		unsigned long long diff = (meas[i] > avg) ? 
			meas[i] - avg : 
			avg - meas[i];
		dev += (diff*diff);
	}
	dev /= ITER;
	printf("deviation^2 = %llu\n", dev);

	printf("avg %llu\n", avg);

	if (munmap(a, MAP_SIZE) < 0) {
		perror("munmap");
		exit(-1);
	}
	/* if (write(fd, name, strlen(name)) < 0) { */
	/* 	perror("read"); */
	/* 	exit(-1); */
	/* } */
	
	return 0;
}
