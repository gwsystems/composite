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

#include <fd.h>
#include <sched.h>

static int main_fd, data_fd;
const char *service_names[] = {
	"/cgi/hw",
	"/cgi/HW",
	NULL
};
const char *msg = "hello world";
#define BUFF_SZ 1024

void cos_init(void *arg)
{
	struct cos_array *data;
	int i;
	data = cos_argreg_alloc(BUFF_SZ + sizeof(struct cos_array));

	for (i = 0 ; NULL != service_names[i] ; i++) {
		memcpy(data->mem, service_names[i], strlen(service_names[i]));
		data->sz = strlen(service_names[i]);
		if (0 > (main_fd = cos_app_open(0, data))) {
			printc("cgi: cannot open service, ret=%d\n", main_fd);
			BUG();
		}
	}

	main_fd = cos_wait_all();
	assert(data);
	while (1) {
		cos_mpd_update();
		while (1) {
			int amnt;

			data_fd = cos_split(main_fd);
			if (-EAGAIN == data_fd) break;
			else if (0 > data_fd) BUG();

			do {
				amnt = cos_read(data_fd, data->mem, BUFF_SZ);
				if (0 > amnt) BUG();
				if (0 == amnt) break;
			} while (0 != amnt);

			memcpy(data->mem, msg, strlen(msg));
			cos_write(data_fd, data->mem, strlen(msg));
			cos_close(data_fd);
		}
		cos_wait(main_fd);
	}
}

void symb_bin(void)
{
	sched_block(cos_spd_id(), 0);
}
