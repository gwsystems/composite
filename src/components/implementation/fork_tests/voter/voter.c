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

/* Note: a state of READ or WRITE means we are DEFINITELY blocked. */
typedef enum {
	REPLICA_ST_UNINIT = 0,			/* beginning state, should move away from this quickly				*/
	REPLICA_ST_PROCESSING,			/* has RETURNED or not initiated a read or write yet				*/
	REPLICA_ST_READ,			/* has issued a read and has now been blocked					*/
	REPLICA_ST_WRITE			/* has issued a write and has now been blocked					*/ 
} replica_state_t;

struct replica {
	spdid_t spdid;				/* A value of 0 means this replica hasn't even been initialized yet 		*/
	unsigned short int thread_id;
	replica_state_t state;
	unsigned int epoch[N_CHANNELS];		/* Epoch per channel 								*/
	
	/* Buffers */
	cbuf_t read_buffer;			/* cbuf_t is an id, void * is an actual pointer to the memory			*/
	cbuf_t write_buffer;
	void *buf_read;
	void *buf_write;
	size_t sz_read;
	size_t sz_write;
};

struct nmodcomp {
	struct replica replicas[N_MAX];
	int nreplicas;
};

struct channel {
	struct nmodcomp *snd, *rcv;
	int have_data;
	size_t sz_data;
	cbuf_t data_cbid;
	void *data_buf;
	unsigned int epoch;
};

struct map_entry {
	struct replica *replica;
	struct nmodcomp *component;
};

struct nmodcomp components[N_COMPS];
struct channel channels[N_CHANNELS];
struct map_entry map[N_MAP_SZ];
int matches[N_MAX];				/* To keep track of voting. Not in nmodcomp because it can be reused - we're only ever voting on one thing at a time */

struct channel *
channel_get(channel_id cid) {
	assert(cid >= 0 && cid < N_CHANNELS);
	return &channels[cid];
}

struct nmodcomp *
component_get(spdid_t spdid) {
	return map[spdid].component;
}

struct replica *
replica_get(spdid_t spdid) {
	return map[spdid].replica;
}

static inline void 
replica_transition(struct replica *replica, replica_state_t transition) {
	if (transition == REPLICA_ST_READ || transition == REPLICA_ST_WRITE) assert(replica->state != transition);
	replica->state = transition;
}

int
replica_wakeup(struct replica *replica) {
	assert(replica->thread_id);
	if (replica->thread_id == cos_get_thd_id()) return 1;	/* We are already awake */
	printd("Thread %d: Waking up replica with spdid %d, thread %d\n", cos_get_thd_id(), replica->spdid, replica->thread_id);
	sched_wakeup(cos_spd_id(), replica->thread_id);
	return 0;
}

/* TODO: this entire function is kind of a mess */
int
replica_block(struct replica *replica, replica_state_t transition) {
	int i, j;
	int n = 0;

	for (i = 0; i < N_COMPS; i++) {
		for (j = 0; j < components[i].nreplicas; j++) {
			if ((components[i].replicas[j].state == REPLICA_ST_PROCESSING || components[i].replicas[j].state == REPLICA_ST_UNINIT ) && 
				components[i].replicas[j].spdid != replica->spdid) n++;
		}
	}

	/*
	 * Only put to sleep if someone will still be awake afterwards.
	 * Otherwise find some other replica and wake it
	 */
	if (n > 0) {
		if (!replica->thread_id) BUG();
		if (cos_get_thd_id() != replica->thread_id) BUG();
		replica_transition(replica, transition);
		sched_block(cos_spd_id(), 0);
		return 0;
	} else {
		return -1;
	}
}

/*
 * Wake up some writer that has NOT written data already
 * Return 0 on success, -1 if no such writers were found
 * Intended to be called when we find that some reader got
 * woken up but it STILL has no data.
 */
int
wakeup_writer(channel_id cid) {
	int i;
	struct nmodcomp *snd;
	struct replica *replica, *target;
	struct channel *c = channel_get(cid);
	target = NULL;
	snd = c->snd;
	for (i = 0; i < snd->nreplicas; i++) {
		replica = &snd->replicas[i];
		if (replica->epoch[cid] <= c->epoch) target = replica;
	}

	if (target) return replica_wakeup(target);
	return -1;
}

