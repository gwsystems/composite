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

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

#define _itr 10
volatile unsigned long long stsc;
volatile int done = 0;
unsigned long long timer_arr[_itr];


void *
timer_thd(void *data)
{
	int i = 0;

	while(1) {
		rdtscll(stsc);
		timer_arr[i] = stsc;
		if(done) {
			printf("Done reading packets!\n");
			break;
		}
		i = (i + 1) % _itr;
	}
	pthread_exit(NULL);
}

void *
recv_pkt(void *data)
{
	char **arguments = (char**) data;

	int msg_size = atoi(arguments[2]);
	int fdr, i, ret;
	char *msg;
	struct sockaddr_in sinput;

	msg = malloc(msg_size);
	printf("Message size is (%d)\n", msg_size);

    if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing receive socket");
		pthread_exit(NULL);;
	}

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(atoi(arguments[1]));
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);

	printf("binding receive socket to port %d\n", atoi(arguments[1]));
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		perror("binding receive socket");
		pthread_exit(NULL);
	}

	i = _itr;

	while(i > 0) {
		struct sockaddr sa;
		socklen_t len;

		len = sizeof(sa);

		ret = read(fdr, msg, 100);
		if (ret != msg_size && errno != EINTR) {
		  printf("ret (%d) errno (%d)\n", ret, errno);
			perror("read");
			pthread_exit(NULL);
		} 	
		printf("Message received!\n");
		i--;
	}
	done = 1;
	pthread_exit(NULL);
}

int 
main(int argc, char *argv[])
{
	// thread stuff
	int ret, i;
	pthread_t tid_timer, tid_recv;
	struct sched_param sp_timer, sp_recv;
	void *thd_ret;

	if (argc != 3) {
		printf("Usage: %s <in_port> <msg size>\n", argv[0]);
		return -1;
	}

	// Set up the timer and create a thread for it
	sp_timer.sched_priority = (sched_get_priority_max(SCHED_RR) - 1);
	if(pthread_create(&tid_timer, NULL, timer_thd, NULL) != 0) {
		perror("pthread create timer: ");
		return -1;
	}
	if(!pthread_setschedparam(tid_timer, SCHED_RR, &sp_timer)) {
		perror("pthread setsched timer: ");
		return -1;
	}
	printf("timer priority: (%d)\n", sp_timer.sched_priority);

	// Set up the receiver and create a thread for it
	sp_recv.sched_priority = sched_get_priority_max(SCHED_RR);
	if(pthread_create(&tid_recv, NULL, recv_pkt, (void *) argv) != 0) {
		printf("Error starting recv_pkt thread\n");
		return -1;
	}
	if(!pthread_setschedparam(tid_recv, SCHED_RR, &sp_recv)) {
		perror("pthread setsched timer: ");
		return -1;
	}
	printf("receiver priority: (%d)\n", sp_recv.sched_priority);

	// Just sleep until receiver finishes its iterations
	while(!done)
		sleep(1);

	printf("Done reading %d iterations!\n", _itr);

	// Read out the timer array
	for(i = 0; i < _itr; i++) {
		printf("time: %llu\n", timer_arr[i]);
	}
	pthread_join(tid_timer, &thd_ret);
	pthread_join(tid_recv, &thd_ret);

	return 0;
}
