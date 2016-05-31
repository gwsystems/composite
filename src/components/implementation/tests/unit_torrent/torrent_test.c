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
#include <perf_test_read.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif
#define DEBUG 0

long long doit(td_t t, int size)
{
	cbuf_t cb;
	long long time1, time2;
	int ret = 0;
	char *s = cbuf_alloc(size, &cb); //null terminator?

	memset(s, 'a', size - 2);
	s[size - 1] = '\0';

	if (!s) { printc("UNIT TEST FAILED: cbut alloc failed %s\n", s); return -1; }

	/* take clock ticks */
	rdtscll(time1);
	ret = twritep(cos_spd_id(), t, cb, 0, size);
	/* take clock ticks after */
	rdtscll(time2);

	long long diff = time2 - time1;	

#if DEBUG
	printc("t1: %lld t2: %lld diff: %lld\n", time1, time2, diff);
	printc("ret - sz: %d\n", ret - size);
#endif

	return diff;
}

void test_write(void)
{
	long evt1 = 0;
	td_t t;
	char *params1 = "bar";
	int chunk_size;
	
	/* open file */
	t = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t < 1) { printc("UNIT TEST FAILED: split failed %c\n", t); return; }

	for (chunk_size = 64; chunk_size <= 16384; chunk_size *= 2) {
		unsigned int iterations = 5;
		unsigned int max = iterations;
		long long total = 0;

		while (0 < iterations--) {
			total += doit(t, chunk_size);
		}
		long long average = (long long) ((long double) total / (long double) max);
		printc("avr clock ticks for chunk size %d: %lld\n", chunk_size, average);
	}

	/* close file */
	trelease(cos_spd_id(), t);

	call();
}

void perf_tests(void)
{
	test_write();
}

void twritep_readp_tests(void)
{
	td_t t1, t2;
	long evt1 = 0;
	char *params1 = "bar";
	int ret1, ret2;

	int a = 0, b = 0, c = 0;
	
	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) { printc("UNIT TEST FAILED: split2 failed %c\n", t1); return; }
	cbuf_t cb;
	char *str1 = "AAA games are just great!";
	char *d = cbuf_alloc(strlen(str1), &cb);
	if (!d) { printc("UNIT TEST FAILED: split2 failed %c\n", *d); return; }
	memcpy(d, str1, strlen(str1));
	c = twritep(cos_spd_id(), t1, cb, 0, strlen(str1));
	
	cbuf_t cb2;
	char *str2 = " Actually wait, no. What am I saying?";
	char *e = cbuf_alloc(strlen(str2), &cb2);
	if (!e) { printc("UNIT TEST FAILED: split2 failed %c\n", *e); return; }
	memcpy(e, str2, strlen(str2));
	c = twritep(cos_spd_id(), t1, cb2, 0, strlen(str2));

	cbuf_t cb3;
	char *str3 = "terrible!";
	char *f = cbuf_alloc(strlen(str3), &cb3);
	if (!f) { printc("UNIT TEST FAILED: split2 failed %c\n", *f); return; }
	memcpy(f, str3, strlen(str3));
	char val[8];
	snprintf(val, 8, "%d", 14);
	int rt = twmeta(cos_spd_id(), t1, "offset", strlen("offset"),
		 val, strlen(val));
	c = twritep(cos_spd_id(), t1, cb3, 0, strlen(str3));
	
	cbuf_t cb4;
	char *str4 = " Diablo 1 and Torchlight are the best! Also Borderlands.";
	char *g = cbuf_alloc(strlen(str4), &cb4);
	if (!g) { printc("UNIT TEST FAILED: split2 failed %c\n", *g); return; }
	memcpy(g, str4, strlen(str4));
	snprintf(val, 8, "%d", 23);
	rt = twmeta(cos_spd_id(), t1, "offset", strlen("offset"),
		 val, strlen(val));
	c = twritep(cos_spd_id(), t1, cb4, 0, strlen(str4));
	
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

	printc("UNIT TEST Unit tests for torrents - specifically treadp, twritep... are done\n");
	trelease(cos_spd_id(), t1);

	unsigned int i, j;
	int str_length = 0;
	unsigned long long t; unsigned long rnd;
	td_t t3;
	long evt2 = 0;
	char *params2 = "boo";
	t3 = tsplit(cos_spd_id(), td_root, params2, strlen(params1), TOR_ALL, evt2);
	if (t3 < 1) {
		printc("UNIT TEST FAILED: split2 failed %c\n", t3); return;
	}
	
	for (i = 0; i < 100; i++) {
		
		
		rdtscll(t);
		rnd = (int) (t & 127);
		int rnd_val;
		
		if (rnd > 0 && rnd < 42) {
			rnd_val = 0;
		}
		else if (rnd >= 42 && rnd < 84) {
			rnd_val = 1;
		}
		else
		{
			rnd_val = 2;
		}

		int offset;
		switch (rnd_val) {
			case 0:
				offset = 0;
			case 1:
				offset = 50;
			case 2: 
				offset = str_length;
		}

		// pick a random length
		unsigned int length = 15; // do the random part later
		
		cbuf_t cb;
		char *rnd_str = cbuf_alloc(length, &cb);
		if (!rnd_str) {
			printc("UNIT TEST FAILED: split failed %c\n", *rnd_str); return;
		}
		
		for (j = 0; j < length; j++) {
			rnd_str[j] = (char) i + 'a';
		}

		printc("%d: %s\n", i, rnd_str);
		int ret = twritep(cos_spd_id(), t3, cb, 0, length);
		printc("twritep %d return value: %d\n", i, ret);

	}
	
	trelease(cos_spd_id(), t3);




	return;
}

void cos_init(void)
{
	perf_tests();
	//twritep_readp_tests();
}
