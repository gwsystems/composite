#include <cos_component.h>
#include <print.h>
#include <quarantine.h>
#include <cbuf.h>
#include <sched.h>
#include <voter.h>
#include <periodic_wake.h>

#undef DEBUG

#if defined(DEBUG)
#define printd(...) printc("Voter: "__VA_ARGS__)
#else
#define printd(...)
#endif

#define N_COMPS 2				/* number of nMR components - symbolizes an encapsulated redundant component 	*/
#define N_CHANNELS 2				/* number of channels between components. One-directional 			*/
#define N_MAP_SZ 250				/* max number of spid's in the system. Super arbitrary				*/
#define N_MAX 9					/* max number of components in an nMR component. Also arbitrary.		*/

struct replica_info {
	spdid_t spdid;
	unsigned short int thread_id;
	int blocked;
	void *destination;			/* NULL if this replica hasn't written */
	unsigned int epoch[N_CHANNELS];
	
	/* Buffers */
	cbuf_t read_buffer;
	cbuf_t write_buffer;
	void *buf_read;
	void *buf_write;
	size_t sz_read;
	size_t sz_write;
};

struct nmod_comp {
	struct replica_info replicas[N_MAX];
	int nreplicas;
};

struct channel {
	struct nmod_comp *snd, *rcv;
	int have_data;
	size_t sz_data;
	cbuf_t data_cbid;
	void *data_buf;
	unsigned int epoch;
};

struct map_entry {
	struct replica_info *replica;
	struct nmod_comp *component;
};

struct nmod_comp components[N_COMPS];
struct channel channels[N_CHANNELS];
struct map_entry map[N_MAP_SZ];
int matches[N_MAX];				/* To keep track of voting. Not in nmod_comp because it can be reused - we're only ever voting on one thing at a time */
static int N = 0;

void cos_fix_spdid_metadata(spdid_t o_spd, spdid_t f_spd) { }

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
	printd(" [channel %x (data %d) between\n", channel, channel->have_data);
	printd(" %x (", channel->snd);
	for (i = 0; i < channel->snd->nreplicas; i++) {
		printd("%x (%d) ", &channel->snd->replicas[i], channel->snd->replicas[i].spdid);
	}
	printd(")\n and\n %x (", channel->rcv);
	for (i = 0; i < channel->rcv->nreplicas; i++) {
		printd("%x (%d) ", &channel->rcv->replicas[i], channel->rcv->replicas[i].spdid);
	}
	printd(")\n");
}

/* Another helper function */
void get_replica_info(struct replica_info *replica) {
	printd(" [This is replica %x with spdid %d\n read buf %d (%x) write buf %d (%x)\n thd id %d, block status %d]\n",
		replica, replica->spdid, replica->read_buffer, replica->buf_read, replica->write_buffer, replica->buf_write,
		replica->thread_id, replica->blocked);	
}

struct channel *get_channel(struct nmod_comp *snd, struct nmod_comp *rcv) {
	int i;
	if (snd == NULL || rcv == NULL) return NULL;
	for (i = 0; i < N_CHANNELS; i++) {
		if (channels[i].snd == snd && channels[i].rcv == rcv) return &channels[i];
	}
	return NULL;
}

int wakeup_replica(struct replica_info * replica) {
	printc("Thread %d: Waking up replica with spdid %d, thread %d\n", cos_get_thd_id(), replica->spdid, replica->thread_id);
	assert(replica->thread_id);
	replica->blocked = 0;
	sched_wakeup(cos_spd_id(), replica->thread_id);
	return 0;
}

int block_replica(struct replica_info *replica) {
	int i, j;
	int n = 0;

	for (i = 0; i < N_COMPS; i++) {
		for (j = 0; j < components[i].nreplicas; j++) {
			if (components[i].replicas[j].blocked == 0 && components[i].replicas[j].spdid != replica->spdid) {
				n++;
			}
		}
	}
	printc("Thread %d: After this block, %d replicas will be awake\n", cos_get_thd_id(), n);

	/*
	 * Only put to sleep if someone will still be awake afterwards.
	 * Otherwise find some other replica and wake it
	 */
	if (n > 0) {
		if (!replica->thread_id) BUG();
		if (cos_get_thd_id() != replica->thread_id) BUG();
		printc("Thread %d: Blocking thread %d\n", cos_get_thd_id(), replica->thread_id);
		replica->blocked = 1;
		sched_block(cos_spd_id(), 0);
		return 0;
	}
	else {
		// caller should wake something up
		return -1;
	}
}

