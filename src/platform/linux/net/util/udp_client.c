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
#include <assert.h>

#include <sys/time.h>
#include <signal.h>
#include <time.h>

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

unsigned long long max = 0, min = (unsigned long long)-1, tot = 0, cnt = 0;
int rcv = 0;
unsigned char *rcv_msg;
int script[100];

#define MSG_SZ 31
#define JPEG_REQ 77

void construct_header(char *msg)
{
	static unsigned int seqno = 1;
	unsigned long long time;
	unsigned int *int_msg = (unsigned int *)msg;
	unsigned long long *ll_msg = (unsigned long long *)msg;

	rdtscll(time);
	int_msg[0] = seqno;
	int_msg[1] = 9;
	ll_msg[2] = time;
//	printf("sending message %d with timestamp %lld\n", seqno, time);
	seqno++;

	return;
}

unsigned int msg_sent = 0, msg_rcved;
void signal_handler(int signo)
{
	//printf("Messages sent/sec: %d", msg_sent);
	if (rcv) {
	//	printf(", avg response time: %lld, WC: %lld, min: %lld, received/sent:%d\n", 
	//	       cnt == 0 ? 0 : tot/cnt, 
	//	       max, min, 
	//	       msg_sent == 0 ? 0 : (unsigned int)(msg_rcved*100)/(unsigned int)msg_sent);
		tot = cnt = max = 0;
		min = (unsigned long long)-1;
		msg_rcved = 0;
	} else {
	//	printf("\n");
	}
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

void
create_wifi_str(void )
{

	char str[80];
	char *new = "4";
	strcpy(str, "echo -n '");
	strcat(str, new);
	strcat(str, "' | nc -4u -w1 192.168.137.51 2390");
	
	/*length 44?*/
	printf("str: %s \n", str);
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

FILE *fp;
unsigned long jpg_s;
char * buf;

int
read_jpeg(void)
{
	printf("reading in jpeg\n");
	fp = fopen("greenroomba.jpg", "rb");
	assert(fp);
	printf("fseek\n");
	fseek(fp, 0, SEEK_END);
	jpg_s = ftell(fp);
	printf("jpeg size: %lu \n", jpg_s);
	rewind(fp);

	buf = (char *)malloc(sizeof(char)*(jpg_s));
	
	char s;
	int num_read = 0;
	while(( num_read = fread(&s, 1, 1, fp))) {
		strncat(buf, &s, 1);
	}

	fclose(fp);
}

int foo = 0;

int main(int argc, char *argv[])
{
	int fd, fdr;
	struct sockaddr_in sa;
	int msg_size;
	char *msg;
	int sleep_val;

	create_wifi_str();

	if (argc != 5 && argc != 6) {
		printf("Usage: %s <ip> <port> <msg size> <sleep_val> <opt:rcv_port>\n", argv[0]);
		return -1;
	}
	sleep_val = atoi(argv[4]);
	sleep_val = sleep_val == 0 ? 1 : sleep_val;

	msg_size = atoi(argv[3]);
	msg_size = msg_size < (sizeof(unsigned long long)*2) ? (sizeof(unsigned long long)*2) : msg_size;
	msg     = malloc(msg_size);
	rcv_msg = malloc(msg_size);

	sa.sin_family      = AF_INET;
	sa.sin_port        = htons(atoi(argv[2]));
	sa.sin_addr.s_addr = inet_addr(argv[1]);
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}

	if (argc == 6) {
		struct sockaddr_in si;

		if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
			perror("Establishing receive socket");
			return -1;
		}
		/*if (socket_nonblock(fdr)) {
			return -1;
		}*/
		memset(&si, 0, sizeof(si));
		si.sin_family      = AF_INET;
		si.sin_port        = htons(atoi(argv[5]));
		si.sin_addr.s_addr = htonl(INADDR_ANY);
		printf("Binding the receive socket to port %d\n", atoi(argv[5]));
		if (bind(fdr, (struct sockaddr *)&si, sizeof(si))) {
			perror("binding receive socket");
			return -1;
		}
		rcv = 1;
	}

	start_timers();

	while (1) {
		int i;
		
		if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
		    errno != EINTR) {
			perror("sendto");
			return -1;
		}
		msg_sent++;
		
		for (i=0 ; i < sleep_val ; i++) {
			if (argc == 6) {
				do_recv_proc(fdr, msg_size);
			}
			foo++;
		}

		/* Send jpeg in buf */
		unsigned long sent = 0;
		int b = 0;

		/* Server has requested an image */
		if (((unsigned char *)rcv_msg)[0] == JPEG_REQ) {
			printf("Server requested an image\n");	
			read_jpeg();

			((unsigned int *)msg)[0] = 79;			
			if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
			    errno != EINTR) {
				return -1;
			}


			printf("jpg_size: %lu \n", jpg_s);
			printf("msg_size: %d \n", msg_size);
			while ( (sent+msg_size) < jpg_s) {
				for (b = 0; b < msg_size; b++) {
					msg[b] = buf[sent + b];
				}
				if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
				    errno != EINTR) {
					return -1;
				}

				for (i=0 ; i < sleep_val ; i++) {
					if (argc == 6) {
						do_recv_proc(fdr, msg_size);
					}
					foo++;
				}
				sent+= msg_size;
				//printf("%lu : %02x , ", sent, (unsigned char)msg[7]);
			}	
			

			for (b = 0; b < (jpg_s - sent); b++) {
				msg[b] = buf[sent + b];
			}
			sent += b;

			if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
			    errno != EINTR) {
				return -1;
			}
		
			printf("jpg_size: %lu  sent: %lu  \n", jpg_s, sent);	
			
			for (i=0 ; i < sleep_val ; i++) {
				if (argc == 6) {
					do_recv_proc(fdr, msg_size);
				}
				foo++;
			}

			//break;
		}
	
	}

	/* For script sending/recving */	
//	while (1) {
//		int i;
//		
//		if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
//		    errno != EINTR) {
//			perror("sendto");
//			return -1;
//		}
//		msg_sent++;
//		
//		for (i=0 ; i < sleep_val ; i++) {
//			if (argc == 6) {
//				do_recv_proc(fdr, msg_size);
//			}
//			foo++;
//		}
//
//		int j = 0;
//		int l = 1;
//		int script_num = ((unsigned char *)rcv_msg)[0];
//
//		/* Store message into local script at corresponding place */	
//		for (j = MSG_SZ*script_num; j < (MSG_SZ*script_num)+MSG_SZ ; j ++) {
//			if (((unsigned char *)rcv_msg)[l] == 66 ) break;
//			script[j] = ((unsigned char *)rcv_msg)[l];
//			l++;
//		}
//
//		/* Print entirety of recieved script */
//		if (((unsigned char *)rcv_msg)[l] == 66 ) {
//			int k = 0;
//			for (k = 0; k < j; k++) {
//				if (k%MSG_SZ == 0 && k != 0) printf("\n");
//				printf("%d, ", script[k]) ;
//			}
//	
//			((unsigned int *)msg)[0] = 99;			
//			if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
//			    errno != EINTR) {
//				perror("sendto");
//				return -1;
//			}
//			free(msg);
//			free(rcv_msg);
//			break;
//		}
//	}
	

	return 0;
}
