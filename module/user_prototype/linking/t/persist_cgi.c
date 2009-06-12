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
const char *service_name = "/cgi/hw";
const char *msg = "hello world";
#define BUFF_SZ 1024

void cos_init(void *arg)
{
	struct cos_array *data;
	data = cos_argreg_alloc(BUFF_SZ + sizeof(struct cos_array));
	memcpy(data->mem, service_name, strlen(service_name));
	data->sz = strlen(service_name);
	if (0 > (main_fd = cos_app_open(0, data))) assert(0);
	
	assert(data);
	while (1) {
		int amnt;

		cos_wait(main_fd);
		while (1) {
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
