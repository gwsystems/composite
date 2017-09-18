#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "micro_booter.h"
#include "rk_inv_api.h"

#define __rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

#define IN_PORT  9998
#define OUT_PORT 9999
#define MSG_SZ   16
#define TP_INFO_MS (unsigned long long)(5*1000)

extern int vmid;

static unsigned long long prev = 0, now = 0;
static unsigned long msg_count = 0;
static char msg[MSG_SZ + 1] = { '\0' };

static int
__test_udp_server(void)
{
	int fd, fdr;
	struct sockaddr_in soutput, sinput;
	int msg_size=MSG_SZ;
	int tp_counter = 0;

	soutput.sin_family      = AF_INET;
	soutput.sin_port        = htons(OUT_PORT);
//	PRINTC("%x\n", (unsigned int)soutput.sin_addr.s_addr);
	soutput.sin_addr.s_addr = htonl(INADDR_ANY);
	PRINTC("Sending to port %d\n", OUT_PORT);
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		PRINTC("Error Establishing socket\n");
		return -1;
	}
	PRINTC("fd for socket: %d\n", fd);
	if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		PRINTC("Error Establishing receive socket\n");
		return -1;
	}
	PRINTC("fd for receive socket: %d\n", fdr);

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(IN_PORT);
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);
	PRINTC("binding receive socket to port %d\n", IN_PORT);
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		PRINTC("binding receive socket\n");
		return -1;
	}

	__rdtscll(now);
	prev = now;

	do {
		struct sockaddr sa;
		socklen_t len;

		if (recvfrom(fdr, msg, msg_size, 0, &sa, &len) != msg_size) {
			PRINTC("read");
			continue;
		}
		//PRINTC("Received-msg: seqno:%u time:%llu\n", ((unsigned int *)msg)[0], ((unsigned long long *)msg)[1]);
		/* Reply to the sender */
		soutput.sin_addr.s_addr = ((struct sockaddr_in*)&sa)->sin_addr.s_addr;
		if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput)) < 0) {
			PRINTC("sendto");
			continue;
		}
		//PRINTC("Sent-msg: seqno:%u time:%llu\n", ((unsigned int *)msg)[0], ((unsigned long long *)msg)[1]);

		msg_count ++;
		__rdtscll(now);
	//	PRINTC("now:%llu prev:%llu %llu, cycs_per_msec:%d\n", now, prev, now - prev, cycs_per_msec);
		if ((now - prev) >= ((unsigned long long)cycs_per_usec * TP_INFO_MS * 1000)) {
			PRINTC("%d:Msgs processed:%lu, last seqno:%u\n", tp_counter++, msg_count, ((unsigned int *)msg)[0]);
			msg_count = 0;
			prev = now;
		}
	} while (1) ;

	return -1;
}

int
udpserv_main(void)
{
	rk_socketcall_init();
	
	PRINTC("%d: Starting udp-server [in:%d out:%d]\n", vmid, IN_PORT, OUT_PORT);
	__test_udp_server();

	return 0;
}
