#include <sys/types.h>
#include <sys/socket.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <unistd.h>

int connmgr_connect(short int port)
{
	int fd;
	struct sockaddr_in server;

	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}

	server.sin_family      = AF_INET;
	server.sin_port        = htons(port);
	if (!inet_aton("127.0.0.1", &(server.sin_addr))) {
		printf("Could not convert net address\n");
		exit(-1);
	}
//	server.sin_addr.s_addr = gethostbyname("127.0.0.1");//htonl(INADDR_ANY);
	if (connect(fd, (struct sockaddr *)&server, sizeof(server))) {
		perror("connecting socket");
		exit(-1);
	}

	return fd;
}

static const char request[] = 
	"GET / HTTP/1.1\r\n"
	"User-Agent: httperf/0.8\r\n"
	"Host: localhost\r\n\r\n";

int main(void)
{
	int fd;
	char buff[1025];
	int amnt;

	fd = connmgr_connect(8000);

	amnt = write(fd, request, sizeof(request)-1);
	if (amnt != sizeof(request)-1) {
		printf("WTF\n");
		return -1;
	}
	amnt = read(fd, buff, 1024);
	while (amnt > 0) {
		buff[amnt] = '\0';
		printf("\nEntire message (len %d):\n%s<end>\n", amnt, buff);
		amnt = read(fd, buff, 1024);
	}
	return 0;
}
