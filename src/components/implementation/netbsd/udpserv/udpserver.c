#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <rk_inv_api.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <rk.h>
#include <memmgr.h>
#include <capmgr.h>
#include <sched.h>

#include <udpserver.h>
#include <gateway_spec.h>

#define IN_PORT  9998
#define OUT_PORT 9999
#define MSG_SZ   32
#define TP_INFO_MS (unsigned long long)(10*1000) //5secs
#define HPET_REQ_US (100*1000) //100ms
#define HPET_REQ_BUDGET_US (500) //0.5ms

int spdid;
extern int vmid;
extern struct cos_component_information cos_comp_info;

int shdmem_id;
vaddr_t shdmem_addr;

static char __msg[MSG_SZ + 1] = { '\0' };
unsigned char script[MAX_SCRIPT_SZ];

asndcap_t robot_cont_asnd;

/* Will be mdae asynchronous: notifies udp server of a script update */
int
udpserv_script(int shdmemid)
{
	printc("udpserv_script\n");

	int i = 0;
	unsigned char * shm_ptr = (unsigned char *)shdmem_addr;

	for (i = 0; i < MAX_SCRIPT_SZ; i ++) {
		script[i] = (unsigned char)shm_ptr[i];
	}

	return 0;
}

static void
reset_script(void)
{
	int i = 0;
	for (i = 0; i < MAX_SCRIPT_SZ; i++) {
		script[i] = 0;
	}
}

/* Used for internal testing, will be replaced by above fn */
static int
update_script()
{
	int i = 0;
	int j = 0;
	
	static int num = 0;
	int sz = MSG_SZ;

	script[sz] = '\0';
	j = MSG_SZ*num;
        
	/* First char indicates index into script this message is */
	((unsigned char*)__msg)[i] = num;
	for (i = 1; i < sz ; i++) {
	
		((unsigned char*)__msg)[i] = script[j];
		if (script[j] == SCRIPT_END) {
			/* Reset num and script */
			num = -1;
			break;
		}
		j++;
	}
	
	if (num == -1) num = 0;
	else num++;

	return 0;
}

static void
check_task_done(int x, int y) {
	printc("Roomba claims task is done (%d, %d) \n", x, y);
	printc("\nRESETTING SCRIPT\n");
	reset_script();
	cos_asnd(robot_cont_asnd, ROBOT_CONT_AEP_KEY);
}

static int
udp_server_start(void)
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
		if (recvfrom(fdr, __msg, msg_size, 0, &sa, &len) != msg_size) {
			printc("read");
			continue;
		}

		if (((unsigned int *)__msg)[0] == TASK_DONE) {
			memset(__msg, 0, MSG_SZ);
			check_task_done(0, 1);		
		} else {
			memset(__msg, 0, MSG_SZ);
			update_script();
		}
	
		/* Reply to the sender */
		soutput.sin_addr.s_addr = ((struct sockaddr_in*)&sa)->sin_addr.s_addr;

		if (sendto(fd, __msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput)) < 0) {
			printc("sendto");
			continue;
		}


	} while (1) ;

	return -1;
}

int
udpserv_main(void)
{
	rk_socketcall_init();

	printc("Starting udp-server [in:%d out:%d]\n", IN_PORT, OUT_PORT);
	udp_server_start();

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

	shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
	assert(shdmem_id  -1 && shdmem_addr > 0);
	printc("shdmemid: %d \n", shdmem_id);

	char * test = "testingudp";
	memcpy((char *)shdmem_addr, test, 10);

	/* Setup AEP to robot_cont */
	printc("creating asnd in udp\n");
	robot_cont_asnd = capmgr_asnd_key_create(ROBOT_CONT_AEP_KEY);
	assert(robot_cont_asnd);
	cos_asnd(robot_cont_asnd, ROBOT_CONT_AEP_KEY);

	udpserv_main();
}




