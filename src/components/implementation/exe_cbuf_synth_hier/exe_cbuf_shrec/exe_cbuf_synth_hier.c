#include <cos_component.h>

#include <sched.h>
#include <sched_conf.h>
#include <print.h>


#include <exe_cbuf_synth_hier.h>
#include <cbuf.h>

#define MAXULONG (~0)

#include <stdlib.h>

#define TOTAL_AMNT 128		/* power of 2 */

unsigned int spin = 1000, l_to_r = 64, num_invs = 1, cbuf_l_to_r = 1;

#define AVG_INVC_CYCS 1000   /* From the measurement */

#define NUM_LOOPS 10000

#define PERCENT_EXE 10

#define SZ 4096  // size of one cbuf item
#define NCBUF 10   // number of cbufs to create each time

#define ALLOC_CBUF
#define CBUF2BUF

#define DEBUG_SYNTH

#ifdef DEBUG_SYNTH
#define DOUTs(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define DOUTs(fmt, ...)
#endif


volatile unsigned long kkk = 0;
unsigned long loop_cost = 0;

static inline void do_loop(unsigned long iter)
{
	unsigned long i;

 	for (i=0;i<iter;i++) kkk++;
}

static unsigned long get_loop_cost(unsigned long loop_num)
{
	u64_t start,end;

	rdtscll(start);
	do_loop(loop_num);
	rdtscll(end);

	return end-start; 
}

static char *parse_step(char *d)
{
	char *s = strchr(d, ' ');
	if (!s) {
		if ('\0' == d) return d;
		s = d + strlen(d);
	} else {
		*s = '\0';
		s++;
	}

	switch(*d) {
	case 's':		/* spin */
		spin = atoi(++d);
		break;
	case 'r':		/* ratio */
		l_to_r = atoi(++d);
		if (l_to_r > TOTAL_AMNT) l_to_r = TOTAL_AMNT;
		break;
	case 'n':		/* num of invocations */
		num_invs = atoi(++d);
		break;
	}

	return s;
}

static void parse_initstr(void)
{
	struct cos_array *data;
	char *c;
	static int parsed = 0;

	if (parsed) return;
	parsed = 1;

	data = cos_argreg_alloc(sizeof(struct cos_array) + 52);
	assert(data);
	data->sz = 52;
	
	if (sched_comp_config_initstr(cos_spd_id(), data)) {
		printc("No initstr found.\n");
		return;
	}
	//DOUTs("%s\n", data->mem);

	c = data->mem;
	while ('\0' != *c) c = parse_step(c);
	
	cos_argreg_free(data);
}

extern unsigned long calll_left(unsigned long exe_t, unsigned long const initial_exe_t, cbuf_t cbt, int len);
extern unsigned long callr_right(unsigned long exe_t, unsigned long  const initial_exe_t, cbuf_t cbt, int len);

static unsigned long measure_loop_costs(unsigned long spin) 
{
	
	unsigned long temp = 0;
	temp = 6 * spin;
	loop_cost = temp;
	return temp;
	/*rdtscll(start);
	do {
		int i;
		min = MAXULONG;
		max= 0;
		for (i = 0 ; i < 10 ; i++) {
			temp = get_loop_cost(spin);
			if (temp < min) min = temp;
			if (temp > max) max = temp;
		}
	} while ((max-min) > (min/128));

	loop_cost = temp;
	rdtscll(end);
	assert(end>start);
	DOUTs("spin:%lu, loopcost measurement :%lu\n",spin, temp );
	return temp;*/
}

