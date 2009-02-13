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

#include <sys/socket.h>

extern int cos_wait(int fd);
extern int cos_wait_all(void);
extern int cos_write(int fd, char *buf, int sz);
extern int cos_read(int fd, char *buf, int sz);
extern int cos_close(int fd);
extern int cos_accept(int fd);
extern int cos_bind(int fd, u32_t ip, u16_t port);
extern int cos_listen(int fd);
extern int cos_socket(int domain, int type, int protocol);
extern int sched_block(spdid_t);

int main(void)
{
	int fd1, fd2, amnt;
	char *buf;

	assert(0 <= (fd1 = cos_socket(PF_INET, SOCK_STREAM, 0)));
	printc("socket created with fd %d", fd1);
	assert(0 <= cos_bind(fd1, 0, 200));
	printc("bind");
	assert(0 <= cos_listen(fd1));
	printc("listen");
	assert(0 <= (fd2 = cos_accept(fd1)));
	printc("accept returned with fd %d", fd2);
	buf = cos_argreg_alloc(128);
	assert(buf);
	do {
		amnt = cos_read(fd2, buf, 127);
		if (0 == amnt) cos_wait(fd2);
		else cos_write(fd2, buf, amnt);
	} while (1);
	cos_argreg_free(buf);
	assert(0 == cos_close(fd2));
	printc("closed fd %d", fd2);

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