/*
 * Wake up some writer that has NOT written data already
 * Return 0 on success, -1 if no such writers were found
 * Intended to be called when we find that some reader got
 * woken up but it STILL has no data.
 */
int wakeup_writer(struct channel *c) {
	int i;
	struct nmod_comp *snd;
	struct replica_info *replica, *target;
	target = NULL;
	snd = c->snd;
	printc("Thread %d: Attempting to find a writer to wakeup\n", cos_get_thd_id());
	for (i = 0; i < snd->nreplicas; i++) {
		replica = &snd->replicas[i];
		printd("Inspecting writer with spdid %d\n", replica->spdid);
		if (replica->blocked && !replica->destination) {
			printd("Thread %d: Found a replica that is asleep and hasn't written yet\n", cos_get_thd_id());
			target = replica;
		}
	}

	if (target) return wakeup_replica(target);

	printd("Thread %d: Did not find any replica suitable for wakeup\n", cos_get_thd_id());
	return -1;
}

int wakeup_reader(struct channel *c, unsigned int target_epoch) {
	int i;
	struct nmod_comp *rcv;
	struct replica_info *replica, *target;
	target = NULL;
	rcv = c->rcv;
	printc("Thread %d: Attempting to find a reader to wakeup for epoch %d\n", cos_get_thd_id(), target_epoch);
	for (i = 0; i < rcv->nreplicas; i++) {
		replica = &rcv->replicas[i];
		printc("Inspecting reader with spdid %d, blocked %d, epoch %d\n", replica->spdid, replica->blocked);
		if (replica->blocked) { //  && replica->epoch <= target_epoch) {
			printc("Thread %d: Found a replica that is asleep and hasn't read yet\n", cos_get_thd_id());
			target = replica;
		}
	}
	
	if (target) return wakeup_replica(target);

	printc("Thread %d: Did not find any replica suitable for wakeup\n", cos_get_thd_id());
	return -1;
}

static inline int
check_writes(struct channel *c) {
	int found = 0; 
	int i;
	struct replica_info *replica;

	/* Check snd replicas. All need to have written. */
	found = 0;
	for (i = 0; i < c->snd->nreplicas; i++) {
		replica = &c->snd->replicas[i];
		if (!replica->spdid) continue;
		printc("(%d->%x)", replica->spdid, replica->destination);
		if (replica->destination) found++;
	}
	printc("\n");
	if (found != c->snd->nreplicas) return 0;

	printc("Thread %d: We have enough data to send\n", cos_get_thd_id());
	return found;
}

/* 
 * See if the channel in question is ready to pass data along.
 * Return 0 if it is and read/write calls CAN reach their return statements
 * Return anything else to block a thread
 *
 * It is assumed that this is the only place where threads can be woken up,
 * after the current call to inspect_channel finds data is ready to proceed.
 *
 * In the future, threads will probably also be woken up in the voter's checkup
 * on the system, if it determines that something has timed out or crashed.
 */
