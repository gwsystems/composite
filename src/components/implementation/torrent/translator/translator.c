#include <cos_component.h>
#include <torrent.h>
#include <torlib.h>
#include <cos_synchronization.h>
#include <evt.h>
#include <cos_alloc.h>
#include <valloc.h>
#include <sched.h>

cos_lock_t l;
#define LOCK() if (lock_take(&l)) BUG();
#define UNLOCK() if (lock_release(&l)) BUG();

#include <cringbuf.h>
struct channel_info {
	int exists, direction;
	/* if channel is read-only (direction = COS_TRANS_DIR_LTOC),
	   there is only one torrent, otherwise, NULL */
	struct torrent *t; 	
	struct cringbuf rb;
} channels[10];

td_t 
tsplit(spdid_t spdid, td_t td, char *param, 
       int len, tor_flags_t tflags, long evtid) 
{
	td_t ret = -1;
	struct torrent *t, *nt;
	int channel, direction;

	LOCK();
	if (tor_isnull(td)) ERR_THROW(-EINVAL, done);
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	if (len > 1) ERR_THROW(-EINVAL, done);

	channel = (int)(*param - '0');
	if (channel > 9 || channel < 0) ERR_THROW(-EINVAL, done);
	if (!channels[channel].exists)  ERR_THROW(-ENOENT, done);

	nt = tor_alloc(&channels[channel], tflags);
	if (!nt) ERR_THROW(-ENOMEM, done);
	ret = nt->td;

	direction = channels[channel].direction;
	if (direction == COS_TRANS_DIR_LTOC) {
		if (tflags != TOR_READ)  ERR_THROW(-EINVAL, free);
		if (channels[channel].t) ERR_THROW(-EBUSY, free);
	}
	if (direction == COS_TRANS_DIR_CTOL && tflags != TOR_WRITE) ERR_THROW(-EINVAL, free);
	if (direction == COS_TRANS_DIR_LTOC) {
		nt->evtid = evtid;
		channels[channel].t = nt;
	} else {
		nt->evtid = 0;
	}
done:
	UNLOCK();
	return ret;
free:
	tor_free(nt);
	goto done;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	struct torrent *t;
	int ret = 0;

	LOCK();
	if (!tor_is_usrdef(td)) ERR_THROW(-EINVAL, done);
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	/* currently only allow deletion */
	if (td_into != td_null) ERR_THROW(-EINVAL, done);
	assert(t->data);
	((struct channel_info *)t->data)->t = NULL;
	tor_free(t);
done:
	UNLOCK();
	return ret;
}

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;

	if (!tor_is_usrdef(td)) return;
	LOCK();
	t = tor_lookup(td);
	if (!t) goto done;
	assert(t->data);
	((struct channel_info *)t->data)->t = NULL;
	tor_free(t);
done:
	UNLOCK();
	return;
}

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1;
	struct channel_info *channel;
	struct torrent *t;
	char *buf;

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(!tor_is_usrdef(td) || t->data);
	if (!(t->flags & TOR_READ)) ERR_THROW(-EACCES, done);

	buf = cbuf2buf(cbid, sz);
	if (!buf) goto done;

	channel = (struct channel_info*)t->data;
	ret = cringbuf_consume(&channel->rb, buf, sz);
done:	
	UNLOCK();
	return ret;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1;
	struct channel_info *channel;
	struct torrent *t;
	char *buf;

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	channel = (struct channel_info*)t->data;
	ret = cringbuf_produce(&channels->rb, buf, sz);
	cos_trans_cntl(COS_TRANS_TRIGGER, 0, 0, 0);

	t->offset += ret;
done:	
	UNLOCK();
	return ret;
}

static int channel_init(int channel)
{
	char *addr, *start;
	unsigned long i, sz;
	int acap, srv_acap;
	int direction;

	direction = cos_trans_cntl(COS_TRANS_DIRECTION, channel, 0, 0);
	if (direction < 0) {
		channels[channel].exists = 0;
		return 0;
	}  
	channels[channel].exists = 1;
	channels[channel].direction = direction;

	sz = cos_trans_cntl(COS_TRANS_MAP_SZ, channel, 0, 0);
	assert(sz <= (4*1024*1024)); /* current 8MB max */
	start = valloc_alloc(cos_spd_id(), cos_spd_id(), sz/PAGE_SIZE);
	assert(start);
	for (i = 0, addr = start ; i < sz ; i += PAGE_SIZE, addr += PAGE_SIZE) {
		assert(!cos_trans_cntl(COS_TRANS_MAP, channel, (unsigned long)addr, i));
	}
	cringbuf_init(&channels[channel].rb, start, sz);

	if (direction == COS_TRANS_DIR_LTOC) {
		acap = cos_async_cap_cntl(COS_ACAP_CREATE, cos_spd_id(), cos_spd_id(), 
					  cos_get_thd_id() << 16 | cos_get_thd_id());
		assert(acap);
		/* cli acap not used. Linux thread will be triggering the
		 * acap. We set the cli acap owner to the current thread for
		 * access control only.*/
		srv_acap = acap & 0xFFFF;
		cos_trans_cntl(COS_TRANS_ACAP, channel, srv_acap, 0);

		while (1) {
			int ret;
			if (-1 == (ret = cos_areceive(srv_acap))) BUG();
			assert(channels[channel].t);
			evt_trigger(cos_spd_id(), channels[channel].t->evtid);
		}
	}

	return 0;
}

int cos_init(void)
{
	int i;

	lock_static_init(&l);
	torlib_init();
	for (i = 0 ; i < COS_TRANS_SERVICE_MAX ; i++) {
		channel_init(i);
	}

	return 0;
}
