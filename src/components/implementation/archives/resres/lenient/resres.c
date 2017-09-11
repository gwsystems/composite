#include <cos_component.h>
#include <resres.h>
#include <sched.h>

#include <cos_map.h>
#include <cos_synchronization.h>
#include <cos_alloc.h>

COS_MAP_CREATE_STATIC(res_descs);
cos_lock_t l;
#define LOCK() lock_take(&l)
#define UNLOCK() lock_release(&l)

/* resource reservation */
struct resres {
	res_t id;
	spdid_t owner;
	u16_t dest; 		/* spdid, threadid, ... */
	res_type_t type;
	res_hardness_t h;
	res_spec_t spec;
};

res_t resres_create(spdid_t owner, u16_t dest, res_type_t t, res_hardness_t h)
{
	long rid;
	int ret = -1;
	struct resres *rr;

	/* lockfree malloc -- no locking required */
	rr = malloc(sizeof(struct resres));
	if (!rr) return -1;
	*rr = (struct resres){.id = -1, .owner = owner, .dest = dest, 
			      .type = t, .h = h, .spec = NULL_RSPEC};

	LOCK();
	rid = cos_map_add(&res_descs, rr);
	if (rid < 0) goto done;
	rr->id = rid;
	ret = rid;
done:
	UNLOCK();
	return ret;
}

/* Lock should be taken when calling this */
static inline struct resres *
rr_lookup(res_t r, spdid_t c)
{
	struct resres *rr;
	rr = cos_map_lookup(&res_descs, r);
	if (!rr || rr->owner != c) return NULL;
	return rr;
}

int resres_bind(spdid_t c, res_t r, res_spec_t rs)
{
	int ret = -1;
	struct resres *rr;

	LOCK();
	rr = rr_lookup(r, c);
	if (!rr) goto done;

	switch(rr->type) {
	case RESRES_CPU:
		BUG_ON(sched_thread_params(cos_spd_id(), rr->dest, rs));
		break;
	default:
		goto done;
	}

	rr->spec.a = rs.a;
	rr->spec.w = rs.w;
	ret = 0;
	/* trigger the reservation's event if set */
done:
	UNLOCK();
	return ret;
}

/* TODO: add in the event creation, triggering, etc... */
res_spec_t resres_notif(spdid_t c, res_t r, res_spec_t ra)
{
	res_spec_t ret = NULL_RSPEC;
/* 	struct resres *rr; */

/* 	LOCK(); */
/* 	rr = rr_lookup(r, c); */
/* 	if (!rr) goto done; */

	
/* done: */
/* 	UNLOCK(); */
	return ret;
}

int resres_delete(spdid_t c, res_t r)
{
	int ret = 0;
	struct resres *rr;

	LOCK();
	rr = rr_lookup(r, c);
	if (!rr) { 
		ret = -1; 
		goto done;
	}

	cos_map_del(&res_descs, r);
	free(rr);
done:
	UNLOCK();
	return ret;
}

void cos_init(void)
{
	lock_static_init(&l);
	cos_map_init_static(&res_descs);
}

void hack(void) { sched_block(cos_spd_id(), 0); }
