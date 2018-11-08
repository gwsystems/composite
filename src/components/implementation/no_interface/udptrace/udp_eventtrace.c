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
#include <event_trace.h>
#include <sched.h>
#include <channel.h>
#include <capmgr.h>
#include <cos_time.h>
#include <workload.h>
#include <hypercall.h>

static struct sockaddr_in soutput, sinput;
static int fd, fdr;
static cycles_t test_last_reboot = 0;
#undef TEST_REBOOT_ENABLE
#define TEST_REBOOT_US (20*1000*1000) //20secs

#define CLIBUF_SZ 16
#define TRACE_FLUSH_SLEEP_US 500

#undef UDP_TRACE_ENABLE

static char *cfe_apps[] = {
	"kit_to", /* reboots fine. needs the cosmos side to do "Enable Telemetry" after reboot and everything just works! */
//	"kit_ci", /* TODO: socket connection not working on reboot. perhaps more bookkeeping? */
//	"kit_sch", /* multi-threaded.. reboot limitation! */
//	"ds", /* TODO: Test */
//	"fm", /* multi-threaded.. reboot limitation! */
//	"lc", /* TODO: test this! */
//	"sc", /* TODO: debug it. looks like initialization fails.! */
//	"bm", /* reboots just fine */
//	"mm", /* reboots fine */
//	"hc", /* TODO: test */
//	"f42", /* should not fail, so not testing!, high-critical task. theoretically, can't just reboot this app! */
//	"i42", /* should not fail, not tested!, high-critical task. theoretically, can't just reboot this app! */
};

int
udp_request_reboot(char *app)
{
	static int first_req = 1;
	static vaddr_t reqaddr = NULL;
	static cbuf_t shmid = 0;
	static unsigned long npages = 0;
	spdid_t s;
	static unsigned int *tail = NULL;
	unsigned int cur_val = 0;
	static int *ret = NULL;
	static asndcap_t snd = 0;

	if (unlikely(shmid == 0)) {
		shmid = channel_shared_page_map(CFE_REBOOT_REQ_KEY, &reqaddr, &npages);
		if (shmid == 0) return -1;

		assert(reqaddr && npages == 1);
		tail = (unsigned int *)(reqaddr + 4);
		ret = (int *)(reqaddr + 8);
		snd = capmgr_asnd_key_create(CFE_REBOOT_REQ_KEY);
		assert(snd);
	}

	/* reboot func is busy! lets try again later */
	if (unlikely(first_req == 0 && *ret == 0)) return 0;
	/* single producer!! */
	cur_val = (unsigned int)ps_faa((unsigned long *)tail, 1);
	s = hypercall_comp_id_get(app);
	PRINTC("Requested to reboot: %s, %u\n", app, s);
	assert(s > 0);
	*(spdid_t *)(reqaddr + 12 + ((cur_val)*sizeof(spdid_t))) = s;
	cos_asnd(snd, 1);

	if (unlikely(first_req == 1)) first_req = 0;

	return 0;
}

int
udp_writeout(unsigned char *buf, unsigned int sz)
{
	static int first = 1;
	static unsigned int reboot_count = 0;
	cycles_t now;

#ifdef UDP_TRACE_ENABLE
	struct sockaddr sa;
	socklen_t len = sizeof(struct sockaddr);

	if (sendto(fd, buf, sz, 0, (struct sockaddr*)&soutput, sizeof(soutput)) != sz) {
		printc("sendto");
		assert(0);
	}
#else
	static unsigned long num_read = 0;

	/* first writeout has header info. */
	if (unlikely(first)) {
		first = 0;
	} else {
		assert(sz && sz % sizeof(struct event_trace_info) == 0);
		num_read += (sz / sizeof(struct event_trace_info));
	}
#endif

#ifdef TEST_REBOOT_ENABLE
	rdtscll(now);
	if ((now - test_last_reboot) >= time_usec2cyc(TEST_REBOOT_US)) {
		int sz = sizeof(cfe_apps)/sizeof(cfe_apps[0]);

		test_last_reboot = now;
		udp_request_reboot(cfe_apps[reboot_count % sz]);
		reboot_count++;
	}
#endif

	return sz;
}

static int
udp_event_trace_init(void)
{
#ifdef UDP_TRACE_ENABLE
	struct sockaddr client_sa;
	socklen_t client_salen = sizeof(struct sockaddr);
	char buf[CLIBUF_SZ] = { 0 };

	soutput.sin_family      = AF_INET;
	soutput.sin_port        = htons(EVENT_TRACE_OUTPORT);
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
	sinput.sin_port        = htons(EVENT_TRACE_INPORT);
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		printc("binding receive socket\n");
		return -1;
	}

	/* wait for a client to connect */
	if (recvfrom(fdr, buf, CLIBUF_SZ, 0, &client_sa, &client_salen) != CLIBUF_SZ) {
		printc("recvfrom");
		return -1;
	}
	soutput.sin_addr.s_addr = ((struct sockaddr_in*)&client_sa)->sin_addr.s_addr;

	printc("Recvfrom success: [%s]\nStarting to flush out events!\n", buf);
#endif

#ifdef EVENT_TRACE_REMOTE
	event_trace_client_init(udp_writeout);
#else
	assert(0);
#endif

	return 0;
}

static int
udp_event_trace_loop(void)
{
	/*
	 * NOTE: if no client connects, we don't go past recvfrom and
	 *       so ring buffer will start to drop events
	 */
	rdtscll(test_last_reboot);

	while (1) {
		cycles_t abs_sleep = 0;

		event_flush();
		rdtscll(abs_sleep);

		/* perhaps sleep for a bit? */
		sched_thd_block_timeout(0, abs_sleep + time_usec2cyc(TRACE_FLUSH_SLEEP_US));
	}

	return -1;
}

void
cos_init(void)
{
	printc("Welcome to the udp event trace component\n");

	rk_libcmod_init();

	udp_event_trace_init();
	schedinit_child();

	udp_event_trace_loop();
}
