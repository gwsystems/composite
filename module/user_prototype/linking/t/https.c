#include <string.h>
#include <stdlib.h>

#include <stdio.h>

inline static int is_whitespace(char s)
{
	if (' ' == s) return 1;
	return 0;
}

inline static char *remove_whitespace(char *s, int len)
{
	while (is_whitespace(*(s++)) && len--);
	return s-1;
}

inline static char *find_whitespace(char *s, int len)
{
	while (!is_whitespace(*(s++)) && len--);
	return s-1;
}

inline static char *get_http_version(char *s, int len, int *minor_version)
{
	char *c;
	const char http_str[] = "HTTP/1.";
	int http_sz = sizeof(http_str)-1; // -1 for \n

	c = remove_whitespace(s, len);
	if (memcmp(http_str, c, http_sz)) {
		*minor_version = -1;
		return NULL;
	}
	c += http_sz;
	*minor_version = ((int)*c) - (int)'0';
	return c+1;
}

/* INVARIANT: len >= 2 */
static inline char *http_end_line(char *s, int len)
{
	if ('\r' != s[0] && '\n' != s[1]) return s+2;
	return NULL;
}

static char *http_find_next_line(char *s, int len)
{
	char *r, *e = s;

	for (e = s; 
	     len >= 2 && NULL == (r = http_end_line(e, len)); 
	     e++, len--);

	if (len < 2) return NULL;
	return r;
}

static char *http_find_header_end(char *s, int len)
{
	char *c = s, *p;

	do {
		p = c;
		c = http_find_next_line(c, len);
		/* Malformed: last characters not cr lf */
		if (NULL == c) return NULL;
	} while(c != p + 2);
}

typedef struct {
	char *end;
	char *path;
} get_ret_t;

static int http_get_parse(char *req, int len, get_ret_t *ret)
{
	char *s = &req[3], *e;
	char *path;
	int path_len, minor_version;
	len -= 3;

	path = remove_whitespace(s, len);
	e = find_whitespace(path, len - (path-s));
	path_len = e-path;
	e = remove_whitespace(e, len - (e-s));
	e = get_http_version(e, len - (e-s), &minor_version);
	if (NULL == e) goto malformed_request;
	e = http_find_header_end(e, len - (e-s));
	if (NULL == e) goto malformed_request;

	ret->end = e;
	ret->path = path;
	return 0;
malformed_request:
	return -1;
}

char *http_get_response(char *path, int len)
{
	char *response;
	static char *success = 
		"HTTP/1.0 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: 12\r\n"
		"\r\nhello world!";

	return success;
}

int http_get(char *req, int len)
{
	get_ret_t r;
	char *s = req;
	char *response;

	if (http_get_parse(s, len - (s - req), &r)) {
		return -1;
	}
	response = http_get_response(r.path, r.end-s);
	s = r.end;
	
	return 0;
}

int http_new_request(char *req, int len, char **resp) // ipaddr?
{
	
	if (!memcmp("GET", req, 3)) {
		http_get(req, len);
	}
}

int http_new_connection(int fd)
{
	
}



#define LINUX_ENV
#ifdef LINUX_ENV

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <unistd.h>

int connmgr_create_server(short int port)
{
	int fd;
	struct sockaddr_in server;

	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}

	server.sin_family      = AF_INET;
	server.sin_port        = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&server, sizeof(server))) {
		perror("binding receive socket");
		return -1;
	}
	listen(fd, 10);

	return fd;
}

int connmgr_accept(int fd)
{
	struct sockaddr_in sai;
	int new_fd, len = sizeof(sai);

	new_fd = accept(fd, (struct sockaddr *)&sai, &len);
	if (-1 == new_fd) {
		perror("accept");
		return -1;
	}
	return new_fd;
}

int main(void)
{
	int sfd, nfd;
	char buff[1024];
	int amnt;
	char *resp;

	sfd = connmgr_create_server(8000);
	nfd = connmgr_accept(sfd);
	amnt = read(nfd, buff, 1024);
	buff[amnt] = '\0';
	http_new_request(buff, amnt, &resp);
	

	printf("\nEntire message:\n%s\n", buff);
	
 	

	return 0;
}

#endif
