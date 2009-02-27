/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT

//#include <cos_synchronization.h>
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#include <print.h>
#include <errno.h>

#include <sys/socket.h>

extern int cos_wait(int fd);
extern int cos_wait_all(void);
extern int cos_write(int fd, char *buf, int sz);
extern int cos_read(int fd, char *buf, int sz);
extern int cos_close(int fd);
extern int cos_accept(int fd);
extern int cos_bind(int fd, u32_t ip, u16_t port);
extern int cos_listen(int fd, int queue_len);
extern int cos_socket(int domain, int type, int protocol);
extern int sched_block(spdid_t);

#define BUFF_SZ (COS_MAX_ARG_SZ/2)

/* same as in fd_api */
#define MAX_FDS 512
struct fd_struct {
	int fd_pair;
} fds[MAX_FDS];

int main(void)
{
	int accept_fd, fd, amnt;
	char *buf;

	assert(0 <= (accept_fd = cos_socket(PF_INET, SOCK_STREAM, 0)));
	printc("socket created with fd %d", accept_fd);
	assert(0 <= cos_bind(accept_fd, 0, 200));
	printc("bind");
	assert(0 <= cos_listen(accept_fd, 10));
	printc("listen");
	while (1) {
//		printc("waiting");
		fd = cos_wait_all();
		if (fd == accept_fd) {
			while (1) {
				printc("accept");
				if (0 > (fd = cos_accept(accept_fd))) {
					if (fd == -EAGAIN) break;
					assert(0);
				}
			}
		} else {
			buf = cos_argreg_alloc(BUFF_SZ);
			assert(buf);
			while (1) {
//				printc("read");
				amnt = cos_read(fd, buf, BUFF_SZ-1);
				if (0 == amnt) break;//cos_wait(fd);
				else if (-EPIPE == amnt) {
					cos_close(fd);
					break;
				} else if (amnt < 0) {
					assert(0);
				}
//				printc("write");
				cos_write(fd, buf, amnt);
			}
			cos_argreg_free(buf);
		}
	}

	while(1);
}

void cos_init(void *arg)
{
	volatile static int first = 1;

	if (first) {
		first = 0;
		main();
		assert(0);
	} else {
		prints("conn: not expecting more than one bootstrap.");
	}
}

void bin(void)
{
	sched_block(cos_spd_id());
}
