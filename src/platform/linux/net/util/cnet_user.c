#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#define DEVCNET "/dev/net/cnet"
#define ETHCNET "cnet0"
/* ip address of host */
//#define IPADDR  "10.0.2.9"
#define IPADDR  "128.164.157.248"
#define P2PPEER "10.0.2.8"

int main(void) {
	struct ifreq ifr;
	int fd;
	char buf[] = "ifconfig "ETHCNET" inet "IPADDR" netmask 255.255.255.0 pointopoint "P2PPEER;

	fd = open(DEVCNET, O_RDWR);
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN; //|IFF_NO_PI; /*IFF_TAP*/
	if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) {
		perror(DEVCNET" ioctl TUNSETIFF");
		exit(1);
	}
	if (ioctl(fd, TUNSETPERSIST, 1) < 0) {
		perror(DEVCNET" ioctl TUNSETPERSIST");
		exit(1);
	}

	printf("executing: \"%s\"\n", buf);
	if (!system(buf)) {
		//pause();
		return 0;
	} /* else error */

	return -1;
}
