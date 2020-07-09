#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int fd, fdr;
	struct sockaddr_in soutput, sinput;
	int msg_size;
	char *msg;

	if (argc != 4) {
		printf("Usage: %s <in_port> <dst_port> <msg size>\n", argv[0]);
		return -1;
	}

	msg_size = atoi(argv[3]);
	msg = malloc(msg_size);

	soutput.sin_family      = AF_INET;
	soutput.sin_port        = htons(atoi(argv[2]));
//	soutput.sin_addr.s_addr = inet_addr("10.0.1.6");//htonl(INADDR_ANY);//inet_addr(argv[1]);
//	printf("%x\n", (unsigned int)soutput.sin_addr.s_addr);
	soutput.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(argv[1]);
	printf("Sending to port %d\n", atoi(argv[2]));
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}
	if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing receive socket");
		return -1;
	}

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(atoi(argv[1]));
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);
	printf("binding receive socket to port %d\n", atoi(argv[1]));
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		perror("binding receive socket");
		return -1;
	}

	while (1) {
		struct sockaddr sa;
		socklen_t len;

		if (recvfrom(fdr, msg, msg_size, 0, &sa, &len) != msg_size && errno != EINTR) {
			perror("read");
			return -1;
		} 
		/* Reply to the sender */
		soutput.sin_addr.s_addr = ((struct sockaddr_in*)&sa)->sin_addr.s_addr;
//		printf("%x\n", (unsigned int)soutput.sin_addr.s_addr);
		if (sendto(fd, msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput)) < 0 &&
			   errno != EINTR) {
			perror("sendto");
			return -1;
		}
		//printf("in, out and on its way!\n");
	}

	return 0;
}
