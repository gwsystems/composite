#include <cos_component.h>
#include <print.h>
#include <quarantine.h>
#include <cbuf.h>
#include <sched.h>
#include <voter.h>
#include <periodic_wake.h>

#define N_COMPS 2				// number of components
#define N_CHANNELS 2				// number of channels between components
#define N_MAP_SZ 250				// number of spid's in the system. Super arbitrary
#define N_MAX 9

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
	void *destination;			// NULL if this replica hasn't written
};

struct nmod_comp {
	struct replica_info replicas[N_MAX];	// let's call this max
	int nreplicas;
};

struct channel {
	struct nmod_comp *snd, *rcv;
	int have_data;
	size_t sz_data;		 		// or can just merge with have_data?
	cbuf_t data_cbid;
	void *data_buf;
	u64_t start;				// set each time the replica is woken up. Accurate?
};

struct map_entry {
	struct replica_info *replica;
	struct nmod_comp *component;
};

struct nmod_comp components[N_COMPS];
struct channel channels[N_CHANNELS];
struct map_entry map[N_MAP_SZ];
int matches[N_MAX];

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

/* Helper function */
void get_channel_info(struct channel *channel) {
	struct nmod_comp *snd, *rcv;
	int i;
	printc(" [channel %x (data %d) between\n", channel, channel->have_data);
	printc(" %x (", channel->snd);
	for (i = 0; i < channel->snd->nreplicas; i++) {
		printc("%x (%d) ", &channel->snd->replicas[i], channel->snd->replicas[i].spdid);
	}
	printc(")\n and\n %x (", channel->rcv);
	for (i = 0; i < channel->rcv->nreplicas; i++) {
		printc("%x (%d) ", &channel->rcv->replicas[i], channel->rcv->replicas[i].spdid);
	}
	printc(")\n");
}

/* Another helper function */
void get_replica_info(struct replica_info *replica) {
	printc(" [This is replica %x with spdid %d\n read buf %d (%x) write buf %d (%x)\n thd id %d, block status %d]\n",
		replica, replica->spdid, replica->read_buffer, replica->buf_read, replica->write_buffer, replica->buf_write,
		replica->thread_id, replica->blocked);	
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
	if (cos_get_thd_id() != replica->thread_id) BUG();
	printc("Blocking thread %d\n", replica->thread_id);
	replica->blocked = 1;
	return sched_block(cos_spd_id(), 0);
}

int inspect_channel(struct channel *c) {
	int i, k;
	int found;
	int majority, majority_index;
	struct replica_info *replica;

	/* Check rcv replicas. If they're not ready, we can just error out */
	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		if (!replica->spdid) return -1; 	/* We can write but read replicas haven't been confirmed yet */
	}

	// See if we can make a separate function fo r this
	if (!c->have_data) {
		/* Check snd replicas. All need to have written. */
		found = 0;
		for (i = 0; i < c->snd->nreplicas; i++) {
			replica = &c->snd->replicas[i];
			if (replica->destination) {
				found++;
			}
		}

		if (found != c->snd->nreplicas) {
			printc("Found only %d written replicas when we need %d to continue\n", found, c->snd->nreplicas);
			/* return success once we are unblocked */
			return -1;
		}
		printc("We have enough data to send\n");
		
		/* Reset the matches array */	
		for (i = 0; i < N_MAX; i++) matches[i] = 0;
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

		// just for debugging - delete
		for (i = 0; i < N_MAX; i++) printc("[%d] ", matches[i]);
		printc("\n");

		/* Now put the majority index's data into the channel */
		assert(c->data_buf);
		assert(c->snd->replicas[majority_index].sz_write <= 1024);
		c->have_data = c->snd->nreplicas;
		c->sz_data = c->snd->replicas[majority_index].sz_write;
		memcpy(c->data_buf, c->snd->replicas[majority_index].buf_write, c->snd->replicas[majority_index].sz_write);
	}

	/* Unblocking reads */
	printc("Thread %d unblocking reads\n", cos_get_thd_id());
	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		memcpy(replica->buf_read, c->data_buf, c->sz_data);

		if (replica->blocked) {
			replica->blocked = 0;
			if (!replica->thread_id) BUG();
			printc("thread %d waking up read thread %d\n", cos_get_thd_id(), replica->thread_id);
			sched_wakeup(cos_spd_id(), replica->thread_id);
		}
		else if (replica->thread_id != cos_get_thd_id()) {	/* The replica could also be this thread */
			printc("The replica for thread %d did not initiate a read, or at least did not get blocked. Probably an error in the future.\n", replica->spdid);
		}
	}
	
	/* Unblocking writes */
	printc("Thread %d unblocking writes\n", cos_get_thd_id());
	for (i = 0; i < c->snd->nreplicas; i++) {
		replica = &c->snd->replicas[i];
		if (replica->blocked) {
			replica->blocked = 0;
			if (!replica->thread_id) BUG();
			printc("waking up write thread %d\n", replica->thread_id);
			sched_wakeup(cos_spd_id(), replica->thread_id);
		}
		else {
			printc("The replica for spd %d is not blocked. It better be us!\n", replica->spdid);
			printc("But actually it might not be due to how the system is now running\n");
			//assert(replica->thread_id == cos_get_thd_id());
		}
	}

	return 0;	
}

