/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <torrent.h>
#include <torlib.h>

#include <cbuf.h>
#include <print.h>
#include <cos_synchronization.h>
#include <evt.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <fs.h>
#include <tar.h>

static cos_lock_t fs_lock;
struct fsobj root;
#define LOCK() if (lock_take(&fs_lock)) BUG();
#define UNLOCK() if (lock_release(&fs_lock)) BUG();

td_t 
tsplit(spdid_t spdid, td_t td, char *param, 
       int len, tor_flags_t tflags, long evtid) 
{
	td_t ret = -1;
	struct torrent *t, *nt;
	struct fsobj *fso, *fsc, *parent; /* obj, child, and parent */
	char *subpath;

	if (tor_isnull(td)) return -EINVAL;
	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	fso = t->data;

	fsc = fsobj_path2obj(param, len, fso, &parent, &subpath);
	if (!fsc) return -ENOENT;

	fsobj_take(fsc);
	nt = tor_alloc(fsc, tflags);
	if (!nt) ERR_THROW(-ENOMEM, done);
	ret = nt->td;

	/* If we created the torrent, then trigger an event as we have data! */
	evt_trigger(cos_spd_id(), evtid);
done:
	UNLOCK();
	return ret;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	return -EINVAL;
}

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;

	if (!tor_is_usrdef(td)) return;

	LOCK();
	t = tor_lookup(td);
	if (!t) goto done;
	fsobj_release((struct fsobj *)t->data);
	tor_free(t);
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

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(!tor_is_usrdef(td) || t->data);
	if (!(t->flags & TOR_READ)) ERR_THROW(-EACCES, done);

	fso = t->data;
	assert(fso->size <= fso->allocated);
	assert(t->offset <= fso->size);
	if (!fso->size) ERR_THROW(0, done);

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	left = fso->size - t->offset;
	ret  = left > sz ? sz : left;

	assert(fso->data);
	memcpy(buf, fso->data + t->offset, ret);
	t->offset += ret;
	cbuf_free(cbid);
done:	
	UNLOCK();
	return ret;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	return -EINVAL;
}

static void
tar_parse_file(char *file, struct fsobj *root)
{
	struct tar_record *r;

	r = (struct tar_record*)file;
	while (r) {
		struct fsobj *o;
		r = tar_parse_record(r, &o, root);
	}
}

#include <initf.h>

int 
cos_init(void)
{
	char *tar_file;
	int tar_sz, tot = 0;

	lock_static_init(&fs_lock);
	torlib_init();

	fs_init_root(&root);
	root_torrent.data = &root;
	root.flags = TOR_READ | TOR_SPLIT;
	
	/* FIXME: should map in the tar file rather than copy it. */
	tar_sz = initf_size();
	tar_file = malloc(tar_sz);
	if (!tar_file) {
		printc("Tar file of size %d cannot be read\n", tar_sz);
		return -1;
	}
	while (tot < tar_sz) tot += initf_read(tot, &tar_file[tot], tar_sz-tot);
	tar_parse_file(tar_file, &root);

	return 0;
}