int 
wakeup_reader(struct channel *c, unsigned int target_epoch) {
	int i;
	struct nmodcomp *rcv;
	struct replica *replica, *target;
	target = NULL;
	rcv = c->rcv;
	for (i = 0; i < rcv->nreplicas; i++) {
		replica = &rcv->replicas[i];
		target = replica;
	}
	
	if (target) return replica_wakeup(target);
	return -1;
}

int
channel_get_awake(struct channel *c) {
	unsigned int awake = 0;
	struct replica *replica;
	int i;

	for (i = 0; i < c->snd->nreplicas; i++) {
		replica = &c->snd->replicas[i];
		if (replica->state == REPLICA_ST_PROCESSING || replica->state == REPLICA_ST_UNINIT) awake++;
	}
	
	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		if (replica->state == REPLICA_ST_PROCESSING || replica->state == REPLICA_ST_UNINIT) awake++;
	}

	return awake;
}

static inline int
comp_writes_complete(channel_id cid) {
	int found = 0; 
	int i;
	struct replica *replica;
	struct channel *c = channel_get(cid);

	/* Check snd replicas. All need to have written FOR THIS EPOCH. */
	for (i = 0; i < c->snd->nreplicas; i++) {
		replica = &c->snd->replicas[i];
		if (replica->epoch[cid] != c->epoch + 1) return 0;
		found++;
	}
	if (found != c->snd->nreplicas) return 0;
	
	printd("Thread %d: We have enough data to send\n", cos_get_thd_id());
	return found;
}

/* 
 * See if the channel in question is ready to pass data along.
 * Return 0 if it is, meaning all writes have passed data along
 * Return anything else if writes are not finished
 *
 * In the future, threads will probably also be woken up in the voter's checkup
 * on the system, if it determines that something has timed out or crashed.
 */
