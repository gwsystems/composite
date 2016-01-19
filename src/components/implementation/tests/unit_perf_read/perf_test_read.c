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
#define DEBUG 1

#ifndef MIN
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#endif

size_t xfersize = 512;
size_t count = 512;

/* analogous to bzero, bcopy, etc., except that it just reads
* data into the processor
*/
long
bread(void* buf, long nbytes)
{
	long sum = 0;
	register long *p, *next;
	register char *end;
	p = (long*)buf;
	end = (char*)buf + nbytes;
	for (next = p + 128; (void*)next <= (void*)end; p = next, next += 128) {
		sum +=
		p[0]+p[1]+p[2]+p[3]+p[4]+p[5]+p[6]+p[7]+
		p[8]+p[9]+p[10]+p[11]+p[12]+p[13]+p[14]+
		p[15]+p[16]+p[17]+p[18]+p[19]+p[20]+p[21]+
		p[22]+p[23]+p[24]+p[25]+p[26]+p[27]+p[28]+
		p[29]+p[30]+p[31]+p[32]+p[33]+p[34]+p[35]+
		p[36]+p[37]+p[38]+p[39]+p[40]+p[41]+p[42]+
		p[43]+p[44]+p[45]+p[46]+p[47]+p[48]+p[49]+
		p[50]+p[51]+p[52]+p[53]+p[54]+p[55]+p[56]+
		p[57]+p[58]+p[59]+p[60]+p[61]+p[62]+p[63]+
		p[64]+p[65]+p[66]+p[67]+p[68]+p[69]+p[70]+
		p[71]+p[72]+p[73]+p[74]+p[75]+p[76]+p[77]+
		p[78]+p[79]+p[80]+p[81]+p[82]+p[83]+p[84]+
		p[85]+p[86]+p[87]+p[88]+p[89]+p[90]+p[91]+
		p[92]+p[93]+p[94]+p[95]+p[96]+p[97]+p[98]+
		p[99]+p[100]+p[101]+p[102]+p[103]+p[104]+
		p[105]+p[106]+p[107]+p[108]+p[109]+p[110]+
		p[111]+p[112]+p[113]+p[114]+p[115]+p[116]+
		p[117]+p[118]+p[119]+p[120]+p[121]+p[122]+
		p[123]+p[124]+p[125]+p[126]+p[127];
	}
	for (next = p + 16; (void*)next <= (void*)end; p = next, next += 16) {
		sum +=
		p[0]+p[1]+p[2]+p[3]+p[4]+p[5]+p[6]+p[7]+
		p[8]+p[9]+p[10]+p[11]+p[12]+p[13]+p[14]+
		p[15];
	}
	for (next = p + 1; (void*)next <= (void*)end; p = next, next++) {
		sum += *p;
	}
	return sum;
}

long long doit(td_t t)
{
	long long time1, time2;
	int off, sz;
	int ret = 0;
	size_t size, chunk;
	char *buf;

	size = count; // need count
	chunk = xfersize; // this too

	/* take clock ticks */
	rdtscll(time1);

	while (size > 0) {
		if (size < chunk) chunk = size;
		printc("Calling read\n");
		ret = treadp(cos_spd_id(), t, &off, &sz);

		if (!ret) break;

		buf = cbuf2buf(ret, sz);

		bread(buf, MIN(size, xfersize));
		size -= chunk;
	}

	/* take clock ticks after */
	rdtscll(time2);

	long long diff = time2 - time1;
	
#if DEBUG
	printc("read: [%s]\n", buf);	
	printc("t1: %lld t2: %lld diff: %lld\n", time1, time2, diff);
#endif

	return diff;
}

void call(void)
{
	printc("Starting read test");
	
	long evt1 = 0;
	td_t t;
	char *params1 = "bar";
	int i;
	
	/* open file */
	t = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t < 1) { printc("UNIT TEST FAILED: split failed %c\n", t); return; }

	for (i = 0; i <= 10; i++) {
		unsigned int iterations = 5;
		unsigned int max = iterations;
		long long total = 0;

		while (0 < iterations--) {
			total += doit(t);
		}
		long long average = (long long) ((long double) total / (long double) max);
		printc("avr clock ticks for iteration %d: %lld\n", i, average);
	}

	/* close file */
	trelease(cos_spd_id(), t);
}

void cos_init(void)
{
}
