#ifdef EVENT_LINUX_DECODE
#include <assert.h>
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
#include <event_trace.h>

#define TRACE_BUF_SIZE 2048

int udp_trace_loop(char *ip, unsigned short inport, unsigned short outport)
{
	int fd, fdr;
	struct sockaddr_in soutput, sinput;
	int msg_size;
	char *msg;
	struct sockaddr sa;
	socklen_t len = sizeof(struct sockaddr);
	int first = 1;

	msg_size = 16;
	msg = malloc(msg_size);

	soutput.sin_family      = AF_INET;
	soutput.sin_port        = htons(inport);
	soutput.sin_addr.s_addr = inet_addr(ip);
	if ((fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}
	if ((fdr = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		perror("Establishing receive socket");
		return -1;
	}

	sinput.sin_family      = AF_INET;
	sinput.sin_port        = htons(outport);
	sinput.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fdr, (struct sockaddr *)&sinput, sizeof(sinput))) {
		perror("binding receive socket");
		return -1;
	}

	int ret = sendto(fd, msg, msg_size, 0, (struct sockaddr*)&soutput, sizeof(soutput));
	if (ret <= 0) {
		perror("sendto");
		return -1;
	}

	while (1) {
		char trace_buf[TRACE_BUF_SIZE] = { 0 }, *trace_ptr = NULL;
		unsigned int trace_len = 0;

		if ((trace_len = recv(fdr, trace_buf, TRACE_BUF_SIZE, 0)) <= 0) {
			if (trace_len == 0) continue;

			perror("read");
			return -1;
		}

		trace_ptr = trace_buf;
		if (first) {
			trace_ptr = event_trace_check_hdr(trace_buf, &trace_len);

			first = 0;
			if (!trace_len) continue;
		}
		if (trace_len % (sizeof(struct event_trace_info))) assert(0);

		event_decode(trace_ptr, trace_len);
	}

	return 0;
}
#endif