int inspect_channel(struct channel *c) {
	int i, k;
	int majority, majority_index;
	struct replica_info *replica;

	/* Check rcv replicas. If they're not ready, we can just error out */
	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		if (!replica->spdid) return -1; 	/* We can write but read replicas haven't been confirmed yet */
	}

	if (!check_writes(c)) return -1;

	// Can optimize so that voting is only done once... wait it might only be done once now. */
	/* Reset the matches array */	
	for (i = 0; i < N_MAX; i++) matches[i] = 0;
	majority = 0;

	/* now vote */
	for (i = 0; i < c->snd->nreplicas; i++) {
		//c->snd->replicas[i].destination = NULL; Oh god no
		matches[i]++;						// for our own match. Is this how voting works? Do we even know anymore...
		for (k = i + 1; k < c->snd->nreplicas; k++) {
			/* 
			 * compare j to k
			 * This SEEMS ridiculously messy and it is but all it really does is compare sz_write of j and k 
			 * and then the contents of the data if size matches
			 */
			if (c->snd->replicas[i].sz_write == c->snd->replicas[k].sz_write && 
			    memcmp(c->snd->replicas[i].buf_write, c->snd->replicas[k].buf_write, c->snd->replicas[i].sz_write) == 0) {
				matches[i]++;
				matches[k]++;
			}
		}
		if (matches[i] > majority) {
			majority = matches[i];
			majority_index = i;
		}
	}

	/* Now put the majority index's data into the channel */
	assert(c->data_buf);
	assert(c->snd->replicas[majority_index].sz_write <= 1024);
	c->have_data = c->rcv->nreplicas;
	c->sz_data = c->snd->replicas[majority_index].sz_write;
	memcpy(c->data_buf, c->snd->replicas[majority_index].buf_write, c->snd->replicas[majority_index].sz_write);

	/* Unblocking reads */
	printc("Thread %d: unblocking reads\n", cos_get_thd_id());
	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		memcpy(replica->buf_read, c->data_buf, c->sz_data);

		if (replica->blocked) wakeup_replica(replica);
		//printc("read wakeup done\n");	// this fixed it??? Ok...
	}
	
	/* Unblocking writes */
	printc("Thread %d: unblocking writes\n", cos_get_thd_id());
	for (i = 0; i < c->snd->nreplicas; i++) {
		replica = &c->snd->replicas[i];
		if (replica->blocked) wakeup_replica(replica); 
	}

	return 0;	
}

int nwrite(spdid_t spdid, channel_id to, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct channel *c;
	int ret;

	if (!replica) BUG();
	if (!replica->thread_id) replica->thread_id = cos_get_thd_id();				/* for a fork that couldn't be assigned a thread id yet */
	if (to >= N_CHANNELS) BUG();
	if (replica->destination) {
		printd("This replica has already sent some data down the channel\n");
		return -1; // debug this a lot more thoroughly
	}
	assert(replica->buf_write);
	assert(sz <= 1024);

	c = &channels[to];
	printc("Thread %d: Setting destination for replica with spdid %d to %x\n", cos_get_thd_id(), replica->spdid, c->rcv);
	replica->destination = c->rcv;
	replica->sz_write = sz;
	while (inspect_channel(c)) {
		printc("Thread %d: inspected channel with epoch %d\n", cos_get_thd_id(), replica->epoch[to]);
		int ret = block_replica(replica);
		if (ret) wakeup_writer(c);
		printc("Thread %d: before next iteration, channel epoch %d compared to our epoch %d\n", cos_get_thd_id(), c->epoch, replica->epoch[to]);
		if (c->epoch > replica->epoch[to]) break;
	}

	/* One wakeup */
	/* We may have enough data to send (inspect_channel passes) BUT not everyone has read yet */	
	while (c->have_data) {
		printc("Thread %d: not everyone has read yet, also our destination is %x, epoch %d to channel %d\n", cos_get_thd_id(), replica->destination, replica->epoch[to], c->epoch);
		if (N++ > 7) assert(0);
		int ret = block_replica(replica);
		if (ret) wakeup_reader(c, c->epoch);
	}
	
	printd("Thread %d: Incrementing write epoch\n", cos_get_thd_id());
	replica->epoch[to]++;
		
	replica->destination = NULL;
	return 0;
}

