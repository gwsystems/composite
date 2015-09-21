/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <torlib.h>

static cos_lock_t fs_lock;
#define LOCK() if (lock_take(&fs_lock)) BUG();
#define UNLOCK() if (lock_release(&fs_lock)) BUG();

#define META_TD         "td"
#define META_OFFSET     "offset"
#define META_FLAGS      "flags"
#define META_EVTID      "evtid"

/* Default torrent implementations */
__attribute__((weak)) int
treadp(spdid_t spdid, int sz, int *off, int *len)
{
	return -ENOTSUP;
}
__attribute__((weak)) int
twritep(spdid_t spdid, td_t td, int cbid, int sz)
{
	return -ENOTSUP;
}

COS_MAP_CREATE_STATIC(torrents);
struct torrent null_torrent, root_torrent;

int
trmeta(spdid_t spdid, td_t td, const char *key, unsigned int klen, char *retval, unsigned int max_rval_len)
{
	/* spdid is not used ? */

	struct torrent *t;

	LOCK();
	t = tor_lookup(td);
	if (!t) {UNLOCK(); return -1;} // we need to have a unified return point which include an UNLOCK()

	if (strlen(key) != klen) return -1;

	if (strncmp(key, META_TD, klen) == 0) {
		sprintf(retval, "%d", t->td);
	}
	else if (strncmp(key, META_OFFSET, klen) == 0) {
		sprintf(retval, "%ld", (long)t->offset);
	}
	else if (strncmp(key, META_FLAGS, klen) == 0) {
		sprintf(retval, "%d", t->flags);
	}
	else if (strncmp(key, META_EVTID, klen) == 0) {
		sprintf(retval, "%ld", t->evtid);
	}
	else {UNLOCK(); return -1;}

	UNLOCK();
	if (strlen(retval) > max_rval_len) return -1;

	return strlen(retval);
}

int
twmeta(spdid_t spdid, td_t td, const char *key, unsigned int klen, const char *val, unsigned int vlen)
{
	/* spdid is not used ? */

	struct torrent *t;

	LOCK();
	t = tor_lookup(td);
	if (!t) {UNLOCK(); return -1;}

	if (strlen(key) != klen) return -1;
	if (strlen(val) != vlen) return -1;

	if(strncmp(key, META_TD, klen) == 0) {
		t->td = atoi(val); // type of td need to be confirmed
	}
	else if(strncmp(key, META_OFFSET, klen) == 0) {
		t->offset = atoi(val);
	}
	else if(strncmp(key, META_FLAGS, klen) == 0) {
		t->flags = atoi(val); // type of flags need to be confirmed
	}
	else if(strncmp(key, META_EVTID, klen) == 0) {
		t->evtid = atol(val); // type need to be confirment
	}
	else { UNLOCK(); return -1;}

	UNLOCK();
	return 0;
}

int
tor_cons(struct torrent *t, void *data, int flags)
{
	td_t td;
	assert(t);

	td        = (td_t)cos_map_add(&torrents, t);
	if (td == -1) return -1;
	t->td     = td;
	t->data   = data;
	t->flags  = flags;
	t->offset = 0;
	t->evtid  = 0;

	return 0;
}

struct torrent *
tor_alloc(void *data, int flags)
{
	struct torrent *t;

	t = malloc(sizeof(struct torrent));
	if (!t) return NULL;
	if (tor_cons(t, data, flags)) {
		free(t);
		return NULL;
	}
	return t;
}

/* will not deallocate ->data */
void
tor_free(struct torrent *t)
{
	assert(t);
	if (cos_map_del(&torrents, t->td)) BUG();
	free(t);
}

void
torlib_init(void)
{
	cos_map_init_static(&torrents);
	/* save descriptors for the null and root spots */
	null_torrent.td = td_null;
	if (td_null != cos_map_add(&torrents, NULL)) BUG();
	root_torrent.td = td_root;
	if (td_root != cos_map_add(&torrents, &root_torrent)) BUG();
}

