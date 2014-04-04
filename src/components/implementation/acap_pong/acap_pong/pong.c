#include <cos_component.h>
#include <print.h>
#include <acap_pong.h>

#include <par_mgr.h>
#include <cos_alloc.h>
#include <sched.h>

//volatile int f;
//void call(void) { f = *(int*)NULL; return; }
int call(int a, int b, int c, int d) { 
	printc("core %ld, spd %ld: doing call in pong with params %d %d %d %d\n",
	       cos_cpuid(), cos_spd_id(), a,b,c,d);
	return a+b+c+d; 
}

/////////////////// move to lib later
int cos_ainv_handling(void) {
	struct __cos_ainv_srv_thd curr_data = { .stop = 0 };
	struct __cos_ainv_srv_thd *curr = &curr_data;
	int acap, i;
	int curr_thd_id = cos_get_thd_id();

	assert(curr);

	printc("upcall thread %d (core %ld) waiting in pong...\n", cos_get_thd_id(), cos_cpuid());
	sched_block(cos_spd_id(), 0);
	printc("upcall thread %d (core %ld) up!\n", cos_get_thd_id(), cos_cpuid());
		
	curr->acap = acap_srv_lookup(cos_spd_id());
	curr->cli_ncaps = acap_srv_ncaps(cos_spd_id());
	curr->shared_page = acap_srv_lookup_ring(cos_spd_id());
	assert(curr->acap && curr->cli_ncaps && curr->shared_page);

	init_shared_page(&curr->shared_struct, curr->shared_page);

	curr->fn_mapping = malloc(sizeof(vaddr_t) * curr->cli_ncaps);
	if (unlikely(curr->fn_mapping == NULL)) goto err_nomem;
	for (i = 0; i < curr->cli_ncaps; i++) {
		curr->fn_mapping[i] = (vaddr_t)acap_srv_fn_mapping(cos_spd_id(), i);
	}
	
	assert(curr);
	acap = curr->acap;

	printc("server %ld, upcall thd %d has acap %d.\n", 
	       cos_spd_id(), curr_thd_id, acap);

	struct shared_struct *shared_struct = &curr->shared_struct;
	CK_RING_INSTANCE(inv_ring) *ring = shared_struct->ring;
	assert(ring);

	struct inv_data inv;
	while (curr->stop == 0) {
		CLEAR_SERVER_ACTIVE(shared_struct); // clear active early to avoid race (and atomic instruction)
		if (CK_RING_DEQUEUE_SPSC(inv_ring, ring, &inv) == false) {
			printc("thread %d waiting on acap %d\n", cos_get_thd_id(), acap);
			cos_areceive(acap);
			printc("thread %d up from areceive\n", cos_get_thd_id());
		} else {
			SET_SERVER_ACTIVE(shared_struct); /* setting us active */
			printc("core %ld: got inv for cap %d, param %d, %d, %d, %d\n",
			       cos_cpuid(), inv.cap, inv.params[0], inv.params[1], inv.params[2], inv.params[3]);
			if (unlikely(inv.cap > curr->cli_ncaps || !curr->fn_mapping[inv.cap])) {
				printc("Server thread %d in comp %ld: receiving invalid cap %d\n",
				       cos_get_thd_id(), cos_spd_id(), inv.cap);
			} else {
				assert(curr->fn_mapping[inv.cap]);
				//execute!
				exec_fn((void *)curr->fn_mapping[inv.cap], 4, inv.params);
				// and write to the return value.
			}
		}
	}

	return 0;
err_nomem:
	printc("couldn't allocate memory in spd %ld\n", cos_spd_id());
	return -1;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_THD_CREATE:
	{
		cos_ainv_handling();
		break;
	}
	default:
		/* fault! */
		//*(int*)NULL = 0;
		printc("\n upcall type t %d\n", t);
		return;
	}
	return;
}
