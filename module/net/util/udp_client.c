#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdlib.h>

#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

void construct_header(char *msg)
{
	unsigned long long time;
	static unsigned int count = 0;
	unsigned int *int_msg = (unsigned int *)msg;
	unsigned long long *ll_msg = (unsigned long long *)msg;
	
	rdtscll(time);
	int_msg[0] = count;
	ll_msg[1] = time;
	count++;

	return;
}

unsigned int msg_sent = 0;
void signal_handler(int signo)
{
	printf("%d messages sent per second.\n", msg_sent);
	msg_sent = 0;
}

void start_timers()
{
	struct itimerval itv;
	struct sigaction sa;

	sa.sa_handler = signal_handler;
	sa.sa_flags = 0;
	//sigemptyset(&sa.sa_mask);
	sigfillset(&sa.sa_mask);

	if (sigaction(SIGALRM, &sa, NULL)) {
		perror("Setting up alarm handler");
		exit(-1);
	}

	memset(&itv, 0, sizeof(itv));
	itv.it_value.tv_sec = 1;
	itv.it_interval.tv_sec = 1;

	if (setitimer(ITIMER_REAL, &itv, NULL)) {
		perror("setitimer; setting up sigalarm");
		return;
	}

	return;
}

int foo = 0;

int main(int argc, char *argv[])
{
	int fd;
	struct sockaddr_in sa;
	int msg_size;
	char *msg;
	struct timespec ts;
	int sleep_val;

	if (argc != 5) {
		printf("Usage: %s <ip> <port> <msg size> <sleep_val>\n", argv[0]);
		return -1;
	}

	sleep_val = atoi(argv[4]);
	ts.tv_nsec = sleep_val;
	ts.tv_sec = 0;

	msg_size = atoi(argv[3]);
	if (msg_size < (sizeof(unsigned long long)*2)) 
		msg_size = (sizeof(unsigned long long)*2);

	msg = malloc(msg_size);

	sa.sin_family = AF_INET;
	sa.sin_port = htons(atoi(argv[2]));
	sa.sin_addr.s_addr = inet_addr(argv[1]);

	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}

	start_timers();

	while (1) {
		int i;
		
		construct_header(msg);
		
		if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
		    errno != EINTR) {
			perror("sendto");
			return -1;
		}
		msg_sent++;
		
		for (i=0 ; i < sleep_val ; i++) foo++;
		//nanosleep(&ts, NULL);
	}

	return 0;
}
