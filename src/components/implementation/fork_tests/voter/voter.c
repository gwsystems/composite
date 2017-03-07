#include <cos_component.h>
#include <print.h>
#include <quarantine.h>
#include <cbuf.h>
#include <sched.h>
#include <voter.h>
#include <periodic_wake.h>

#define N_COMPS 2			// number of components
#define N_CHANNELS 2			// number of channels between components
#define N_MAP_SZ 250			// number of spid's in the system. Super arbitrary

struct replica_info {
	spdid_t spdid;
	cbuf_t read_buffer;
	cbuf_t write_buffer;
	void *buf_read;
	void *buf_write;
	unsigned short int thread_id;
	int blocked;
	u64_t start;			// set each time the replica is woken up. Accurate?
};

struct nmod_comp {
	struct replica_info *replicas;
	int nreplicas;
};

struct channel {
	struct nmod_comp *snd, *rcv;
	int data, have_data;	// make these arrays
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
	return map[spdid].component;
}

struct replica_info *get_replica(spdid_t spdid) {
	return map[spdid].replica;
}

struct channel *get_channel(struct nmod_comp *snd, struct nmod_comp *rcv) {
	int i;
	if (snd == NULL || rcv == NULL) return NULL;

	for (i = 0; i < N_CHANNELS; i++) {
		if (channels[i].snd == snd && channels[i].rcv == rcv) {
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
	printc("New write format\n");

	if (c->have_data) {
		printc("This replica has already sent some data down the channel\n");
		return -1; // whatever, shouldn't happen if blocked
	}
	else {
		c->have_data = 1;
		c->data = data;
		ret = block_replica(replica);
		if (ret < 0) printc("Error\n");
		/* On wakeup */	
		return 0;
	}
}

int nread(spdid_t spdid, replica_type from, int data) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *from_comp = (from == ping) ? &components[0] : (from == pong) ? & components[1] : NULL;
	struct channel *c = get_channel(from_comp, component);
	int ret;
	if (!replica) BUG();
	if (!c) BUG();

	if (c->have_data) {
		c->have_data = 0;
		return c->data;
	}
	else {
		ret = block_replica(replica);
		if (ret < 0) printc("Error");
		/* On wakeup */	
		assert(c->have_data);
		c->have_data = 0; // well this is wrong because maybe other replicas still need to read
		return c->data;
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

void restart_comp(struct replica_info *replica) {
	printc("About to fork but actually not implemented yet :(\n");
}

void monitor(void) {
	struct nmod_comp *rcv_comp, *component;
	struct replica_info *rcv_replica;
	int i, j;
	int found = 0;
	u64_t time;

	/* Let's do waking up and restarting in a separate loop */
	for (i = 0; i < N_COMPS; i++) {
		component = &components[i];

		for (j = 0; j < component->nreplicas; j++) {
			rdtscll(time);
			printc("time: %lu\n", time - component->replicas[j].start);
			if (time > 500) {
				restart_comp(&component->replicas[i]);
			}
		}
	}
	
	for (i = 0; i < N_CHANNELS; i++) {
		printc("Examing channel %d between nmodcomps with replicas %d and %d \n", i, channels[i].snd->replicas[0].spdid, channels[i].rcv->replicas[0].spdid);

		printc("channel data present %d (%d)\n", channels[i].have_data, channels[i].data);

		if (channels[i].have_data) {
			rcv_comp = channels[i].rcv;
			found = 1;
		}

		if (!found) continue;

		printc("Examining receiving comp with replicas %d\n", rcv_comp->nreplicas);

		for (j = 0; j < rcv_comp->nreplicas; j++) {
			rcv_replica = &rcv_comp->replicas[j];

			printc("Looking at replica %d\n", rcv_replica->spdid);

			if (rcv_replica->blocked) {
				rdtscll(rcv_replica->start);
				rcv_replica->blocked = 0;
				if (!rcv_replica->thread_id) BUG();
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

	channels[0].snd = &components[0];
	channels[0].rcv = &components[1];
	channels[1].snd = &components[1];
	channels[1].rcv = &components[0];

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
