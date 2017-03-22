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

int nwrite(spdid_t spdid, replica_type to, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *to_comp = (to == ping) ? &components[0] : (to == pong) ? & components[1] : NULL;
	int ret;
	long i;
	if (!replica) BUG();

	if (replica->destination) {
		printc("This replica has already sent some data down the channel\n");
		return -1; // whatever, shouldn't happen if blocked
	}
	else {
		assert(replica->buf_write);
		assert(sz <= 1024);
		
		replica->destination = to_comp;
		replica->sz_write = sz;
		printc("resplica destination set to %x\n", replica->destination);
		ret = block_replica(replica);
		if (ret < 0) printc("Error\n");
		/* On wakeup */	
		return 0;
	}
}

size_t nread(spdid_t spdid, replica_type from, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct nmod_comp *from_comp = (from == ping) ? &components[0] : (from == pong) ? &components[1] : NULL;
	struct channel *c = get_channel(from_comp, component);
	int ret;
	printc("read spdid %d from %d component %x from_comp %x\n", spdid, from, component, from_comp);
	if (!replica) BUG();
	if (!c) BUG();

	if (c->have_data) {
		c->have_data = 0;
		printc("returning from read\n");
		return c->sz_data;
	}
	else {
		ret = block_replica(replica);
		if (ret < 0) printc("Error");
		/* On wakeup */	
		assert(c->have_data);
		printc("returning from read\n");
		c->have_data = 0; // well this is wrong because maybe other replicas still need to read
		return c->sz_data;
	}
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

	for (i = 0; i < N_CHANNELS; i++) {
		printc("Examing channel %d between nmodcomps with replicas %d and %d \n", i, channels[i].snd->replicas[0].spdid, channels[i].rcv->replicas[0].spdid);
		rdtscll(channels[i].start);

		found = 0;
		snd_comp = channels[i].snd;
		for (j = 0; j < snd_comp->nreplicas; j++) {
			if (snd_comp->replicas[j].destination) {
				printc("Found some data\n");
				found++;
			}
		}

		if (found != snd_comp->nreplicas) continue;
		rcv_comp = channels[i].rcv;

		for (j = 0; j < 9; j++) matches[j] = 0;
		majority = 0;

		/* now vote */
		for (j = 0; j < snd_comp->nreplicas; j++) {
			snd_comp->replicas[j].destination = NULL;
			matches[j]++;						// for our own match. Is this how voting works? Do we even know anymore...
			for (k = j + 1; k < snd_comp->nreplicas; k++) {
				/* compare j to k  */
				if (snd_comp->replicas[j].sz_write == snd_comp->replicas[k].sz_write && memcmp(snd_comp->replicas[j].buf_write, snd_comp->replicas[k].buf_write, snd_comp->replicas[j].sz_write) == 0) {
					matches[j]++;
					matches[k]++;
				}
			}
			if (matches[j] > majority) {
				majority = matches[j];
				majority_index = j;
			}
		}

		for (j = 0; j < 9; j++) printc("[%d] ", matches[j]);
		printc("\n");

		/* Now put the majority index's data into the channel */
		assert(channels[i].data_buf);
		assert(snd_comp->replicas[majority_index].sz_write <= 1024);
		channels[i].have_data = 1;
		channels[i].sz_data = snd_comp->replicas[majority_index].sz_write;
		printc("Majority index %d\n", majority_index);
		printc("starting copy to %x from %x\n", channels[i].data_buf, snd_comp->replicas[majority_index].buf_write);
		memcpy(channels[i].data_buf, snd_comp->replicas[majority_index].buf_write, snd_comp->replicas[majority_index].sz_write);
		printc("finished copy\n");

		// also unblock writes!

		for (j = 0; j < rcv_comp->nreplicas; j++) {
			rcv_replica = &rcv_comp->replicas[j];
			memcpy(rcv_replica->buf_read, channels[i].data_buf, channels[i].sz_data);

			printc("Looking at replica %d\n", rcv_replica->spdid);

			if (rcv_replica->blocked) {
				rcv_replica->blocked = 0;
				if (!rcv_replica->thread_id) BUG();
				printc("waking up thread %d\n", rcv_replica->thread_id);
				sched_wakeup(cos_spd_id(), rcv_replica->thread_id);
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

	periodic_wake_create(cos_spd_id(), period);
	do {
		periodic_wake_wait(cos_spd_id());
		monitor();
	} while (i++ < 2000);
}
