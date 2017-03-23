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
	size_t sz_read;
	size_t sz_write;
	unsigned short int thread_id;
	int blocked;
	void *destination;		// NULL if this replica hasn't written
};

struct nmod_comp {
	struct replica_info replicas[9];// let's call this max
	int nreplicas;
};

struct channel {
	struct nmod_comp *snd, *rcv;
	int have_data;
	size_t sz_data; 		// or can just merge with have_data?
	cbuf_t data_cbid;
	void *data_buf;
	u64_t start;			// set each time the replica is woken up. Accurate?
};

struct map_entry {
	replica_type type;
	struct replica_info *replica;
	struct nmod_comp *component;
};

struct nmod_comp components[N_COMPS];
struct channel channels[N_CHANNELS];
struct map_entry map[N_MAP_SZ];
int matches[9];

int period = 100;

void cos_fix_spdid_metadata(spdid_t o_spd, spdid_t f_spd) { }

int voter_setup(void) {
	return components[0].replicas[0].spdid != 0 && components[1].replicas[0].spdid != 0;
}

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
		printc("%x and %x compared to %x and %x\n", channels[i].snd, channels[i].rcv, snd, rcv);
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

int inspect_channel(struct channel *c) {
	printc("in inspect\n");
	int i, k;
	int found;
	int majority, majority_index;
	struct replica_info *replica;

	/* Check receive replicas. Currently doing nothing with this
	 * but later on, need all n to have initiated a read
	 */ 
	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		printc("inspecting rcv replica with spid %d\n", replica->spdid);
	}
	
	/*
	 * Check snd replicas. All need to have written.
	 */
	found = 0;
	for (i = 0; i < c->snd->nreplicas; i++) {
		replica = &c->snd->replicas[i];
		printc("inspecting rcv replica with spid %d\n", replica->spdid);

		if (replica->destination) {
			printc("Found some data\n");
			found++;
		}
	}
	
	if (found != c->snd->nreplicas) {
		printc("Found only %d written replicas when we need %d to continue\n", found, c->snd->nreplicas);
		sched_block(cos_spd_id(), 0);
		/* return success once we are unblocked */
		return 0;
	}
	printc("We have enough data to send\n");
		
	for (i = 0; i < 9; i++) matches[i] = 0;
	majority = 0;

	/* now vote */
	for (i = 0; i < c->snd->nreplicas; i++) {
		c->snd->replicas[i].destination = NULL;
		matches[i]++;						// for our own match. Is this how voting works? Do we even know anymore...
		for (k = i + 1; k < c->snd->nreplicas; k++) {
			/* compare j to k  */
			if (c->snd->replicas[i].sz_write == c->snd->replicas[k].sz_write && memcmp(c->snd->replicas[i].buf_write, c->snd->replicas[k].buf_write, c->snd->replicas[i].sz_write) == 0) {
				matches[i]++;
				matches[k]++;
			}
		}
		if (matches[i] > majority) {
			majority = matches[i];
			majority_index = i;
		}
	}

	for (i = 0; i < 9; i++) printc("[%d] ", matches[i]);
	printc("\n");

	/* Now put the majority index's data into the channel */
	assert(c->data_buf);
	assert(c->snd->replicas[majority_index].sz_write <= 1024);
	c->have_data = 1;
	c->sz_data = c->snd->replicas[majority_index].sz_write;
	printc("Majority index %d\n", majority_index);
	printc("starting copy to %x from %x\n", c->data_buf, c->snd->replicas[majority_index].buf_write);
	memcpy(c->data_buf, c->snd->replicas[majority_index].buf_write, c->snd->replicas[majority_index].sz_write);
	printc("finished copy\n");

	// also unblock writes!

	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		memcpy(replica->buf_read, c->data_buf, c->sz_data);

		printc("Looking at replica %d\n", replica->spdid);

		if (replica->blocked) {
			replica->blocked = 0;
			if (!replica->thread_id) BUG();
			printc("waking up thread %d\n", replica->thread_id);
			sched_wakeup(cos_spd_id(), replica->thread_id);
		}
	}

	return 0;	
}

int nwrite(spdid_t spdid, replica_type to, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *to_comp = (to == pong) ? &components[0] : (to == ping) ? & components[1] : NULL;
	struct channel *c = get_channel(component, to_comp);
	int ret;
	long i;
	if (!replica) BUG();
	
	if (!c) {
		block_replica(replica);
	}

	if (replica->destination) {
		printc("This replica has already sent some data down the channel\n");
		return -1; // whatever, shouldn't happen if blocked
	}
	else {
		assert(replica->buf_write);
		assert(sz <= 1024);
		replica->destination = to_comp;
		replica->sz_write = sz;
	}

	ret = inspect_channel(c);
	return ret;

	//if (replica->destination) {
	//	printc("This replica has already sent some data down the channel\n");
	//	return -1; // whatever, shouldn't happen if blocked
	//}
	//else {
	//	assert(replica->buf_write);
	//	assert(sz <= 1024);
		
	//	replica->destination = to_comp;
	//	replica->sz_write = sz;
	//	printc("resplica destination set to %x\n", replica->destination);
	//	ret = block_replica(replica);
	//	if (ret < 0) printc("Error\n");
		/* On wakeup */	
	//	return 0;
	//}
}

