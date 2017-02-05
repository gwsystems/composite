/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#define COS_VECT_INIT_VAL ((void*)-1)
#include <cos_vect.h>
#include <print.h>
#include <errno.h>

#include <sys/socket.h>

#include <fd.h>
#include <sched.h>

#define BUFF_SZ 1401 //(COS_MAX_ARG_SZ/2)

COS_VECT_CREATE_STATIC(fds);

static inline int get_fd_pair(int fd)
{
	int pair;

	pair = (int)cos_vect_lookup(&fds, (long)fd);
	assert(pair > 0);
	return pair;
}

static inline void set_fd_pair(int fd, int pair)
{
	if (cos_vect_add_id(&fds, (void*)pair, fd) < 0) BUG();
}

int accept_fd;

static void accept_new(int accept_fd)
{
	int fd, http_fd;

	while (1) {
		if (0 > (fd = cos_accept(accept_fd))) {
			if (fd == -EAGAIN) break;
			BUG();
		}
		if (0 > (http_fd = cos_app_open(0, NULL))) {
			printc("app_open returned %d", http_fd);
			BUG();
		}
		set_fd_pair(fd, http_fd);
		set_fd_pair(http_fd, fd);
	}
}

static void data_new(int fd)
{
	int amnt, fd_pair;
	char *buf;

	fd_pair = get_fd_pair(fd);
	if (fd_pair < 0) return;
	buf = cos_argreg_alloc(BUFF_SZ);
	assert(buf);
	while (1) {
		int ret;

		amnt = cos_read(fd, buf, BUFF_SZ-1);
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			cos_close(fd_pair);
			cos_close(fd);
			set_fd_pair(fd, -1);
			set_fd_pair(fd_pair, -1);
			break;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", fd, amnt);
			BUG();
		}
		if (amnt != (ret = cos_write(fd_pair, buf, amnt))) {
			cos_close(fd_pair);
			cos_close(fd);
			printc("conn_mgr: write failed w/ %d on fd %d\n", ret, fd_pair);
		}
	}
	cos_argreg_free(buf);
}

int main(void)
{
	int fd;

	cos_vect_init_static(&fds);

	if (0 > (accept_fd = cos_socket(PF_INET, SOCK_STREAM, 0))) BUG();
	if (0 > cos_bind(accept_fd, 0, 200)) BUG();
	if (0 > cos_listen(accept_fd, 255)) BUG();
	while (1) {
		fd = cos_wait_all();
		cos_mpd_update();
		assert(fd >= 0);
		if (fd == accept_fd) {
			accept_new(accept_fd);
		} else {
			data_new(fd);
		}
	}
}

void cos_init(void *arg)
{
	static volatile int first = 1;

	if (first) {
		first = 0;
		main();
		BUG();
	} else {
		prints("conn: not expecting more than one bootstrap.");
	}
}
