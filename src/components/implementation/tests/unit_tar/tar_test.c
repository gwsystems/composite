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

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

char buffer[1024];

struct file {
	char *name, *data;
} files[] = {
	{.name = "fs/bar", .data = "bar\n"},
	{.name = "fs/foo", .data = "foo\n"},
	{.name = "fs/foobar/foo", .data = "foobarfoo\n"},
	{.name = NULL, .data = NULL}
};

int validate_data(struct file *f, long evt)
{
	int ret;
	td_t t;

	t = tsplit(cos_spd_id(), td_root, f->name, strlen(f->name), TOR_READ, evt);
	assert(t > 0);
	ret = tread_pack(cos_spd_id(), t, buffer, 1023);
	if (ret > 0) buffer[ret] = '\0';
	assert(!strcmp(buffer, f->data));
	assert(ret == (int)strlen(f->data));
	buffer[0] = '\0';

	trelease(cos_spd_id(), t);
	return 0;
}

void cos_init(void)
{
	long evt;
	int i;

	printc("UNIT TEST Unit tests for initial RO tar fs...\n");

	evt = evt_split(cos_spd_id(), 0, 0);
	for (i = 0 ; files[i].name ; i++) validate_data(&files[i], evt);

	printc("UNIT TEST ALL PASSED\n");

	return;
}
