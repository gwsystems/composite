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
	int have_data;	// do we need this or do we just assume it is blocked if it has sent data???
};

struct nmod_comp {
	struct replica_info *replicas;
	int nreplicas;

	struct nmod_comp **receivers;
	int nreceivers;

	int ndata_recv;	// how many replicas have we received data from.
	int send_data, have_send_data;
	int recv_data, have_recv_data;
};

int n_comps = 2;
replica_type map[250];

struct nmod_comp components[2];

void cos_fix_spdid_metadata(spdid_t o_spd, spdid_t f_spd) { }

struct nmod_comp *get_component(spdid_t spdid) {
	replica_type type = map[spdid];
	int i;

	if (type == none) return NULL;

	if (type == ping) {
		for (i = 0; i < components[0].nreplicas; i++) {
			if (components[0].replicas[i].spdid == spdid) {
				return &components[0];
			}
		}
		return NULL;
	}
	else if (type == pong) {
		for (i = 0; i < components[1].nreplicas; i++) {
			if (components[1].replicas[i].spdid == spdid) {
				return &components[1];
			}
		}
		return NULL;
	}
	else {
		return NULL;
	}
}

struct replica_info *get_replica(spdid_t spdid) {
	struct nmod_comp *component = NULL;
	int i;
	replica_type type = map[spdid];
	if (type == none) return NULL;

	component = get_component(spdid);
	if (!component) return NULL;

	for (i = 0; i < component->nreplicas; i++) {
		if (component->replicas[i].spdid == spdid) {
			return &component->replicas[i];
		}
	}

	return NULL;
}

int nwrite(spdid_t spdid, int to, int data) {
	printc("write got called from %d to %d with data %d\n", spdid, to, data);
	struct replica_info *replica = get_replica(spdid);
	struct replica_info *receiver;
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *receiver_comp;
	int ret;
	int i, j;
	if (!replica) BUG();

	if (replica->have_data) {
		printc("This replica has already sent some data down the channel\n");
		return -1; // whatever, shouldn't happen if blocked
	}
	else {
		replica->have_data = 1;
		component->recv_data = data;
		component->ndata_recv++;
		if (!replica->thread_id) replica->thread_id = cos_get_thd_id();

		// wake up any replicas waiting for data
		if (component->ndata_recv == component->nreplicas) {
			printc("Have received enough data to send to %d receivers\n", component->nreceivers);
			for (i = 0; i < component->nreceivers; i++) {
				receiver_comp = component->receivers[i];
				receiver_comp->have_send_data = 1;
				receiver_comp->send_data = component->recv_data;

				for (j = 0; j < receiver_comp->nreplicas; j++) {
					printc("About to wake up a replica\n");
					receiver = &receiver_comp->replicas[j];
					printc("Replica to wake has spdid %d with block %d\n", receiver->spdid, receiver->blocked);
					assert(receiver && receiver->spdid);
					if (receiver->spdid == spdid) { printc("Wait, this replica IS us\n"); continue; }	// shouldn't happen

					if (receiver->blocked) {
						printc("Replica is indeed blocked\n");
						receiver->blocked = 0;
						printc("waking up thread %d\n", receiver->thread_id);
						assert(receiver->thread_id); // if we got blocked without saving the thread id, we're basically doomed
						sched_wakeup(cos_spd_id(), receiver->thread_id);
					}
				}
			}

			// is this when we do this?
			component->ndata_recv = 0;
		}

		printc("Blocking thread %d\n", replica->thread_id);
		replica->blocked = 1;
		ret = sched_block(cos_spd_id(), 0);
		if (ret < 0) printc("Error\n");
	
		printc("Finally returning data from write with thread %d\n", cos_get_thd_id());
		return component->send_data;
	}
}

int nread(spdid_t spdid, int from, int data) {
	printc("read got called from %d from %d with data %d\n", spdid, from, data);

	struct replica_info *replica = get_replica(spdid);
	struct replica_info *recv_replica;
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *receiver_comp;
	int ret, j;
	int i;
	if (!replica) BUG();

	if (component->have_send_data) {
		component->have_send_data = 0;
		return component->send_data;
	}
	else {
		// should probably be voter that wakes threads up
		for (i = 0; i < component->nreceivers; i++) {
			receiver_comp = component->receivers[i];

			for (j = 0; j < receiver_comp->nreplicas; j++) {
				recv_replica = &receiver_comp->replicas[j];
				if (recv_replica->spdid == spdid) continue;	// shouldn't happen

				if (recv_replica->blocked) {
					recv_replica->blocked = 0;
					printc("waking up thread %d\n", recv_replica->thread_id);
					sched_wakeup(cos_spd_id(), recv_replica->thread_id);
				}
			}
		}
		
		if (!replica->thread_id) replica->thread_id = cos_get_thd_id();
		replica->blocked = 1;
		printc("Blocking thread %d\n", replica->thread_id);
		ret = sched_block(cos_spd_id(), 0);
		if (ret < 0) printc("Error");
	
		printc("Finally returning data from read with thread %d\n", cos_get_thd_id());
		assert(component->have_send_data);
		component->have_send_data = 0; // well this is wrong because maybe other replicas still need to read
		return component->send_data;
	}

	return 0;

}

int confirm(spdid_t spdid, replica_type type) {
	printc("Confirming readiness of spdid %d\n", spdid);
	
	int ret;
	int i;
	
	int t = (type == ping) ? 0 : 1;	
	struct replica_info *replicas = components[t].replicas;
	
	for (i = 0; i < components[t].nreplicas; i++) {
		if (replicas[i].spdid == 0) {
			printc("creating replica for spdid %d in slot %d\n", spdid, i);
			replicas[i].spdid = spdid;
			//replicas[i].spdid = quarantine_fork(cos_spd_id(), comp2fork);
			//if (replicas[i].spdid == 0) printc("Error: f1 fork failed\n");
			replicas[i].buf_read = cbuf_alloc(1024, &replicas[i].read_buffer); 
			replicas[i].buf_write = cbuf_alloc(1024, &replicas[i].write_buffer);
			replicas[i].blocked = 0;
			replicas[i].have_data = 0;
			replicas[i].thread_id = 0;

			map[spdid] = type;
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
	int i, j;
	for (i = 0; i < n_comps; i++) {
		printc("Setting up nmod component %d\n", i);
		components[i].nreplicas = 1; // for now, no fork
		components[i].nreceivers = 1;
		components[i].replicas = (struct replica_info*) malloc(sizeof(struct replica_info) * components[i].nreplicas);
		components[i].receivers = (struct replica_info**) malloc(sizeof(struct replica_info *) * components[i].nreceivers);
	
		for (j = 0; j < components[i].nreplicas; j++) {
			components[i].replicas[j].spdid = 0;
			components[i].replicas[j].blocked = 0;
		}
	}

	for (i = 0; i < 250; i++) {
		map[i] = none;
	}
	
	// wrong - nmod comp should know who its receiver is tho. Replica can't specify because not trusted
	components[0].receivers[0] = &components[1];
	components[1].receivers[0] = &components[0];
}
