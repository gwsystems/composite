#include <cos_component.h>
#include <print.h>
#include <quarantine.h>
#include <cbuf.h>
#include <sched.h>
#include <voter.h>
#include <periodic_wake.h>

#define N_COMPS 2			// number of components
#define N_CHANNELS 1			// number of channels between components
#define N_MAP_SZ 250			// number of spid's in the system. Super arbitrary

struct replica_info {
	spdid_t spdid;
	cbuf_t read_buffer;
	cbuf_t write_buffer;
	void *buf_read;
	void *buf_write;
	unsigned short int thread_id;
	int blocked;
};

struct nmod_comp {
	struct replica_info *replicas;
	int nreplicas;
};

struct channel {
	struct nmod_comp *A, *B;
	u64_t start;
	int A2B_data, have_A2B_data;	// make these arrays
	int B2A_data, have_B2A_data;
};

struct map_entry {
	replica_type type;
	struct replica_info *replica;
	struct nmod_comp *component;
};

struct nmod_comp components[N_COMPS];
struct channel channels[N_CHANNELS];
struct map_entry map[N_MAP_SZ];

int period = 100;

void cos_fix_spdid_metadata(spdid_t o_spd, spdid_t f_spd) { }

struct nmod_comp *get_component(spdid_t spdid) {
	printc("New type of map\n");
	return map[spdid].component;
}

struct replica_info *get_replica(spdid_t spdid) {
	return map[spdid].replica;
}

struct channel *get_channel(struct nmod_comp *A, struct nmod_comp *B) {
	// can we simplify this? Always order to and from in increasing order?
	int i;
	if (A == NULL || B == NULL) return NULL;

	for (i = 0; i < N_CHANNELS; i++) {
		if ((channels[i].A == A && channels[i].B == B) ||
		    (channels[i].A == B && channels[i].B == A)) {
			return &channels[i];
		}
	}

	return NULL;
}

int block_replica(struct replica_info *replica) {
	if (!replica->thread_id) BUG();
	printc("Blocking thread %d\n", replica->thread_id);
	replica->blocked = 1;
	return sched_block(cos_spd_id(), 0);
}

int nwrite(spdid_t spdid, replica_type to, int data) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *to_comp = (to == ping) ? &components[0] : (to == pong) ? & components[1] : NULL;
	struct channel *c = get_channel(component, to_comp);
	int ret;
	int i, j;
	if (!replica) BUG();
	if (!c) BUG();

	if (component == c->A) {
		if (c->have_A2B_data) {
			printc("This replica has already sent some data down the channel\n");
			return -1; // whatever, shouldn't happen if blocked
		}
		else {
			c->have_A2B_data = 1;
			c->A2B_data = data;
			rdtscll(c->start);							// set start time
			ret = block_replica(replica);
			if (ret < 0) printc("Error\n");
			/* On wakeup */	
			return 0;
		}
	}
	else if (component == c->B) {
		if (c->have_B2A_data) {
			printc("This replica has already sent some data down the channel\n");
			return -1; // whatever, shouldn't happen if blocked
		}
		else {
			c->have_B2A_data = 1;
			c->B2A_data = data;
			rdtscll(c->start);							// set start time
			ret = block_replica(replica);
			if (ret < 0) printc("Error\n");
			/* On wakeup */	
			return 0;
		}
	}
	else {
		BUG();
		return -1; // shut up compiler
	}
}

int nread(spdid_t spdid, replica_type from, int data) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *from_comp = (from == ping) ? &components[0] : (from == pong) ? & components[1] : NULL;
	struct channel *c = get_channel(component, from_comp);
	int ret;
	if (!replica) BUG();
	if (!c) BUG();

	if (component == c->A) {
		if (c->have_B2A_data) {
			c->have_B2A_data = 0;
			return c->B2A_data;
		}
		else {
			ret = block_replica(replica);
			if (ret < 0) printc("Error");
			/* On wakeup */	
			assert(c->have_B2A_data);
			c->have_B2A_data = 0; // well this is wrong because maybe other replicas still need to read
			return c->B2A_data;
		}
	}
	else if (component == c->B) {
		if (c->have_A2B_data) {
			c->have_A2B_data = 0;
			return c->A2B_data;
		}
		else {
			ret = block_replica(replica);
			if (ret < 0) printc("Error");
			/* On wakeup */	
			assert(c->have_A2B_data);
			c->have_A2B_data = 0;
			return c->A2B_data;
		}
	}
	else {
		BUG();
		return -1; /* Jus to shut up the compiler; control should never reach here anyway */
	}
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
			replicas[i].thread_id = cos_get_thd_id();

			map[spdid].type = type;
			map[spdid].replica = &replicas[i];
			map[spdid].component = &components[t];
			return 0;
		}
		else {
			printc("replica for spdid %d already exists\n", replicas[i].spdid);
		}
	}
	return 0;
}

void monitor(void) {
	struct nmod_comp *rcv_comp;
	struct replica_info *rcv_replica;
	int i, j;
	int found = 0;
	u64_t time;

	for (i = 0; i < N_CHANNELS; i++) {
		printc("Examing channel %d between nmodcomps with replicas %d and %d \n", i, channels[i].A->replicas[0].spdid, channels[i].B->replicas[0].spdid);

		printc("channel A2B_data present %d (%d)\n", channels[i].have_A2B_data, channels[i].A2B_data);
		printc("channel B2A_data present %d (%d)\n", channels[i].have_B2A_data, channels[i].B2A_data);

		if (channels[i].have_A2B_data) {
			rcv_comp = channels[i].B;
			found = 1;
		}
		if (channels[i].have_B2A_data) {
			rcv_comp = channels[i].A;
			found = 1;
		} // this will only wake up for the last found. Need both

		if (!found) continue;
		rdtscll(time);

		printc("time: %lu\n", time - channels[i].start);

		printc("Examining receiving comp with replicas %d\n", rcv_comp->nreplicas);

		for (j = 0; j < rcv_comp->nreplicas; j++) {
			rcv_replica = &rcv_comp->replicas[j];

			printc("Looking at replica %d\n", rcv_replica->spdid);

			if (!rcv_replica->thread_id) BUG();

			if (rcv_replica->blocked) {
				rcv_replica->blocked = 0;
				printc("waking up thread %d\n", rcv_replica->thread_id);
				sched_wakeup(cos_spd_id(), rcv_replica->thread_id);
			}
		}
	}
}

void cos_init(void)
{
	int i, j;
	for (i = 0; i < N_COMPS; i++) {
		printc("Setting up nmod component %d\n", i);
		components[i].nreplicas = 1; // for now, no fork
		components[i].replicas = (struct replica_info*) malloc(sizeof(struct replica_info) * components[i].nreplicas);
	
		for (j = 0; j < components[i].nreplicas; j++) {
			components[i].replicas[j].spdid = 0;
			components[i].replicas[j].blocked = 0;
		}
	}

	channels[0].A = &components[0];
	channels[0].B = &components[1];

	for (i = 0; i < N_MAP_SZ; i++) {
		map[i].type = none;
		map[i].replica = NULL;
		map[i].component = NULL;
	}

	periodic_wake_create(cos_spd_id(), period);
	do {
		periodic_wake_wait(cos_spd_id());
		monitor();
	} while (i++ < 2000);
}
