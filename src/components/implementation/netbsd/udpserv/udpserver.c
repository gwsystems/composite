#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <rk_inv_api.h>
#include <micro_booter.h>
#include <rumpcalls.h>
#include <cos_types.h>
#include <rk.h>

#define IN_PORT  9998
#define OUT_PORT 9999
#define MSG_SZ   16
#define TP_INFO_MS (unsigned long long)(10*1000) //5secs
#define HPET_REQ_US (100*1000) //100ms
#define HPET_REQ_BUDGET_US (500) //0.5ms

extern int vmid;
extern struct cos_component_information cos_comp_info;

static char __msg[MSG_SZ + 1] = { '\0' };

static int
__test_udp_server(void)
{
	int fd, fdr;
	struct sockaddr_in soutput, sinput;
	int msg_size=MSG_SZ;
	int tp_counter = 0;

	soutput.sin_family      = AF_INET;
	soutput.sin_port        = htons(OUT_PORT);
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

	do {
		struct sockaddr sa;
		socklen_t len = sizeof(struct sockaddr);

		if (recvfrom(fdr, __msg, msg_size, 0, &sa, &len) != msg_size) {
			PRINTC("read");
			continue;
		}
		//PRINTC("Received-msg: seqno:%u time:%llu\n", ((unsigned int *)__msg)[0], ((unsigned long long *)__msg)[1]);
		/* Reply to the sender */
		soutput.sin_addr.s_addr = ((struct sockaddr_in*)&sa)->sin_addr.s_addr;
		if (sendto(fd, __msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput)) < 0) {
			PRINTC("sendto");
			continue;
		}

		//PRINTC("Sent-msg: seqno:%u time:%llu\n", ((unsigned int *)__msg)[0], ((unsigned long long *)__msg)[1]);

	} while (1) ;

	return -1;
}

int
udpserv_main(void)
{
	rk_socketcall_init();

	PRINTC("Starting udp-server [in:%d out:%d]\n", IN_PORT, OUT_PORT);
	__test_udp_server();

	return 0;
}

void
cos_init(void)
{
	printc("Welcome to the udpserver component\n");
	printc("cos_component_information spdid: %ld\n", cos_comp_info.cos_this_spd_id);

	/* Test RK entry */
	printc("calling rk_inv_entry\n");
	rk_entry(RK_GET_BOOT_DONE, 0, 0, 0);
	test_entry(0, 1, 2, 3);

	/* Spinning */
	while(1);
}
