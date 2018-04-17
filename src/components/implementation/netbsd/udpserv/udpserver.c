#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <rk_inv_api.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <rk.h>

#include <memmgr.h>
#include <capmgr.h>
//#include <sched.h>

#include <udpserver.h>
#include <gateway_spec.h>

#define IN_PORT  9998
#define OUT_PORT 9999
#define MSG_SZ   4000
#define TP_INFO_MS (unsigned long long)(10*1000) //5secs
#define HPET_REQ_US (100*1000) //100ms
#define HPET_REQ_BUDGET_US (500) //0.5ms

int spdid;
extern int vmid;
extern struct cos_component_information cos_comp_info;

struct cos_aep_info taeps;

struct sockaddr_in soutput, sinput;

int recv_jpeg = 0;
int send_jpeg = 0;
int send_script = 0;
int send_shutdown = 0;

int shdmem_id;
vaddr_t shdmem_addr;
vaddr_t camera_shdmem_addr;

static char __msg[MSG_SZ + 1] = { '\0' };
unsigned char script[MAX_SCRIPT_SZ];

asndcap_t robot_cont_asnd;
asndcap_t image_ready_asnd;

int
udpserv_request_image(int compid)
{
	printc("udpserv: requesting image\n");

	if (recv_jpeg == 1) {
		printc("image being recvd\n");
		return 0;	
	}
	send_jpeg = 1;	
	return 0;
}

/* Will be mdae asynchronous: notifies udp server of a script update */
int
udpserv_script(int shdmemid, int test)
{
	if (test == REQ_JPEG) {
		udpserv_request_image(shdmemid);
		return 0;
	}

	if (test == SEND_SCRIPT) {
		printc("SENDING SCRIPT\n");
		send_script = 1;
	}
	
	if (test == SEND_SHUTDOWN) {
		printc("SENDING SHUTDOWN\n");
		send_shutdown = 1;
	}

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

static int
update_script()
{
	int i = 0;
	int j = 0;
	
	int sz = MAX_SCRIPT_SZ;

	/* First char indicates this message is a script */
	for (i = 1; i < sz ; i++) {
		//printc("%u , \n", script[j]);
		((unsigned char*)__msg)[i] = script[j];
	
		if (script[j] == SCRIPT_END) {
			/* Reset num and script */
			printc("reached end of script: %d \n", j);
			break;
		}

		j++;
	}
	
	return 0;
}

static void
check_task_done(int x, int y) 
{
	printc("Roomba claims task is done (%d, %d) \n", x, y);
	printc("RESETTING SCRIPT\n");
	reset_script();
	cos_asnd(robot_cont_asnd, ROBOT_CONT_AEP_KEY);
}

static void
store_jpeg(void)
{
	//static char * addr = (char *)shdmem_addr;
	static int stored = 0;
	static int count = 0;

	memcpy((char *)camera_shdmem_addr + stored, __msg, MSG_SZ);
	stored += MSG_SZ;

	count++;	
	if (stored > JPG_SZ) {
		printc("stored in: %d \n", count);
		recv_jpeg = 0;

		cos_asnd(image_ready_asnd, IMAGE_AEP_KEY);
		return;
	}

}

static int
udp_server_start(void)
{
	int fd, fdr;
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

		if (recv_jpeg == 1) {
			send_jpeg = 0;
			store_jpeg();
		}
		
		if (((unsigned int *)__msg)[0] == TASK_DONE) {

			memset(__msg, 0, MSG_SZ);
			/* Request Image */
		//	((unsigned int *)__msg)[0] = REQ_JPEG;
			check_task_done(0, 1);		

		} else if(((unsigned int *)__msg)[0] == RECV_JPEG) {
			
			printc("Recving jpeg now: \n");	
			recv_jpeg = 1;
		}

		if (send_jpeg) {
			((unsigned int *)__msg)[0] = REQ_JPEG;
		}
		
		if (send_script) {
			printc("UDP: Sending script\n");
			((unsigned int *)__msg)[0] = SEND_SCRIPT;
			update_script();
			send_script = 0;
		}
		
		if (send_shutdown) {
			printc("UDP: Sending shutdown\n");
			((unsigned int *)__msg)[0] = SEND_SHUTDOWN;
			update_script();
			send_shutdown = 0;
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
request_image(arcvcap_t rcv, void * data)
{
	printc("request_image\n");

	return;
}

void
cos_init(void)
{
	printc("Welcome to the udpserver component\n");
	printc("cos_component_information spdid: %ld\n", cos_comp_info.cos_this_spd_id);

	spdid = cos_comp_info.cos_this_spd_id;

	/* Test RK entry */
	get_boot_done();
	test_entry(0, 1, 2, 3);

	/* Robot Cont Shared memory */
	shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
	assert(shdmem_id == ROBOTCONT_UDP_SHMEM_ID && shdmem_addr > 0);
	char * test = "testingudp";
	memcpy((char *)shdmem_addr, test, 10);
	
	/* Map Camera shared memory */
	memmgr_shared_page_map(CAMERA_UDP_SHMEM_ID, &camera_shdmem_addr);
	assert(camera_shdmem_addr);
	printc("Camera shmem test: %s \n", (char *)camera_shdmem_addr);

	/* Setup AEP to robot_cont */
	printc("Creating asnd in udp\n");
	robot_cont_asnd = capmgr_asnd_key_create(ROBOT_CONT_AEP_KEY);
	assert(robot_cont_asnd);
	cos_asnd(robot_cont_asnd, ROBOT_CONT_AEP_KEY);

	/* Setup AEP for image available notification */
	printc("Creating asnd in udp\n");
	image_ready_asnd = capmgr_asnd_key_create(IMAGE_AEP_KEY);
	assert(image_ready_asnd);
	cos_asnd(image_ready_asnd, IMAGE_AEP_KEY);
	
	printc("\n************ Starting udp server ***********\n");
	udpserv_main();
}