static unsigned long do_action(unsigned long exe_time_left, const unsigned long initial_exe_t, cbuf_t cbt_map, int len_map)
{

	unsigned long i, j, val;
	unsigned long long t;
	int mark = 0;
	int len = SZ;

	static int first = 1;

	unsigned long has_run;   /* thread has run cycles in this inv */
	
	u32_t id, idx;
	cbuf_t cbt[NCBUF];
	memset(cbt, 0 , NCBUF*sizeof(cbuf_t));

	void *mt[NCBUF];
	int get[NCBUF];
	memset(get, 0 , NCBUF*sizeof(cbuf_t));

	parse_initstr();
	/* DOUTs("thd %d enter comp %ld!\n", cos_get_thd_id(), cos_spd_id()); */
	if (first) {
		unsigned long temp = 0;
		temp = measure_loop_costs(spin);
		first = 0;
		/*if (exe_time_left < temp) return 0;
		  exe_time_left -= temp;*/
	}
	if (AVG_INVC_CYCS > exe_time_left) return 0;
	exe_time_left -= AVG_INVC_CYCS;

	u64_t start,end;	
#ifdef CBUF2BUF
	char *b;
	if(cbt_map && len_map){
		rdtscll(start);
		b = cbuf2buf(cbt_map,len_map);
		rdtscll(end);
		DOUTs("---- cost Bf2Bf :: %llu in spd %ld\n", end-start, cos_spd_id());
		if (!b) {
			DOUTs("Can not map into this spd %ld\n", cos_spd_id());
			return cbuf_null();
		}
		memset(b, 's', len_map);
		/* DOUTs("after buf2buf write sth...\n"); */
	}
#endif
	for (j = 0 ; j < num_invs ; j++) {
		if (exe_time_left == 0) return 0;
		kkk = 0;

		for (i=0;i<spin;i++) kkk++;
		
		unsigned long ss = initial_exe_t / (100 / PERCENT_EXE) / 6;
		for (i=0; i<ss; i++) kkk++;
		has_run = ss * 6;//loop_cost;//

		if (has_run > exe_time_left) {
			return 0;
		}
		exe_time_left -= has_run;

#ifdef ALLOC_CBUF
		for (i = 0; i < NCBUF ; i++){
			rdtscll(t);
			val = (int)(t & (TOTAL_AMNT-1));
			if (val >= cbuf_l_to_r){
				cbt[i] = cbuf_null();
				rdtscll(start);
				mt[i] = cbuf_alloc(len, &cbt[i]);
				rdtscll(end);
				cbuf_unpack(cbt[i], &id, &idx);

				/* DOUTs("---- cost Alloc :: %llu in spd %ld\n", end-start, cos_spd_id()); */
				/* DOUTs("Thd %d create in spd %ld, memid %x, idx %x\n", cos_get_thd_id(), cos_spd_id(), id, idx); */
				memset(mt[i], 'a', len);
				/* DOUTs("after alloc write sth...\n"); */
				get[i] = 1;
				mark = 1;
			}
		}
#endif

		rdtscll(t);
		val = (int)(t & (TOTAL_AMNT-1));

		if(mark == 1){
			if (val >= l_to_r) {
				exe_time_left = calll_left(exe_time_left, initial_exe_t , cbt[0], len);
				
			} else {
				exe_time_left = callr_right(exe_time_left, initial_exe_t, cbt[0], len);
			}
		}
		else{
			if (val >= l_to_r) {
				exe_time_left = calll_left(exe_time_left, initial_exe_t , 0, 0);
				
			} else {
				exe_time_left = callr_right(exe_time_left, initial_exe_t, 0, 0);
			}
		}	

#ifdef ALLOC_CBUF
		for (i = 0; i < NCBUF ; i++){
			if (get[i] == 1){
				get[i] = 0;
				rdtscll(start);
				cbuf_free(mt[i]);
				rdtscll(end);
				/* DOUTs("---- cost Freed :: %llu in spd %ld\n", end-start, cos_spd_id()); */
				/* DOUTs("EXECBUF i %lu : thd %d freed in spd %ld\n",i, cos_get_thd_id(), cos_spd_id()); */
			}
		}
#endif

	}

	return exe_time_left;
}

unsigned long left(unsigned long exe_t, unsigned long const  initial_exe_t, cbuf_t cbt, int len)
{
	return do_action(exe_t,initial_exe_t, cbt, len);
}

unsigned long right(unsigned long exe_t,  unsigned long const initial_exe_t, cbuf_t cbt, int len)
{
	return do_action(exe_t,initial_exe_t, cbt, len);
}


void cos_init(void)
{

	while (1) do_action(10000,10000,0,0);
}
