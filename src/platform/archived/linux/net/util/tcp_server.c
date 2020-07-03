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
	int fdr, fda, msg_size, ret;;
	struct sockaddr_in sinput;
	char *msg;

	struct sockaddr sa;
	socklen_t len;

	if (argc != 3) {
		printf("Usage: %s <in_port> <msg size>\n", argv[0]);
		return -1;
	}

	msg_size = atoi(argv[2]);
	msg = malloc(msg_size);

	if ((fdr = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
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
	if (listen(fdr, 5)) {
		perror("listen error");
		return -1;
	}
	printf("accepting...\n");
	fda = accept(fdr, &sa, &len);
	if (-1 == fda) {
		perror("accept error");
		return -1;
	}

	while (1) {
		if (-1 == (ret = read(fda, msg, msg_size))) {
			perror("read error");
			return -1;
		}
		if (-1 == write(fda, msg, ret)) {
			perror("write error");
			return -1;
		}
	}

	return 0;
}
