/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

#include <cos_component.h>
#include <string.h>
#include <cos_debug.h>
#include <errno.h>

extern int cos_wait(int fd);
extern int cos_wait_all(void);
extern int cos_write(int fd, char *buf, int sz);
extern int cos_read(int fd, char *buf, int sz);
extern int cos_close(int fd);
extern int cos_split(int fd);
extern int cos_app_open(int type, struct cos_array *data);

extern int sched_block(spdid_t spd_id);

static int main_fd, data_fd;
const char *service_names[] = {
	"/cgi/hw",
	"/cgi/hw2",
	NULL
};
const char *msg = "hello world";
#define BUFF_SZ 1024

void cos_init(void *arg)
{
	struct cos_array *data;
	int i;
	data = cos_argreg_alloc(BUFF_SZ + sizeof(struct cos_array));

	for (i = 0 ; 1 ; i++) {
		if (NULL == service_names[i]) assert(0);

		memcpy(data->mem, service_names[i], strlen(service_names[i]));
		data->sz = strlen(service_names[i]);
		if (0 <= (main_fd = cos_app_open(0, data))) break;
	}
	
	assert(data);
	while (1) {
		cos_wait(main_fd);
		cos_mpd_update();
		while (1) {
			int amnt;

			data_fd = cos_split(main_fd);
			if (-EAGAIN == data_fd) break;
			else if (0 > data_fd) assert(0);

			do {
				amnt = cos_read(data_fd, data->mem, BUFF_SZ);
				if (0 > amnt) assert(0);
				if (0 == amnt) break;
			} while (0 != amnt);

			memcpy(data->mem, msg, strlen(msg));
			cos_write(data_fd, data->mem, strlen(msg));
			cos_close(data_fd);
		}
	}
}

void symb_bin(void)
{
	sched_block(cos_spd_id());
}