size_t nread(spdid_t spdid, replica_type from, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *from_comp = (from == ping) ? &components[1] : (from == pong) ? &components[0] : NULL;
	struct channel *c;
	int ret;

	printc("comp %x reading from %x\n", component, from_comp);

	c = get_channel(from_comp, component);

	if (!replica) BUG();
	if (!c) {
		printc("Blocking read because no channel\n");
		block_replica(replica);
	}

	printc("read spdid %d from %d component %x from_comp %x\n", spdid, from, component, from_comp);
	ret = inspect_channel(c);
	return c->sz_data;

	//if (!c) BUG();

	//if (c->have_data) {
	//	c->have_data = 0;
	//	printc("returning from read\n");
	//	return c->sz_data;
	//}
	//else {
	//	ret = block_replica(replica);
	//	if (ret < 0) printc("Error");
		/* On wakeup */	
	//	assert(c->have_data);
	//	printc("returning from read\n");
	//	c->have_data = 0; // well this is wrong because maybe other replicas still need to read
	//	return c->sz_data;
	//}
}

void restart_comp(struct replica_info *replica) {
	printc("About to fork but actually not implemented yet :(\n");
}

void monitor(void) {
	struct nmod_comp *rcv_comp, *snd_comp;
	struct channel *channel;
	struct replica_info *rcv_replica;
	int i, j, k;
	int found = 0;
	u64_t time;
	int majority_index;
	int majority;

	/* Let's do waking up and restarting in a separate loop */
	for (i = 0; i < N_CHANNELS; i++) {
		channel = &channels[i];

		for (j = 0; j < channel->rcv->nreplicas; j++) {
			rdtscll(time);
			printc("time: %lu\n", time - channel->start);
			if (time > 500) {					// this is waaaaaay too small of a timeout. How many cycles are 10 seconds???
				restart_comp(&channel->rcv->replicas[i]);
			}
		}
	}
}

cbuf_t get_read_buf(spdid_t spdid) {
	if (!map[spdid].replica) return NULL;
	return map[spdid].replica->read_buffer;
}

cbuf_t get_write_buf(spdid_t spdid) {
	if (!map[spdid].replica) return NULL;
	return map[spdid].replica->write_buffer;
}

int confirm(spdid_t spdid, replica_type type) {
	printc("Confirming readiness of spdid %d\n", spdid);
	int ret;
	int i;
	int t = (type == ping) ? 0 : 1;	
	struct replica_info *replicas = components[t].replicas;
	vaddr_t dest;

	for (i = 0; i < components[t].nreplicas; i++) {
		if (replicas[i].spdid == 0) {
			printc("creating replica for spdid %d in slot %d\n", spdid, i);
			replicas[i].spdid = spdid;
			//replicas[i].spdid = quarantine_fork(cos_spd_id(), comp2fork);
			//if (replicas[i].spdid == 0) printc("Error: f1 fork failed\n");
			replicas[i].thread_id = cos_get_thd_id();
			replicas[i].destination = NULL;
			replicas[i].buf_read = cbuf_alloc(1024, &replicas[i].read_buffer); 
			replicas[i].buf_write = cbuf_alloc(1024, &replicas[i].write_buffer);
			assert(replicas[i].buf_read);
			assert(replicas[i].buf_write);
			assert(replicas[i].read_buffer > 0);
			assert(replicas[i].write_buffer > 0);
			printc("created write buf %d, read buf %d\n", replicas[i].write_buffer, replicas[i].read_buffer);

			/* Send this information back to the replica */
			cbuf_send(replicas[i].read_buffer);
			cbuf_send(replicas[i].write_buffer);

			/* Set map entries for this replica */
			map[spdid].type = type;
			map[spdid].replica = &replicas[i];
			map[spdid].component = &components[t];
			
			return 0;
		}
		else {
			printc("replica for spdid %d already exists\n", replicas[i].spdid);
		}
	}

	/* We couldn't find a free slot */
	return -1;
}

void cos_init(void)
{
	int i, j;
	for (i = 0; i < N_COMPS; i++) {
		printc("Setting up nmod component %d\n", i);
		components[i].nreplicas = 1; // for now, no fork
	
		for (j = 0; j < components[i].nreplicas; j++) {
			components[i].replicas[j].spdid = 0;
			components[i].replicas[j].blocked = 0;
		}
	}

	channels[0].snd = &components[0];
	channels[0].rcv = &components[1];
	channels[1].snd = &components[1];
	channels[1].rcv = &components[0];
	channels[0].data_buf = cbuf_alloc(1024, &channels[0].data_cbid); 
	assert(channels[0].data_buf);
	channels[1].data_buf = cbuf_alloc(1024, &channels[1].data_cbid); 
	assert(channels[1].data_buf);

	for (i = 0; i < N_MAP_SZ; i++) {
		map[i].type = none;
		map[i].replica = NULL;
		map[i].component = NULL;
	}

	printc("Setup done channel 0 snd (%x) rcv (%x), 1 snd (%x) rcv (%x)\n", channels[0].snd, channels[0].rcv, channels[1].snd, channels[1].rcv);

	//periodic_wake_create(cos_spd_id(), period);
	//do {
	//	periodic_wake_wait(cos_spd_id());
		//monitor();
	//} while (i++ < 2000);
}
