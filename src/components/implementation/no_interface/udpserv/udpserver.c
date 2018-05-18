#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <rk_libc_override.h>
#include <rk.h>
#include <rk_inv.h>
#include <memmgr.h>
#include <schedinit.h>

#define IN_PORT  9998
#define OUT_PORT 9999
#define MSG_SZ   16
#define TP_INFO_MS (unsigned long long)(10*1000) //5secs
#define HPET_REQ_US (100*1000) //100ms
#define HPET_REQ_BUDGET_US (500) //0.5ms

int spdid;
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
	cycles_t prev = 0, now = 0;

	soutput.sin_family      = AF_INET;
	soutput.sin_port        = htons(OUT_PORT);
	soutput.sin_addr.s_addr = htonl(INADDR_ANY);
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		printc("Error Establishing socket\n");
		return -1;
	}
	if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		printc("Error Establishing receive socket\n");
		return -1;
	}

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(IN_PORT);
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		printc("binding receive socket\n");
		return -1;
	}

	rdtscll(prev);
	do {
		struct sockaddr sa;
		socklen_t len = sizeof(struct sockaddr);

		if (recvfrom(fdr, __msg, msg_size, 0, &sa, &len) != msg_size) {
			printc("read");
			continue;
		}
//		printc("Received-msg: seqno:%u time:%llu\n", ((unsigned int *)__msg)[0], ((unsigned long long *)__msg)[1]);
		/* Reply to the sender */
		soutput.sin_addr.s_addr = ((struct sockaddr_in*)&sa)->sin_addr.s_addr;
		if (sendto(fd, __msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput)) < 0) {
			printc("sendto");
			continue;
		}
		tp_counter++;

		if (tp_counter == 1000) {
			rdtscll(now);
			PRINTC("Sent/rcvd %d in %llu cycles\n", tp_counter, now - prev);

			prev = now;
			tp_counter = 0;
		}

//		printc("Sent-msg: seqno:%u time:%llu\n", ((unsigned int *)__msg)[0], ((unsigned long long *)__msg)[1]);

	} while (1) ;

	return -1;
}

int
udpserv_main(void)
{
	rk_libcmod_init();
	schedinit_child();

	printc("Starting udp-server [in:%d out:%d]\n", IN_PORT, OUT_PORT);
	__test_udp_server();

	return 0;
}

void
cos_init(void)
{
	printc("Welcome to the udpserver component\n");
	printc("cos_component_information spdid: %ld\n", cos_comp_info.cos_this_spd_id);

	spdid = cos_comp_info.cos_this_spd_id;

	udpserv_main();
}
