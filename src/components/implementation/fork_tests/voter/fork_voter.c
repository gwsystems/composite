#include <cos_component.h>
#include <print.h>
#include <quarantine.h>
//#include <test_malloc_comp.h>
#include <cbuf.h>
#include <sched.h>
#include <voter.h>

struct replica_info {
	spdid_t spdid;
	cbuf_t read_buffer;
	cbuf_t write_buffer;
	void *buf_read;
	void *buf_write;
	unsigned short int thread_id;
	int blocked;
};

int n_replicas = 2;
struct replica_info replicas[2];

int have_data = 0;
int data_store;

void cos_fix_spdid_metadata(spdid_t o_spd, spdid_t f_spd) {
	printc("fixing stuff\n");
}
 
struct replica_info *get_replica(spdid_t spdid) {
	int i;
	for (i = 0; i < n_replicas; i++) {
		if (replicas[i].spdid == spdid) {
			return &replicas[i];
		}
	}

	return NULL;
}

int nwrite(spdid_t spdid, int to, int data) {
	printc("write got called from %d to %d with data %d\n", spdid, to, data);
	struct replica_info *replica = get_replica(spdid);
	int ret;
	int i;
	if (!replica) BUG();

	if (have_data) {
		return -1; // whatever
	}
	else {
		have_data = 1;
		data_store = data;
		replica->thread_id = cos_get_thd_id(); // need to only do this once ever per replica in final version

		// should probably be voter that wakes threads up
		for (i = 0; i < n_replicas; i++) {
			if (replicas[i].spdid == spdid) continue;

			if (replicas[i].blocked) {
				replicas[i].blocked = 0;
				printc("waking up thread %d\n", replicas[i].thread_id);
				sched_wakeup(cos_spd_id(), replicas[i].thread_id);
			}
		}

		printc("Blocking thread %d\n", replica->thread_id);
		replica->blocked = 1;
		ret = sched_block(cos_spd_id(), 0);
		if (ret < 0) printc("Can't do it\n");
	
		printc("Finally returning data from write with thread %d\n", cos_get_thd_id());
		return 0;
	}
}

int nread(spdid_t spdid, int from, int data) {
	printc("read got called from %d from %d with data %d\n", spdid, from, data);

	struct replica_info *replica = get_replica(spdid);
	int ret;
	int i;
	if (!replica) BUG();

	if (have_data) {
		have_data = 0;
		return data;
	}
	else {
		// should probably be voter that wakes threads up
		for (i = 0; i < n_replicas; i++) {
			if (replicas[i].spdid == spdid) continue;

			if (replicas[i].blocked) {
				replicas[i].blocked = 0;
				printc("waking up thread %d\n", replicas[i].thread_id);
				sched_wakeup(cos_spd_id(), replicas[i].thread_id);
			}
		}
		
		replica->thread_id = cos_get_thd_id();
		replica->blocked = 1;
		printc("Blocking thread %d\n", replica->thread_id);
		ret = sched_block(cos_spd_id(), 0);
		if (ret < 0) printc("Can't do it\n");
	
		printc("Finally returning data from read with thread %d\n", cos_get_thd_id());
		assert(have_data);
		have_data = 0;
		return data;
	}

}

int confirm(spdid_t spdid) {
	printc("Starting a pseudo-voter\n");
	
	int ret;
	int i;

	for (i = 0; i < n_replicas; i++) {
		if (replicas[i].spdid == 0) {
			printc("creating replica for spdid %d in slot %d\n", spdid, i);
			replicas[i].spdid = spdid;
			//replicas[i].spdid = quarantine_fork(cos_spd_id(), comp2fork);
			if (replicas[i].spdid == 0) printc("Error: f1 fork failed\n");
			replicas[i].buf_read = cbuf_alloc(1024, &replicas[i].read_buffer); 
			replicas[i].buf_write = cbuf_alloc(1024, &replicas[i].write_buffer);
			replicas[i].blocked = 0;
			return 0;
		}
		else {
			printc("replica for spdid %d already exists\n", replicas[i].spdid);
		}
	}

	return 0;
}

void cos_init(void)
{
	int i;
	for (i = 0; i < n_replicas; i++) {
		printc("Setting up replica %d\n", i);
		replicas[i].spdid = 0;
		replicas[i].blocked = 0;
	}
}
