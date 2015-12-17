/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <stdio.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

char buffer[1024];

void twritep_readp_tests(void)
{
	td_t t1, t2;
	long evt1, evt2;
	char *params1 = "bar";
	char *params2 = "foo/";
	char *params3 = "foo/bar";
	char *data1 = "1234567890", *data2 = "asdf;lkj", *data3 = "asdf;lkj1234567890";
	int ret1, ret2;

	int a = 0, b = 0, c = 0;
	
	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split2 failed %c\n", t1); return;
	}
	
	cbuf_t cb;
	char *d = cbuf_alloc(10, &cb);
	if (!d) {
		printc("UNIT TEST FAILED: split2 failed %c\n", d); return;
	}
	memcpy(d, "hello!", 6);

	printc("\n\ncalling write 1\n");
	c = twritep(cos_spd_id(), t1, cb, 0, 6);
	
	// Another write
	cbuf_t cb2;
	char *e = cbuf_alloc(30, &cb2);
	if (!e) {
		printc("UNIT TEST FAILED: split2 failed %c\n", e); return;
	}
	memcpy(e, "This is a hardcoded string.", 27);

	char val[8];
	snprintf(val, 8, "%d", 4);
	int rt = twmeta(cos_spd_id(), t1, "offset", strlen("offset"),
		 val, strlen(val));
	int sz = 20;
	printc("spd_id(): %d\ntorrent: %d\ncbuf: %d\nstart: %d\nsz: %d\n", cos_spd_id(), t1, cb2, 0, sz);
	printc("\n\ncalling write 2\n");
	c = twritep(cos_spd_id(), t1, cb2, 0, sz);

	// Close the file so we can read from it
	trelease(cos_spd_id(), t1);





	/* End of writing stage */
	/* Going to test reading next */

	// now test read
	a = 0; b = 0; c = 0;
	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split2 failed %d\n", t1); return;
	}

	printc("\n\ncalling read\n");
	c = treadp(cos_spd_id(), t1, &a, &b);
	char *buf = cbuf2buf(c, 6);
	printc("Expected:[%s] Actual[%s]\n", buf, d);
	
	printc("\n\ncalling read\n");
	c = treadp(cos_spd_id(), t1, &a, &b);
	char *buf2 = cbuf2buf(c, 27);
	printc("Expected:[%s] Actual[%s]\n", buf2, e);

	printc("UNIT TEST Unit tests for torrents - specifically treadp, twritep... are done\n");

	return;
}

void twrite_read_tests(void)
{
	td_t t1, t2;
	long evt1, evt2;
	char *params1 = "bar";
	char *params2 = "foo/";
	char *params3 = "foo/bar";
	char *data1 = "1234567890", *data2 = "asdf;lkj", *data3 = "asdf;lkj1234567890";
	unsigned int ret1, ret2;

	int a = 0, b = 0, c = 0;
	c = treadp(cos_spd_id(), 0, &a, &b);

	printc("UNIT TEST Unit tests for torrents...\n");

	printc("%d %d %d\n", a, b, c);

	evt1 = evt_split(cos_spd_id(), 0, 0);
	evt2 = evt_split(cos_spd_id(), 0, 0);
	assert(evt1 > 0 && evt2 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", t1);
		return;
	}
	trelease(cos_spd_id(), t1);
	printc("UNIT TEST PASSED: split->release\n");

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split2 failed %d\n", t1); return;
	}
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("UNIT TEST FAILED: split3 failed %d\n", t2); return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	printv("write %d & %d, ret %d & %d\n", strlen(data1), strlen(data2), ret1, ret2);

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	printc("UNIT TEST PASSED: 2 split -> 2 write -> 2 release\n");

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t1 < 1 || t2 < 1) {
		printc("UNIT TEST FAILED: later splits failed\n");
		return;
	}
	
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printv("read %d (%d): %s (%s)\n", ret1, strlen(data1), buffer, data1);
	assert(!strcmp(buffer, data1));
	assert(ret1 == strlen(data1));
	buffer[0] = '\0';

	ret1 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	assert(!strcmp(buffer, data2));
	assert(ret1 == strlen(data2));
	printv("read %d: %s\n", ret1, buffer);
	buffer[0] = '\0';

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);

	printc("UNIT TEST PASSED: 2 split -> 2 read -> 2 release\n");

	t1 = tsplit(cos_spd_id(), td_root, params3, strlen(params3), TOR_ALL, evt1);
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printv("read %d: %s\n", ret1, buffer);
	assert(!strcmp(buffer, data2));
	assert(ret1 == strlen(data2));
	printc("UNIT TEST PASSED: split with absolute addressing -> read\n");
	buffer[0] = '\0';

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	printv("write %d, ret %d\n", strlen(data1), ret1);

	trelease(cos_spd_id(), t1);
	t1 = tsplit(cos_spd_id(), td_root, params3, strlen(params3), TOR_ALL, evt1);
	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0 && ret1 < 1024) buffer[ret1] = '\0';
	printv("read %d: %s (%s)\n", ret1, buffer, data3);
	assert(ret1 == strlen(data2)+strlen(data1));
	assert(!strcmp(buffer, data3));
	buffer[0] = '\0';
	printc("UNIT TEST PASSED: writing to an existing file\n");

	printc("UNIT TEST ALL PASSED\n");

	return;
}

void cos_init(void)
{
	//twrite_read_tests();
	twritep_readp_tests();
}
