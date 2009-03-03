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

#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

unsigned long long max = 0, min = (unsigned long long)-1, tot = 0, cnt = 0;
int rcv = 0;
char *rcv_msg;

int conns, *conn_fds;

void construct_header(char *msg)
{
	static unsigned int seqno = 1;
	unsigned long long time;
	unsigned int *int_msg = (unsigned int *)msg;
	unsigned long long *ll_msg = (unsigned long long *)msg;
	
	rdtscll(time);
	int_msg[0] = seqno;
	ll_msg[1] = time;
//	printf("sending message %d with timestamp %lld\n", seqno, time);
	seqno++;

	return;
}

unsigned int msg_sent = 0, msg_rcved;
void signal_handler(int signo)
{
	printf("Messages sent/sec: %d", msg_sent);
	if (rcv) {
		printf(", avg response time: %lld, WC: %lld, min: %lld, received/sent:%d\n", 
		       cnt == 0 ? 0 : tot/cnt, 
		       max, min, 
		       msg_sent == 0 ? 0 : (unsigned int)(msg_rcved*100)/(unsigned int)msg_sent);
		tot = cnt = max = 0;
		min = (unsigned long long)-1;
		msg_rcved = 0;
	} else {
		printf("\n");
	}
	fflush(stdout);
	msg_sent = 0;
}

int socket_nonblock(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
	{
		perror("retrieving flags for socket");
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		perror("setting socket's flags to nonblocking");
		return -1;
	}
	return 0;
}

void do_recv_proc(int fd, int msg_sz)
{
	int ret;

	/* Keep on reading while there are more packets */
	do {
		if ((ret = recv(fd, rcv_msg, msg_sz, MSG_DONTWAIT)) == msg_sz) {
			unsigned long long curr, amnt;
			unsigned int *seqno_ptr = (unsigned int *)rcv_msg, seqno;
			unsigned long long *prev_ptr = (unsigned long long *)rcv_msg, prev;
	
			rcv = 1;
			rdtscll(curr);
			seqno = seqno_ptr[0];
			prev  = prev_ptr[1];
//			printf("Received message %d with stamp %lld @ %lld\n", seqno, prev, curr);
			amnt = curr - prev;
			tot += amnt;
			cnt++;
			min = min < amnt ? min : amnt;
			max = max > amnt ? max : amnt;
			msg_rcved++;
		}
		if (-1 == ret && EAGAIN != errno) {
			perror("Reading from recv socket");
		}
	} while (-1 != ret);
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
		fflush(stdout);
		exit(-1);
	}
	memset(&itv, 0, sizeof(itv));
	itv.it_value.tv_sec = 1;
	itv.it_interval.tv_sec = 1;

	if (setitimer(ITIMER_REAL, &itv, NULL)) {
		perror("setitimer; setting up sigalarm");
		fflush(stdout);
		return;
	}
	return;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in sa;
	char *msg;
	int sleep_val, i, msg_size;

	if (argc != 6) {
		printf("Usage: %s <ip> <port> <msg size> <sleep_val> <conns>\n", argv[0]);
		return -1;
	}
	sleep_val = atoi(argv[4]);
	sleep_val = sleep_val == 0 ? 1 : sleep_val;

	msg_size = atoi(argv[3]);
	msg_size = msg_size < (sizeof(unsigned long long)*2) ? (sizeof(unsigned long long)*2) : msg_size;
	msg      = malloc(msg_size);
	rcv_msg  = malloc(msg_size);

	conns    = atoi(argv[5]);
	conn_fds = malloc(conns * sizeof(int));

	if (!conn_fds || !msg || !rcv_msg) return -1;
	
	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(atoi(argv[2]));
	sa.sin_addr.s_addr = inet_addr(argv[1]);
	for (i = 0 ; i < conns ; i++) {
		if ((conn_fds[i] = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
			perror("Establishing socket");
			fflush(stdout);
			return -1;
		}
		if (socket_nonblock(conn_fds[i])) return -1;
		if (connect(conn_fds[i], (struct sockaddr*)&sa, sizeof(sa)) &&
		    errno != EINPROGRESS) {
			perror("connecting");
			return -1;
		}
	}
	printf("Start communication\n");
	fflush(stdout);
	start_timers();
	while (1) {
		int i, j;

		construct_header(msg);
		
		for (i = 0 ; i < conns ; i++) {
			if (write(conn_fds[i], msg, msg_size) < 0 &&
			    errno != EINTR && errno != EAGAIN) {
				perror("sendto");
				fflush(stdout);
				return -1;
			}
			if (errno != EINTR || errno != EAGAIN) {
				msg_sent++;
			}
			for (j = 0 ; j < sleep_val ; j++) {
				do_recv_proc(conn_fds[i], msg_size);
			}
		}
		//nanosleep(&ts, NULL);
	}

	return 0;
}