size_t nread(spdid_t spdid, channel_id from, size_t sz) {
	struct replica_info *replica = get_replica(spdid);
	struct nmod_comp *component = get_component(spdid);
	struct channel *c;
	int ret;

	if (from >= N_CHANNELS) BUG();
	if (!replica) BUG();
	if (!replica->thread_id) replica->thread_id = cos_get_thd_id();				/* for a fork that couldn't be assigned a thread id yet */

	c = &channels[from];
	ret = inspect_channel(c);
	if (ret) {
		block_replica(replica);
	}

	/* Unfortunately we may get woken up by someone other than a writer - true? */
	while (!c->have_data && wakeup_writer(c)) block_replica(replica);
	assert(c->have_data);
	c->have_data--;
	if (c->have_data == 0) c->epoch++;
	replica->epoch[from]++;
	printd("Thread %d: Returning from read with have_data now at %d\n", cos_get_thd_id(), c->have_data);
	return c->sz_data;	// race condiditon?
}

void restart_comp(struct replica_info *replica) {
	printd("About to fork but actually not implemented yet :(\n");
}

void monitor(void) {
}

void confirm_fork(spdid_t spdid) {
	int i;
	struct nmod_comp *component;
	struct replica_info *replicas;
	
	component  = map[spdid].component;
	if (!component) BUG();
	replicas = component->replicas;
	printd("Starting fork for component designed to have %d replicas\n", component->nreplicas);

	for (i = 0; i < component->nreplicas; i++) {
		if (replicas[i].spdid == 0) {
			printd("creating replica for spdid %d in slot %d\n", spdid, i);
			
			replicas[i].spdid = quarantine_fork(cos_spd_id(), spdid);
			if (!replicas[i].spdid) BUG();

			printd("Forking succeeded - now back to bookkeeping!\n");
			replicas[i].thread_id = 0;					/* This will get set the first time read/write are called */
			replicas[i].destination = NULL;
			replicas[i].buf_read = cbuf_alloc(1024, &replicas[i].read_buffer); 
			replicas[i].buf_write = cbuf_alloc(1024, &replicas[i].write_buffer);
			assert(replicas[i].buf_read);
			assert(replicas[i].buf_write);
			assert(replicas[i].read_buffer > 0);
			assert(replicas[i].write_buffer > 0);
			printd("created write buf %d, read buf %d\n", replicas[i].write_buffer, replicas[i].read_buffer);

			/* Send this information back to the replica */
			cbuf_send(replicas[i].read_buffer);
			cbuf_send(replicas[i].write_buffer);

			/* Set map entries for this replica */
			map[replicas[i].spdid].replica = &replicas[i];
			map[replicas[i].spdid].component = component;

			return;
		}
		else {
			printd("replica for spdid %d already exists\n", replicas[i].spdid);
		}
	}

	printd("For whatever reason, no forking was done\n");
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
			replicas[i].buf_read = cbuf_alloc(1024, &replicas[i].read_buffer); 
			replicas[i].buf_write = cbuf_alloc(1024, &replicas[i].write_buffer);
			assert(replicas[i].buf_read);
			assert(replicas[i].buf_write);
			assert(replicas[i].read_buffer > 0);
			assert(replicas[i].write_buffer > 0);
			cbuf_set_fork(replicas[i].read_buffer, 0);				/* Mildly hackish */
			cbuf_set_fork(replicas[i].write_buffer, 0);

			/* Send this information back to the replica */
			cbuf_send(replicas[i].read_buffer);
			cbuf_send(replicas[i].write_buffer);

			/* Set map entries for this replica */
			map[spdid].replica = &replicas[i];
			map[spdid].component = &components[t];
			
			return 0;
		}
		else {
			printd("replica for spdid %d already exists\n", replicas[i].spdid);
		}
	}

	/* We couldn't find a free slot - is intended to happen if n = 1*/
	return -1;
}

void cos_init(void)
{
	int i, j;
	for (i = 0; i < N_COMPS; i++) {
		printc("Setting up nmod component %d\n", i);
	
		/* This is an ugly if-statement, get rid of it */	
		if (i == 0) components[i].nreplicas = 2;
		else components[i].nreplicas = 1;
	
		for (j = 0; j < components[i].nreplicas; j++) {
			components[i].replicas[j].spdid = 0;
			components[i].replicas[j].blocked = 0;
			components[i].replicas[j].destination = NULL;
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
