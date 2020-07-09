#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sched.h>
#include <math.h>

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITR 1024 // # of iterations we want
volatile  unsigned long long stsc; // timer thread timestamp
volatile int done = 0;
unsigned long long timer_arr[ITR];

void *
timer_thd(void *data)
{
	while(1) {
		rdtscll(stsc);
		if(done) {
			break;
		}
	}
	pthread_exit(NULL);
}

void *
recv_pkt(void *data)
{
	char **arguments = (char**) data;

	int msg_size = atoi(arguments[2]);
	int fdr, i = ITR, j = 0, ret;
	unsigned long long tsc;
	char *msg;
	struct sockaddr_in sinput;

	msg = malloc(msg_size);
	printf("Message size is (%d)\n", msg_size);

	if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing receive socket");
		exit(-1);
	}
	    printf("udp FD (%d) created\n", fdr);

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(atoi(arguments[1]));
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);

	printf("binding receive socket to port %d\n", atoi(arguments[1]));
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		perror("binding receive socket");
		exit(-1);
	}
	
	while(i > 0) {
		ret = read(fdr, msg, msg_size);
		rdtscll(tsc);
		timer_arr[j] = tsc - stsc;
		j += 1;

		if (ret != msg_size && errno != EINTR) {
		  	printf("ret (%d) errno (%d)\n", ret, errno);
			perror("read");
			exit(-1);
		} 	

		i--;
	}
	done = 1;
	close(fdr);
	pthread_exit(NULL);
}

void
get_statistics()
{
	// For mean
	unsigned long long running_sum = 0, i;

	// For stddev
	double running_sdevsum = 0;
	long double sdev_arr[ITR];
	long double mean;
	double sdev;

	// Calculating the mean
	for(i = 1; i < ITR; i++) 
		running_sum += timer_arr[i];
	mean = running_sum / ITR;

	printf("Done reading %d iterations!\n", ITR);
	printf("The sum is  (%llu)\nThe mean is (%Lf)\n", running_sum, mean);

	// Calculating the standard deviation
	for(i = 0; i < ITR; i++) {
		sdev_arr[i] = timer_arr[i] - mean;
		running_sdevsum += (sdev_arr[i] * sdev_arr[i]);
	}
	running_sdevsum /= (ITR);
	sdev = sqrt(running_sdevsum);

	printf("The standard deviation is (%lf)\n", sdev);
}

int 
main(int argc, char *argv[])
{
	// thread stuff
	int ret, i;
	pthread_t tid_timer, tid_recv;
	struct sched_param sp_timer, sp_recv;
	struct rlimit rl;
	cpu_set_t mask;

	void *thd_ret;

	if (argc != 3) {
		printf("Usage: %s <in_port> <msg size>\n", argv[0]);
		return -1;
	}

	// Set up rlimits to allow more CPU usage
	rl.rlim_cur = RLIM_INFINITY;
	rl.rlim_max = RLIM_INFINITY;
	if(setrlimit(RLIMIT_CPU, &rl)) {
		perror("set rlimit: ");
		return -1;
	}
	printf("CPU limit removed\n");


	// Set up the receiver and create a thread for it
	sp_recv.sched_priority = sched_get_priority_max(SCHED_RR);
	if(pthread_create(&tid_recv, NULL, recv_pkt, (void *) argv) != 0) {
		printf("Error starting recv_pkt thread\n");
		return -1;
	}
	/*
	if(pthread_setschedparam(tid_recv, SCHED_RR, &sp_recv) != 0) {
		perror("pthread setsched recv: ");
		return -1;
	}
	printf("receiver priority: (%d)\n", sp_recv.sched_priority);
	*/
	// Set up the timer and create a thread for it
	sp_timer.sched_priority = (sched_get_priority_max(SCHED_RR) - 1);
	if(pthread_create(&tid_timer, NULL, timer_thd, NULL) != 0) {
		perror("pthread create timer: ");
		return -1;
	}
	/*
	if(pthread_setschedparam(tid_timer, SCHED_RR, &sp_timer) != 0) {
		perror("pthread setsched timer: ");
		return -1;
	}
	printf("timer priority: (%d)\n", sp_timer.sched_priority);
	*/

	// Set up processor affinity for both threads
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
 	if(pthread_setaffinity_np(tid_timer, sizeof(mask), &mask ) == -1 ) {
 		perror("setaffinity timer error: ");
 		return -1;
 	}
 	printf("Set affinity for timer thread\n");

 	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
 	if(pthread_setaffinity_np(tid_recv, sizeof(mask), &mask ) == -1 ) {
 		perror("setaffinity recv error: ");
 		return -1;
 	}
 	printf("Set affinity for recv thread\n");


	// We're done here; we'll wait for the threads to do their thing
	pthread_join(tid_recv, &thd_ret);
	pthread_join(tid_timer, &thd_ret);

	get_statistics();

	return 0;
}