int nwrite(spdid_t spdid, channel_id to, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct channel *c;
	int ret;
	long i;
	if (!replica) BUG();

	printc("Write called by spdid %d\n", spdid);
	if (!replica->thread_id) replica->thread_id = cos_get_thd_id();
	
	if (to >= N_CHANNELS) BUG();

	c = &channels[to];

	if (replica->destination) {
		printc("This replica has already sent some data down the channel\n");
		return -1; // whatever, shouldn't happen if blocked
	}
	
	assert(replica->buf_write);
	assert(sz <= 1024);
	replica->destination = c->rcv;
	replica->sz_write = sz;

	ret = inspect_channel(c);
	if (ret) block_replica(replica);

	/* One wakeup */	
	return 0;
}

size_t nread(spdid_t spdid, channel_id from, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct channel *c;
	int ret;

	if (from >= N_CHANNELS) BUG();
	if (!replica) BUG();

	c = &channels[from];
	ret = inspect_channel(c);
	if (ret) block_replica(replica);
	c->have_data--;
	return c->sz_data;
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

int confirm_thd_id(spdid_t spdid, int thd_id) {
	//if (!map[spdid].replica) BUG();
	//if (map[spdid].replica->thread_id) return 0;	// this is actually ok because it means a regular replica is calling BUT THEN HOW DO WE TELL IF THEY'RE NOT?!
	//map[spdid].replica->thread_id = thd_id;
	//printc("Set thread id for spdid %d\n", spdid);
	return 0;
}

void confirm_fork(spdid_t spdid) {
	int i;
	struct nmod_comp *component;
	struct replica_info *replicas;
	
	component  = map[spdid].component;
	if (!component) BUG();
	replicas = component->replicas;
	printc("Starting fork for component designed to have %d replicas\n", component->nreplicas);

	for (i = 0; i < component->nreplicas; i++) {
		if (replicas[i].spdid == 0) {
			printc("creating replica for spdid %d in slot %d\n", spdid, i);
			
			replicas[i].spdid = quarantine_fork(cos_spd_id(), spdid);
			if (!replicas[i].spdid) BUG();

			printc("Forking succeeded - now back to bookkeeping!\n");
			replicas[i].thread_id = 0;
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
			map[replicas[i].spdid].replica = &replicas[i];
			map[replicas[i].spdid].component = component;

			printc("Done with all things\n");
			return;
		}
		else {
			printc("replica for spdid %d already exists\n", replicas[i].spdid);
		}
	}

	printc("For whatever reason, no forking was done\n");
}

cbuf_t get_read_buf(spdid_t spdid) {
	if (!map[spdid].replica) return NULL;
	return map[spdid].replica->read_buffer;
}

cbuf_t get_write_buf(spdid_t spdid) {
	if (!map[spdid].replica) return NULL;
	return map[spdid].replica->write_buffer;
}

int confirm(spdid_t spdid) {
	printc("Confirming readiness of spdid %d\n", spdid);
	int ret;
	int i;
	int t = (spdid == 14) ? 0 : 1;	
	struct replica_info *replicas = components[t].replicas;
	vaddr_t dest;
	spdid_t comp2fork = 14;		// ping

	for (i = 0; i < components[t].nreplicas; i++) {
		if (replicas[i].spdid == 0) {
			printc("creating replica for spdid %d in slot %d\n", spdid, i);
			
			replicas[i].spdid = spdid;
			replicas[i].thread_id = cos_get_thd_id();
			replicas[i].destination = NULL;
			replicas[i].buf_read = cbuf_alloc(1024, &replicas[i].read_buffer); 
			replicas[i].buf_write = cbuf_alloc(1024, &replicas[i].write_buffer);
			assert(replicas[i].buf_read);
			assert(replicas[i].buf_write);
			assert(replicas[i].read_buffer > 0);
			assert(replicas[i].write_buffer > 0);
			cbuf_set_fork(replicas[i].read_buffer, 0);
			cbuf_set_fork(replicas[i].write_buffer, 0);
			printc("created write buf %d, read buf %d\n", replicas[i].write_buffer, replicas[i].read_buffer);

			/* Send this information back to the replica */
			cbuf_send(replicas[i].read_buffer);
			cbuf_send(replicas[i].write_buffer);

			/* Set map entries for this replica */
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
	
		/* This is an ugly if-statement, get rid of it */	
		if (i == 0) components[i].nreplicas = 2;
		else components[i].nreplicas = 1; // for now, no fork
	
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
		map[i].replica = NULL;
		map[i].component = NULL;
	}

	//periodic_wake_create(cos_spd_id(), period);
	//do {
	//	periodic_wake_wait(cos_spd_id());
		//monitor();
	//} while (i++ < 2000);
}
