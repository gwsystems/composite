#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <rk_inv_api.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <rk.h>
#include <memmgr.h>

#define IN_PORT  9998
#define OUT_PORT 9999
#define MSG_SZ   32
#define TP_INFO_MS (unsigned long long)(10*1000) //5secs
#define HPET_REQ_US (100*1000) //100ms
#define HPET_REQ_BUDGET_US (500) //0.5ms

int spdid;
extern int vmid;
extern struct cos_component_information cos_comp_info;

static char __msg[MSG_SZ + 1] = { '\0' };
unsigned char script[100];

static int
update_script()
{
	printc("update_script\n");
	int i = 0;
	
	int sz = MSG_SZ/4;

	for (i = 0; i < sz; i++) {
		script[i] = i+1;
	}
	script[sz] = '\0';

	for (i = 0; i < sz ; i++) {
		
		if (script[i] == '\0') break;
		((unsigned int *)__msg)[i] = script[i];
		printc("msg[%d]: %u \n", i ,((unsigned int *)__msg)[i] );
	}

	return 0;
}

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
	printc("Sending to port %d\n", OUT_PORT);
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		printc("Error Establishing socket\n");
		return -1;
	}
	printc("fd for socket: %d\n", fd);
	if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		printc("Error Establishing receive socket\n");
		return -1;
	}
	printc("fd for receive socket: %d\n", fdr);

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(IN_PORT);
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);
	printc("binding receive socket to port %d\n", IN_PORT);
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		printc("binding receive socket\n");
		return -1;
	}

	do {
		struct sockaddr sa;
		socklen_t len = sizeof(struct sockaddr);
		printc("going to recieve now:\n");
		if (recvfrom(fdr, __msg, msg_size, 0, &sa, &len) != msg_size) {
			printc("read");
			continue;
		}
		
		printc("A Received-msg: seqno:%u msg: %u time:%llu\n", ((unsigned int *)__msg)[0], ((unsigned int *)__msg)[1], ((unsigned long long *)__msg)[1]);
		/* Reply to the sender */

		soutput.sin_addr.s_addr = ((struct sockaddr_in*)&sa)->sin_addr.s_addr;
		memset(__msg, 0, MSG_SZ);
		
		update_script();
		if (sendto(fd, __msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput)) < 0) {
			printc("sendto");
			continue;
		}

		printc("Sent-msg: seqno:%u time:%llu\n", ((unsigned int *)__msg)[0], ((unsigned long long *)__msg)[1]);

	} while (1) ;

	return -1;
}

int
udpserv_main(void)
{
	rk_socketcall_init();

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

	/* Test RK entry */
	printc("calling rk_inv_entry\n");
	get_boot_done();
	test_entry(0, 1, 2, 3);

	udpserv_main();
}
