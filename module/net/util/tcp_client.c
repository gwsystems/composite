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
	int fdr;
	struct sockaddr_in sinput;
	int msg_size;
	char *msg;

	if (argc != 4) {
		printf("Usage: %s <dest_ip> <dest_port> <msg size>\n", argv[0]);
		return -1;
	}

	msg_size = atoi(argv[3]);
	msg = calloc(1, msg_size);
	

	if ((fdr = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Establishing receive socket");
		return -1;
	}

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(atoi(argv[2]));
	sinput.sin_addr.s_addr = inet_addr(argv[1]);
	printf("connecting socket to port %d\n", atoi(argv[2]));
	if (connect(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		perror("connect error");
		return -1;
	}
	while (1) {
		if (msg_size != write(fdr, msg, msg_size)) {
			perror("write error");
			return -1;
		}
	}

	return 0;
}
