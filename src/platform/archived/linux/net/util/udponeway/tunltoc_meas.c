#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <math.h>

#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define LINUX_TEST
#include "../../../translator/translator_ioctl.h"
#include "../../../../../kernel/include/shared/cos_types.h"
#include "../../../../../components/include/cringbuf.h"
#include "../../../../../kernel/include/shared/cos_config.h"

#define PROC_FILE "/proc/translator"
#define MAP_SIZE  COS_PRINT_MEM_SZ //(4096 * 256)
#define PRINT_CHUNK_SZ (4096*16)

struct cringbuf sharedbuf;

#define rdtscll(val) \
        __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITR 1024 // # of iterations we want
volatile unsigned long long stsc; // timer thread timestamp 
volatile int done = 0; 
unsigned long long timer_arr[ITR];

#define IPADDR "128.164.81.57"
//#define IPADDR "10.0.2.8"
#define IFNAME "tun0"
#define P2PPEER "10.0.2.8"

void *
timer_thd(void *data)
{
    while(1) {
        rdtscll(stsc);
        if(done) {
            break;
        }
    }
    pthread_exit(NULL);
}

void *
recv_pkt(void *data)
{
    char **arguments = (char**) data;

    int msg_size = atoi(arguments[2]);
    int fdr, i = ITR, j = 0, ret;
    unsigned long long tsc;
    char *msg;
    char buf[] = "ifconfig "IFNAME" inet "IPADDR" netmask 255.255.255.0 pointopoint "P2PPEER;
    struct ifreq ifr;

	int fd, _read = 0;
	void *a;
	char c, buf1[PRINT_CHUNK_SZ];


    msg = malloc(msg_size);
    printf("Message size is (%d)\n", msg_size);

    // open the tun device
    if((fdr = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("open /dev/net/tun: ");
        exit(-1);
    }
    printf("tun FD (%d) created\n", fdr);
    
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if(ioctl(fdr, TUNSETIFF, (void *) &ifr) < 0) {
        perror("ioctl TUNSETIFF: ");
        close(fdr);
        exit(-1);
    }

    // Uncomment this if a persistant tun interface is needed
    // It will only work properly if you remove the created 
    // interface in between executions of tunrecv
    /*
    if(ioctl(fdr, TUNSETPERSIST, 1) < 0) {
        perror("ioctl TUNSETPERSIST: "); 
        close(fdr);
        exit(-1);
    }
    */
    
    printf("Running: %s\n", buf);
    if(system(buf) < 0) {
        perror("setting tun ipaddress: ");
        exit(-1);
    }
    printf("Done setting TUN. \n");

	fd = open(PROC_FILE, O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(-1);
	}

	trans_ioctl_set_channel(fd, COS_TRANS_SERVICE_PONG);
	trans_ioctl_set_direction(fd, COS_TRANS_DIR_LTOC);
	a = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (MAP_FAILED == a) {
		perror("mmap");
		exit(-1);
	}
	cringbuf_init(&sharedbuf, a, MAP_SIZE);
	/* wait for user to start */
//	_read = read(0, buf1, PRINT_CHUNK_SZ);

	while (i) {
		int off = 0;
		unsigned long long *p;

		ret = read(fdr, msg, msg_size);
		_read = 8 + 1;
		p = buf1;
		*p = stsc;
		*(buf1+8) = *msg;
//		rdtscll(*p);

		do {
			int p;

			p = cringbuf_produce(&sharedbuf, buf1 + off, _read);
			_read -= p;
			off += p;
			if (p) {
				write(fd, &c, 1);
			}
		} while (_read);
		j += 1;
        
		i--;
	}


/*     while(i > 0) { */
/*         ret = read(fdr, msg, msg_size); */
/*         rdtscll(tsc); */
/* //	if (tsc <= stsc) printf("wrong!!!\n"); */
/*         timer_arr[j] = tsc - stsc; */
/*         j += 1; */
        
/*         if (ret != msg_size && errno != EINTR) { */
/*             printf("ret (%d) errno (%d)\n", ret, errno); */
/*             perror("read"); */
/*             exit(-1); */
/*         }    */
/*         i--; */
/*     } */
    done = 1;
    close(fdr);
	if (munmap(a, MAP_SIZE) < 0) {
		perror("munmap");
		exit(-1);
	}

    pthread_exit(NULL);
}

void
get_statistics()
{
    // For mean
    unsigned long long running_sum = 0, i;
    printf("Done !\n");
    return;
    // For stddev
    double running_sdevsum = 0;
    long double sdev_arr[ITR];
    long double mean;
    double sdev;

    // Calculating the mean
    for(i = 1; i < ITR; i++) 
        running_sum += timer_arr[i];
    mean = running_sum / ITR;        

    printf("Done reading %d iterations!\n", ITR);
    printf("The sum is  (%llu)\nThe mean is (%Lf)\n", running_sum, mean);

    // Calculating the standard deviation
    for(i = 0; i < ITR; i++) {
	    sdev_arr[i] = timer_arr[i] > mean ? timer_arr[i] - mean : mean - timer_arr[i];
	    running_sdevsum += (sdev_arr[i] * sdev_arr[i]);
//	    printf("%llu\n", timer_arr[i]);
    }
    running_sdevsum /= (ITR);
    sdev = sqrt(running_sdevsum);

    printf("The standard deviation is (%lf), sum %lf\n", sdev, running_sdevsum);
}

int 
main(int argc, char *argv[])
{
    // thread stuff
    pthread_t tid_timer, tid_recv;
    struct sched_param sp_timer, sp_recv;
    struct rlimit rl;
    cpu_set_t mask;

    void *thd_ret;

    if (argc != 3) {
        printf("Usage: %s <in_port> <msg size>\n", argv[0]);
        return -1;
    }

    // Set up rlimits to allow more CPU usage
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    if(setrlimit(RLIMIT_CPU, &rl)) {
        perror("set rlimit: ");
        return -1;
    }
    printf("CPU limit removed\n");


    // Set up the receiver and create a thread for it
    sp_recv.sched_priority = sched_get_priority_max(SCHED_RR);
    if(pthread_create(&tid_recv, NULL, recv_pkt, (void *) argv) != 0) {
        perror("pthread create receiver: ");
        return -1;
    }
    /*    
    if(pthread_setschedparam(tid_recv, SCHED_RR, &sp_recv) != 0) {
        perror("pthread setsched recv: ");
        return -1;
    }
    printf("receiver priority: (%d)\n", sp_recv.sched_priority);
    */
    // Set up the timer and create a thread for it
    sp_timer.sched_priority = (sched_get_priority_max(SCHED_RR) - 1);
    if(pthread_create(&tid_timer, NULL, timer_thd, NULL) != 0) {
        perror("pthread create timer: ");
        return -1;
    }
    /*
    if(pthread_setschedparam(tid_timer, SCHED_RR, &sp_timer) != 0) {
        perror("pthread setsched timer: ");
        return -1;
    }
    printf("timer priority: (%d)\n", sp_timer.sched_priority);
    */

    // Set up processor affinity for both threads
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if(pthread_setaffinity_np(tid_timer, sizeof(mask), &mask ) == -1 ) {
        perror("setaffinity timer error: ");
        return -1;
    }
    printf("Set affinity for timer thread\n");

    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if(pthread_setaffinity_np(tid_recv, sizeof(mask), &mask ) == -1 ) {
        perror("setaffinity recv error: ");
        return -1;
    }
    printf("Set affinity for recv thread\n");


    // We're done here; we'll wait for the threads to do their thing
    pthread_join(tid_recv, &thd_ret);
    pthread_join(tid_timer, &thd_ret);

    get_statistics();

    return 0;
}