int
channel_inspect(channel_id cid) {
	int i, k;
	int majority, majority_index;
	struct replica *replica;
	struct channel *c = &channels[cid];

	if (!comp_writes_complete(cid)) return -1;

	/* Reset the matches array */	
	for (i = 0; i < N_MAX; i++) matches[i] = 0;
	majority = 0;
	majority_index = -1;

	/* now vote */
	for (i = 0; i < c->snd->nreplicas; i++) {
		matches[i]++;
		for (k = i + 1; k < c->snd->nreplicas; k++) {
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
	
	/* TODO: Possible optimization: skip this and just note the majority index */
	c->sz_data = c->snd->replicas[majority_index].sz_write;
	memcpy(c->data_buf, c->snd->replicas[majority_index].buf_write, c->snd->replicas[majority_index].sz_write);

	/* 
	 * Copy data into read replica buffers. 
	 * Since waking up a replica can set it running, let's do all of this at once and THEN start waking up readers
	 */
	for (i = 0; i < c->rcv->nreplicas; i++) {
		replica = &c->rcv->replicas[i];
		memcpy(replica->buf_read, c->data_buf, c->sz_data);
	}

	return 0;	
}

int
nwrite(spdid_t spdid, channel_id to, size_t sz) {
	struct replica *replica = replica_get(spdid);
	struct channel *c;

	if (!replica) BUG();
	if (!replica->thread_id) replica->thread_id = cos_get_thd_id();				/* for a fork that couldn't be assigned a thread id yet */
	if (replica->state != REPLICA_ST_PROCESSING) BUG();
	assert(replica->buf_write);
	assert(sz <= 1024);

	c = channel_get(to);
	replica->sz_write = sz;
	replica->epoch[to]++;
	while (channel_inspect(to)) {
		if (channel_get_awake(c) > 1) replica_block(replica, REPLICA_ST_WRITE);
		else wakeup_writer(to);
		
		if (c->epoch + 1 >= replica->epoch[to]) break;
	}

	/* We may have enough data to send (channel_inspect passes) BUT not everyone has read yet */	
	while (c->have_data) {
		if (channel_get_awake(c) > 1) replica_block(replica, REPLICA_ST_WRITE);
		else wakeup_reader(c, c->epoch);
	}
	
	replica_transition(replica, REPLICA_ST_PROCESSING);
	return c->sz_data;
}

size_t
nread(spdid_t spdid, channel_id from, size_t sz) {
	struct replica *replica = replica_get(spdid);
	struct channel *c;

	if (!replica) BUG();
	if (!replica->thread_id) replica->thread_id = cos_get_thd_id();				/* for a fork that couldn't be assigned a thread id yet */
	if (replica->state != REPLICA_ST_PROCESSING) BUG(); 

	c = channel_get(from);
	replica->epoch[from]++;

	if (!(c->epoch + 1 == replica->epoch[from] && c->have_data)) {
		while (!c->have_data) {
			if (channel_get_awake(c) > 1) replica_block(replica, REPLICA_ST_READ);
			else wakeup_writer(from);
		}

		assert(c->have_data);
	}
	c->have_data--;
	if (c->have_data == 0) c->epoch++;
	replica_transition(replica, REPLICA_ST_PROCESSING);
	return c->sz_data;
}

void
monitor(void) {
}

void
replica_init(struct replica *replica, spdid_t spdid, struct nmodcomp *comp) {
	replica->spdid = spdid;
	replica->thread_id = 0;					/* This will get set the first time read/write are called */
	replica_transition(replica, REPLICA_ST_PROCESSING);
	replica->buf_read = cbuf_alloc(1024, &replica->read_buffer); 
	replica->buf_write = cbuf_alloc(1024, &replica->write_buffer);
	assert(replica->buf_read);
	assert(replica->buf_write);
	assert(replica->read_buffer > 0);
	assert(replica->write_buffer > 0);

	/* Send this information back to the replica */
	cbuf_send(replica->read_buffer);
	cbuf_send(replica->write_buffer);

	/* Set map entries for this replica */
	map[replica->spdid].replica = replica;
	map[replica->spdid].component = comp;
}

void
confirm_fork(spdid_t spdid) {
	int i;
	struct nmodcomp *component;
	struct replica *replicas;
	spdid_t fork_spd;
	
	component  = map[spdid].component;
	if (!component) BUG();
	replicas = component->replicas;

	for (i = 0; i < component->nreplicas; i++) {
		if (replicas[i].state == REPLICA_ST_UNINIT) {
			fork_spd = quarantine_fork(cos_spd_id(), spdid);
			if (!fork_spd) BUG();
			replica_init(&replicas[i], fork_spd, component);
			
			return;		/* TODO: actually probably don't */
		}
	}
}

cbuf_t
get_read_buf(spdid_t spdid) {
	if (!map[spdid].replica) return 0;
	return map[spdid].replica->read_buffer;
}

cbuf_t
get_write_buf(spdid_t spdid) {
	if (!map[spdid].replica) return 0;
	return map[spdid].replica->write_buffer;
}

int
replica_confirm(spdid_t spdid) {
	int ret;
	int i;
	int t = (spdid == 14) ? 0 : 1;		/* TODO: Ok, this is hackish but we shouldn't let the replicas TELL US what component they belong in */
	struct replica *replicas = components[t].replicas;

	for (i = 0; i < components[t].nreplicas; i++) {
		if (replicas[i].state == REPLICA_ST_UNINIT) {
			printd("creating replica for spdid %d in slot %d\n", spdid, i);
			replica_init(&replicas[i], spdid, &components[t]);
			replicas[i].thread_id = cos_get_thd_id();				/* We can override this now */	
			cbuf_set_fork(replicas[i].read_buffer, 0);				/* Mildly hackish */
			cbuf_set_fork(replicas[i].write_buffer, 0);
			
			return 0;
		}
	}

	/* We couldn't find a free slot - is intended to happen if n = 1*/
	return -1;
}

/* Just set everything to NULL or most appropriate "just created" value */
void
replica_clear(struct replica *replica) {
	replica->spdid = 0;
	replica_transition(replica, REPLICA_ST_UNINIT);
}

void
cos_init(void)
{
	int i, j;
	for (i = 0; i < N_COMPS; i++) {
		printd("Setting up nmod component %d\n", i);
	
		/* TODO: This is an ugly if-statement, get rid of it */	
		if (i == 0) components[i].nreplicas = 2;
		else components[i].nreplicas = 1;
	
		for (j = 0; j < components[i].nreplicas; j++) {
			replica_clear(&components[i].replicas[j]);
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
