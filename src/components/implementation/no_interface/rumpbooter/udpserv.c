#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "micro_booter.h"
#include "rk_inv_api.h"
#include "timer_inv_api.h"

#define IN_PORT  9998
#define OUT_PORT 9999
#define MSG_SZ   16
#define TP_INFO_MS (unsigned long long)(5*1000) //5secs
#define HPET_REQ_US (20*1000) //20ms
#define HPET_REQ_BUDGET_US (500) //1ms

extern int vmid;

static unsigned long long __tp_out_prev = 0, __now = 0, __hpet_req_prev = 0;
static unsigned long __msg_count = 0;
static char __msg[MSG_SZ + 1] = { '\0' };
static u32_t __hpets_last_pass = 0;

static volatile u32_t *__hpets_shm_addr = (u32_t *)APP_SUB_SHM_BASE;
static u32_t __interval_count = (TP_INFO_MS * 1000)/(HPET_PERIOD_US);

static void
__get_hpet_counter(void)
{
#if defined(APP_COMM_ASYNC)
	int ret = 0;
	tcap_res_t b = HPET_REQ_BUDGET_US * cycs_per_usec;

	ret = cos_tcap_delegate(APP_CAPTBL_SELF_IOSND_BASE, BOOT_CAPTBL_SELF_INITTCAP_BASE, b, PRIO_HIGH, 0);
	if (ret != -EPERM) assert(ret == 0);
#elif defined(APP_COMM_SYNC)
	*__hpets_shm_addr = (u32_t)timer_get_counter();
#else
	assert(0);
#endif
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

	rdtscll(__now);
	__tp_out_prev = __now;

	do {
		struct sockaddr sa;
		socklen_t len;

		if (recvfrom(fdr, __msg, msg_size, 0, &sa, &len) != msg_size) {
			PRINTC("read");
			continue;
		}

		/* Reply to the sender */
		soutput.sin_addr.s_addr = ((struct sockaddr_in*)&sa)->sin_addr.s_addr;
		if (sendto(fd, __msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput)) < 0) {
			PRINTC("sendto");
			continue;
		}

		__msg_count ++;
		rdtscll(__now);

		/* Request Number of HPET intervals passed. Every HPET_REQ_US usecs */
		if ((__now - __hpet_req_prev) >= ((cycles_t)cycs_per_usec*HPET_REQ_US)) {
			__hpet_req_prev = __now;
			__get_hpet_counter();
		}

		/* Log every __interval_count number of HPETS processed */
		if (*__hpets_shm_addr >= __hpets_last_pass + __interval_count) {
			PRINTC("%d:Msgs processed:%lu, last seqno:%u\n", tp_counter++, __msg_count, ((unsigned int *)__msg)[0]);
			__msg_count = 0;
			__tp_out_prev = __now;
			__hpets_last_pass = *__hpets_shm_addr;
		}

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
