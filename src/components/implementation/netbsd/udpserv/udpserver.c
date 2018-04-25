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

struct sockaddr_in soutput, sinput;

enum serving {
	pending,
	rcving_jpeg,
	req_jpeg,
	snding_script,
	snding_shutdown
};
enum serving status = pending;


int shdmem_id;
vaddr_t shdmem_addr;
vaddr_t camera_shdmem_addr;
int jpeg_sz = 0;

static char __msg[MSG_SZ + 1] = { '\0' };
unsigned char script[MAX_SCRIPT_SZ];

asndcap_t robot_cont_asnd;
asndcap_t image_ready_asnd;

int
udpserv_request(int shdmemid, int request)
{
	int i = 0;

	/* mutex, where art thou, or we should queue tasks intelligently */
	if (status != pending) return -1;

	switch (request) {
		case REQ_JPEG:
			printc("REQUESTING JPEG\n");
			if (status == rcving_jpeg) return 0;	
			status = req_jpeg;
			return 0;
		case SEND_SCRIPT:
			printc("SENDING SCRIPT: %d \n", shdmemid);
			status = snding_script;
			break;
		case SEND_SHUTDOWN:
			printc("SENDING SHUTDOWN\n");
			status = snding_shutdown;
			break;
		default:
			break;
	}

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
		((unsigned char*)__msg)[i] = script[j];
		//printc("%u , ", ((unsigned char*)__msg)[i]);
	
		if (script[j] == SCRIPT_END) break;

		j++;
	}
	
	return 0;
}

static void
check_task_done(int x, int y) 
{
	reset_script();
	cos_asnd(robot_cont_asnd, AUTODRIVE_COMP);
}

static void
store_jpeg(void)
{
	static int stored = 0;

	memcpy((char *)camera_shdmem_addr + stored, __msg, MSG_SZ);
	stored += MSG_SZ;

	if (stored > jpeg_sz) {
		printc("stored in: %d \n", stored);
		status = pending;

		cos_asnd(image_ready_asnd, IMAGE_AEP_KEY);
		stored = 0;
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

		if (status == rcving_jpeg) {
			store_jpeg();
		}
		
		/* Recieve routine */	
		switch(((unsigned int *)__msg)[0]) {
		case TASK_DONE:
			printc("Recvd task done\n");
			memset(__msg, 0, MSG_SZ);
			check_task_done(0, 1);		
			break;
		case RECV_JPEG:
			printc("Recving jpeg now: sz: %lu \n", ((unsigned long *)__msg)[1]);
			jpeg_sz = ((unsigned long *)__msg)[1];
			status = rcving_jpeg;
			break;
		case SCRIPT_RECV_ACK:
			printc("Script Recvd by client \n");	
			status = pending;
			break;
		default:
			break;
		}

		/* Serving Routine */	
		switch(status) {
		case req_jpeg:
			((unsigned int *)__msg)[0] = REQ_JPEG;
			break;
		
		case snding_script:
			update_script();
			((unsigned char *)__msg)[0] = SEND_SCRIPT;
			break;
		
		case snding_shutdown:
			printc("UDP: Sending shutdown: %d \n", status);
			((unsigned int *)__msg)[0] = SEND_SHUTDOWN;
			//update_script();
			status = pending;
			break;
		
		default:
			break;
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




