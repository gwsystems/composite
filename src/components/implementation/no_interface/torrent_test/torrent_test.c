/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

char buffer[1024];

void cos_init(void)
{
	td_t t1, t2;
	long evt1, evt2;
	char *params1 = "bar";
	char *params2 = "foo/";
	char *params3 = "foo/bar";
	char *data1 = "1234567890", *data2 = "asdf;lkj";
	int ret1, ret2;
	
	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1)+1, TOR_ALL, evt1);
	if (t1 < 1) {
		printc("split failed %d\n", t1);
		return;
	}
	trelease(cos_spd_id(), t1);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2) + 1, TOR_ALL, evt1);
	if (t1 < 1) {
		printc("split2 failed %d\n", t1); return;
	}
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1) + 1, TOR_ALL, evt2);
	if (t2 < 1) {
		printc("split3 failed %d\n", t2); return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)+1);
	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2)+1);
	printc("write %d & %d, ret %d & %d\n", strlen(data1)+1, strlen(data2)+1, ret1, ret2);

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2) + 1, TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params2, strlen(params2) + 1, TOR_ALL, evt2);
	if (t1 < 1 || t2 < 1) {
		printc("later splits failed\n");
		return;
	}
	
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printc("read %d: %s\n", ret1, buffer);
	buffer[0] = '\0';

	ret1 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printc("read %d: %s\n", ret1, buffer);
	buffer[0] = '\0';

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);

	t1 = tsplit(cos_spd_id(), td_root, params3, strlen(params3) + 1, TOR_ALL, evt1);
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printc("read %d: %s\n", ret1, buffer);
	buffer[0] = '\0';
	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)+1);
	printc("write %d, ret %d\n", strlen(data1)+1, ret1);

	trelease(cos_spd_id(), t1);
	t1 = tsplit(cos_spd_id(), td_root, params3, strlen(params3) + 1, TOR_ALL, evt1);
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printc("read %d: %s\n", ret1, buffer);
	buffer[0] = '\0';

	return;
}
