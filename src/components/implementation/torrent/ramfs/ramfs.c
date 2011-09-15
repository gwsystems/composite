/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <torrent.h>
#include <cos_component.h>

#include <cbuf.h>
#include <print.h>
#include <cos_synchronization.h>
#include <evt.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <fs.h>

COS_MAP_CREATE_STATIC(torrents);
cos_lock_t l;
struct fsobj root;
#define LOCK() if (lock_take(&l)) BUG();
#define UNLOCK() if (lock_release(&l)) BUG();

#define ERR_HAND(errval, label) do { ret = errval; goto label; } while (0)

struct torrent {
	td_t tid;
	u32_t offset;
	struct fsobj *fso;
};
struct torrent null_torrent, root_torrent;

td_t 
tsplit(spdid_t spdid, td_t td, char *param, 
       int len, tor_flags_t tflags, long evtid) 
{
	td_t ret = -1;
	struct torrent *t, *nt;
	struct fsobj *fso, *fsc, *parent; /* obj and child */
	char *p, *subpath;

	if (td == td_null) return -EINVAL;
	LOCK();
	t = cos_map_lookup(&torrents, td);
	if (!t) ERR_HAND(-EINVAL, done);
	assert(t->tid == td);
	assert(t->fso);
	fso = t->fso;

	p = malloc(len+1);
	if (!p) ERR_HAND(-ENOMEM, done);
	strncpy(p, param, len);
	p[len] = '\0';

	fsc = fsobj_path2obj(p, fso, &parent, &subpath);
	if (!fsc) {
		assert(parent);
		printc("parent %s, creating %s\n", parent->name, subpath);
		fsc = fsobj_alloc(subpath, parent);
		if (!fsc) ERR_HAND(-EINVAL, free1);
	}

	fsobj_take(fsc);
	nt = malloc(sizeof(struct torrent));
	if (!nt) ERR_HAND(-ENOMEM, free1);

	ret = (td_t)cos_map_add(&torrents, nt);
	if (ret == -1) goto free2;

	nt->tid    = ret;
	nt->fso    = fsc;
	nt->offset = 0;
	
	printc("t %d is fso %p for %s, size %d\n", ret, fso, fso->name, fso->size);
done:
	UNLOCK();
	return ret;
free2:  
	free(nt);
free1:  free(p);
	goto done;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	struct torrent *t, *t_into;
	int ret = -1;

	if (td == td_null || td == td_root) return -1;

	LOCK();
	t = cos_map_lookup(&torrents, td);
	if (!t) ERR_HAND(-EINVAL, done);
	t_into = cos_map_lookup(&torrents, td_into);
	if (td_into != td_null && !t_into) ERR_HAND(-EINVAL, done);

	if (cos_map_del(&torrents, t->tid)) BUG();
	free(t);

	ret = 0;
done:   UNLOCK();
	return ret;
}

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;

	if (td == td_null || td == td_root) return;

	LOCK();
	t = cos_map_lookup(&torrents, td);
	if (!t) goto done;
	cos_map_del(&torrents, td);
	fsobj_release(t->fso);
	free(t);
done:
	UNLOCK();
	return;
}

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1, left;
	struct torrent *t;
	struct fsobj *fso;
	char *buf;

	LOCK();
	t = cos_map_lookup(&torrents, td);
	if (!t) goto done;
	assert(t->tid == td);
	assert(t->tid <= td_root || t->fso);
	fso = t->fso;
	assert(fso->size <= fso->allocated);
	printc("torrent %d (%p), offset %d, size %d\n", td, t, t->offset, fso->size);
	assert(t->offset <= fso->size);
	if (!fso->size) ERR_HAND(0, done);

	buf = cbuf2buf(cbid, sz);
	if (!buf) goto done;

	left = fso->size - t->offset;
	ret  = left > sz ? sz : left;

	assert(fso->data);
	memcpy(buf, fso->data + t->offset, ret);
	t->offset += ret;
done:	
	UNLOCK();
	return ret;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1, left;
	struct torrent *t;
	struct fsobj *fso;
	char *buf;

	LOCK();
	t = cos_map_lookup(&torrents, td);
	if (!t) ERR_HAND(-EINVAL, done);
	assert(t->tid == td);
	assert(t->fso);
	fso = t->fso;
	assert(fso->size <= fso->allocated);
	printc("torrent %d (%p), offset %d, size %d\n", td, t, t->offset, fso->size);
	assert(t->offset <= fso->size);

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_HAND(-EINVAL, done);

	left = fso->allocated - t->offset;
	if (left >= sz) {
		ret = sz;
		if (fso->size < (t->offset + sz)) fso->size = t->offset + sz;
	} else {
		char *new;
		int new_sz;

		new_sz = fso->allocated == 0 ? 256 : fso->allocated * 2;
		new    = malloc(new_sz);
		if (!new) ERR_HAND(-ENOMEM, done);
		if (fso->data) {
			memcpy(new, fso->data, fso->size);
			free(fso->data);
		}

		fso->data      = new;
		fso->allocated = new_sz;
		left           = new_sz - t->offset;
		ret            = left > sz ? sz : left;
		fso->size      = t->offset + ret;
	}
	memcpy(fso->data + t->offset, buf, ret);
	t->offset += ret;
	printc("torrent %d (%p), offset %d, size %d\n", td, t, t->offset, fso->size);
done:	
	UNLOCK();
	return ret;
}

int cos_init(void)
{
	lock_static_init(&l);
	cos_map_init_static(&torrents);
	fs_init_root(&root);
	/* save descriptors for the null and root spots */
	null_torrent.tid = td_null;
	if (td_null != cos_map_add(&torrents, &null_torrent)) BUG();
	root_torrent.tid = td_root;
	root_torrent.fso = &root;
	if (td_root != cos_map_add(&torrents, &root_torrent)) BUG();
	return 0;
}
