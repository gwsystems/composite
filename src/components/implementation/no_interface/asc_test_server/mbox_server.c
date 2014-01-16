/**
 */

#include <stdlib.h>
#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>
#include <periodic_wake.h>
#define ITER 10
void parse_args(int *p, int *n)
{
	char *c;
	int i = 0, s = 0;
	c = cos_init_args();
	while(c[i] != ' ') {
		s = 10*s+c[i]-'0';
		i++;
	}
	*p = s;
	s = 0;
	i++;
	while(c[i] != '\0') {
		s = 10*s+c[i]-'0';
		i++;
	}
	*n = s;
	return ;
}
void cos_init(void *arg)
{
        td_t t1 = td_root, cli;
	long evt;
	char *params1 = "foo", *params2 = "";
	int period, num;
	parse_args(&period, &num);
	evt = evt_split(cos_spd_id(), 0, 0);
	assert(evt > 0);
       	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL | TOR_NONPERSIST, evt);
	if (t1 < 1) 
		printc("UNIT TEST FAILED: split failed %d\n", t1);
	evt_wait(cos_spd_id(), evt);
       	cli = tsplit(cos_spd_id(), t1, params2, strlen(params2), TOR_RW, evt);
	if (cli < 1) 
		printc("UNIT TEST FAILED: split1 failed %d\n", cli);
	periodic_wake_create(cos_spd_id(), period);
	cbufp_t cb1;
	char *buf;
	int sz, off, i, j;
	u64_t start, end = 0;
	for(i=0; i<ITER; i++) {
		for(j=0; j<num; j++) {
			while(1) {
				cb1 = treadp(cos_spd_id(), cli, &off, &sz);
				if((int)cb1>0)
					break;
			}
			buf = cbufp2buf(cb1,sz);
			rdtscll(end);
			start = ((u64_t *)buf)[0];
			cbufp_deref(cb1);
//			printc("ryx: %llu s %llu e\n", start, end);
		}
		periodic_wake_wait(cos_spd_id());
	}
done:
	trelease(cos_spd_id(), cli);
	trelease(cos_spd_id(), t1);
	printc("server UNIT TEST PASSED: split->release\n");

	printc("server UNIT TEST ALL PASSED\n");
	return;
}
