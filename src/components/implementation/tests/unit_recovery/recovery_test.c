#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

#include <pgfault.h>

char *params[3];
char *data[3];
long evt[3];

char buffer[1024];

void read(char *r_params, char *r_data, long r_evt)
{
	unsigned int ret;
	td_t s_tid;

	s_tid = tsplit(cos_spd_id(), td_root, r_params, strlen(r_params), TOR_ALL, r_evt);
	if (s_tid < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", s_tid);
		return;
	}

	ret = tread_pack(cos_spd_id(), s_tid, buffer, 1023);

	if (ret > 0) buffer[ret] = '\0';
	assert(!strcmp(buffer, r_data));
	assert(ret == strlen(r_data));
	buffer[0] = '\0';

	trelease(cos_spd_id(), s_tid);
	return;
}

void write(char *w_params, char *w_data, long w_evt)
{
	unsigned int ret;
	td_t s_tid;

	s_tid = tsplit(cos_spd_id(), td_root, w_params, strlen(w_params), TOR_ALL, w_evt);
	if (s_tid < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", s_tid);
		return;
	}

	ret = twrite_pack(cos_spd_id(), s_tid, w_data, strlen(w_data));

	trelease(cos_spd_id(), s_tid);

	return;
}

void write_tor1()
{
	return write(params[0], data[0], evt[0]);
}

void write_tor2()
{
	return write(params[1], data[1], evt[1]);
}

void write_tor3()
{
	return write(params[2], data[2], evt[2]);
}

void read_tor1()
{
	return read(params[0], data[0], evt[0]);
}

void read_tor2()
{
	return read(params[1], data[1], evt[1]);
}

void read_tor3()
{
	return read(params[2], data[2], evt[2]);
}


void cos_init(void)
{
	td_t t1, t2, t3;

	params[0] = "tor1";
	params[1] = "tor2";
	params[2] = "tor3";
	
	data[0] = "data_111_tor1";
	data[1] = "data_2222_tor22";
	data[2] = "data_33333_tor333";

	evt[0] = evt_create(cos_spd_id());
	evt[1] = evt_create(cos_spd_id());
	evt[2] = evt_create(cos_spd_id());
	assert(evt[0] > 0 && evt[1] > 0 && evt[2] > 0);

	t1 = tsplit(cos_spd_id(), td_root, params[0], strlen(params[0]), TOR_ALL, evt[0]);
	if (t1 < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", t1);
		return;
	}
	
	t2 = tsplit(cos_spd_id(), td_root, params[1], strlen(params[1]), TOR_ALL, evt[1]);
	if (t2 < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", t2);
		return;
	}
	
	t3 = tsplit(cos_spd_id(), td_root, params[2], strlen(params[2]), TOR_ALL, evt[2]);
	if (t3 < 1) {
		printc("UNIT TEST FAILED: split failed %d\n", t3);
		return;
	}

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t3);
	
	printc("Recovery TEST. 3 files have been created:\n ");
	printc("t1 %d t2 %d t3 %d\n", t1,t2,t3);
	printc("evt1 %ld evt2 %ld evt3 %ld\n", evt[0], evt[1], evt[2]);

	write_tor1();

 	read_tor1();

	printc("Recovery TEST PASSED\n");
	return;
}
