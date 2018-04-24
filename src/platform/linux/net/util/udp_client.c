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

#define JPEG_REQ 77
#define SEND_SCRIPT 80
#define SCRIPT_RECV_ACK 100

unsigned int msg_sent = 0, msg_rcved;

void 
signal_handler(int signo)
{
	if (rcv) {
		tot = cnt = max = 0;
		min = (unsigned long long)-1;
		msg_rcved = 0;
	} else {
	
	}
	msg_sent = 0;
}

void 
do_recv_proc(int fd, int msg_sz)
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

char * 
build_script() 
{
        int i;

        char * sbuf = (char*) malloc(800*sizeof(char));
        char * pre = "echo -n '";
        char * post = "' | nc -4u -w1 192.168.137.174 2390";
	static int count = 0;
        memcpy(sbuf, pre, 9);
        for(i = 0; i < script[1]+3; i++) {
                snprintf(&sbuf[strlen(sbuf)], 20*sizeof(char), "%d ", script[i]);
		count += 20;
        }
        snprintf(&sbuf[strlen(sbuf)], 400*sizeof(char), "%s", post);

        return sbuf;
}

void
req_jpeg(void)
{
        system("avconv -rtsp_transport udp -i 'rtsp://192.168.248.102:554/onvif1' -f image2 -vframes 1 -pix_fmt yuvj420p hey.jpg");
}

int 
send_script() 
{
    	char * post = build_script();	
    	printf("post: %s \n", post);

        system(post);
        return 0;
}

int 
send_shutdown() 
{
        char* post = "echo -n 'stop' | nc -4u -w1 192.168.248.103 2390";
	printf("post: %s \n", post);
        system(post);
        
        return 0;
}

void 
start_timers()
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
unsigned char * buf;

int
read_jpeg(void)
{
	printf("reading in jpeg\n");
	fp = fopen("obstacleman.jpg", "rb");
	assert(fp);
	
	fseek(fp, 0, SEEK_END);
	jpg_s = ftell(fp);

	printf("jpeg size: %lu \n", jpg_s);
	rewind(fp);
	buf = (char *)malloc(sizeof(char)*(jpg_s));
	assert(buf);


	fread(buf, jpg_s, sizeof(unsigned char), fp);

	fclose(fp);
}

int foo = 0;

int 
main(int argc, char *argv[])
{
	int fd, fdr;
	struct sockaddr_in sa;
	int msg_size;
	char *msg;
	int sleep_val;

	//send_script();
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
		((unsigned int *)msg)[0] = 0;			
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
		//printf("msg: rcd: %d \n", ((unsigned char *)rcv_msg)[0]);

		/* Send jpeg in buf */
		unsigned long sent = 0;
		int b = 0;

		/* Server has requested an image */
		if (((unsigned char *)rcv_msg)[0] == JPEG_REQ) {
			printf("Server requested an image\n");	
			//req_jpeg();
			read_jpeg();
			printf("sending jpeg\n");
			((unsigned int *)msg)[0] = 79;			
			if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&sa, sizeof(sa)) < 0 &&
			    errno != EINTR) {
				return -1;
			}


			//printf("jpg_size: %lu \n", jpg_s);
			//printf("msg_size: %d \n", msg_size);
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
			
			printf("jpg sent: %lu  sent: %lu  \n", jpg_s, sent);	
			
		}
		else if (((unsigned char *)rcv_msg)[0] == SEND_SCRIPT) {

			/* Server is going to send a script */
			int j;
			printf("Recvd script\n");
			for (j = 0; j < 100 ; j ++) {
				if (((unsigned char *)rcv_msg)[j+1] == 66 ) break;
				script[j] = ((unsigned char *)rcv_msg)[j+1];
			}
			send_script();

			((unsigned char *)msg)[0] = SCRIPT_RECV_ACK;			
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

		} else if (((unsigned char *)rcv_msg)[0] == 81) {
			/* Shut Roomba Down */
			int j;
			printf("Recvd SHUT DOWN COMMAND\n");
			send_shutdown();
		} else {
//			printf("recvd message no instruc: %d \n", ((unsigned char *)rcv_msg)[0]);
		}
	
	}

	int j;
	printf("Recvd script\n");
	for (j = 0; j < 100 ; j ++) {
		printf("%d : %d\n", j, script[j]);
	}
	

	return 0;
}
