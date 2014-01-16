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
  //  printc("I am the client\n");
        td_t t1, serv;
	long evt;
	char *params1 = "foo", *params2 = "";
	int period, num, ret;

	printc("ryx : asc test client begin\n");
	parse_args(&period, &num);

	evt = evt_split(cos_spd_id(), 0, 0);
	assert(evt > 0);
      	serv = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_RW, evt);
	if (serv < 1) 
		printc("UNIT TEST FAILED: split1 failed %d\n", serv); 
	evt_wait(cos_spd_id(), evt);
	printc("client split successfully\n");
	periodic_wake_create(cos_spd_id(), period);
	/* 
	 * insert evt_grp_wait(...) into the code below where it makes
	 * sense to.  Simulate if the code were executing in separate
	 * threads.
	 */

	cbufp_t cb1;
	char *d;
        u64_t start = 0;
        int sz, i, j;
        sz = 4096;
        for(i=1; i<=ITER; i++) {
                for(j=0; j<num; j++) {
                	d = cbufp_alloc(i*sz, &cb1);
                        if (!d) goto done;
                        cbufp_send_deref(cb1);
                        rdtscll(start);
                        ((u64_t *)d)[0] = start;
                        ret = twritep(cos_spd_id(), serv, cb1, i*sz);
                }
                periodic_wake_wait(cos_spd_id());
        }
done:
	trelease(cos_spd_id(), serv);
	printc("client UNIT TEST PASSED: split->release\n");

	printc("client UNIT TEST ALL PASSED\n");
	return;
}
