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
#include <printc.h>
#include <cos_synchronization.h>
#include <evt.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <fs.h>

static cos_lock_t fs_lock;
struct fsobj root;
#define LOCK() if (lock_take(&fs_lock)) BUG();
#define UNLOCK() if (lock_release(&fs_lock)) BUG();

#define MIN_DATA_SZ 256

typedef struct {
	cbuf_t cbuf_id;
	u32_t cbuf_len;
	u32_t start;
	u32_t len;
	void* next;
} __file_data_t;

td_t 
tsplit(spdid_t spdid, td_t td, char *param, 
       int len, tor_flags_t tflags, long evtid) 
{
	td_t ret = -1;
	struct torrent *t, *nt;
	struct fsobj *fso, *fsc, *parent; /* obj, child, and parent */
	char *subpath;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	fso = t->data;

	fsc = fsobj_path2obj(param, len, fso, &parent, &subpath);
	if (!fsc) {
		assert(parent);
		if (!(parent->flags & TOR_SPLIT)) ERR_THROW(-EACCES, done);
		fsc = fsobj_alloc(subpath, parent);
		if (!fsc) ERR_THROW(-EINVAL, done);
		fsc->flags = tflags;
	} else {
		/* File has less permissions than asked for? */
		if ((~fsc->flags) & tflags) ERR_THROW(-EACCES, done);
	}

	fsobj_take(fsc);
	nt = tor_alloc(fsc, tflags);
	if (!nt) ERR_THROW(-ENOMEM, free);
	ret = nt->td;
done:
	UNLOCK();
	return ret;
free:  
	fsobj_release(fsc);
	goto done;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	struct torrent *t;
	int ret = 0;

	if (!tor_is_usrdef(td)) return -1;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	/* currently only allow deletion */
	if (td_into != td_null) ERR_THROW(-EINVAL, done);

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
done:	
	UNLOCK();
	return ret;
}

int
treadp(spdid_t spdid, td_t td, int *off, int *sz)
{
	printc("starting treadp\n");
	int ret = -1;
	struct torrent *t;
	struct fsobj *fso;
	__file_data_t *current;
	
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

	current = (__file_data_t*) fso->data;
	u32_t total_offset = 0;

	// trying to read a file with no data
	if (!current) { ERR_THROW(-EINVAL, done); }

	// find where we are reading from
	while (total_offset < t->offset && current != NULL)
	{
		total_offset += current->len;
		current = current->next;
	}

	// current should now be equal to the file offset.
	*off = current->start;
	*sz = current->len;
	ret = current->cbuf_id;

	t->offset += current->len;

	cbuf_send_free(current->cbuf_id);

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

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	fso = t->data;
	assert(fso->size <= fso->allocated);
	assert(t->offset <= fso->size);

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	left = fso->allocated - t->offset;
	if (left >= sz) {
		ret = sz;
		if (fso->size < (t->offset + sz)) fso->size = t->offset + sz;
	} else {
		char *new;
		int new_sz;

		new_sz = fso->allocated == 0 ? MIN_DATA_SZ : fso->allocated * 2;
		new    = malloc(new_sz);
		if (!new) ERR_THROW(-ENOMEM, done);
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
done:	
	UNLOCK();
	return ret;
}

int 
twritep(spdid_t spdid, td_t td, int cb, int start, unsigned int sz)
{
	printc(">>entering twritep\n");
	int ret = -1, left;
	struct torrent *t;
	struct fsobj *fso;
	__file_data_t *file_data; 

	printc(">>>starting basic checks\n");
	if (!sz) return -EINVAL;

	if (tor_isnull(td)) return -EINVAL;

	LOCK();

	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	fso = t->data;
	assert(fso->size <= fso->allocated);
	assert(t->offset <= fso->size);
	__file_data_t *current = (__file_data_t*) fso->data; // start of file
	__file_data_t *prev = NULL;

	// find where we want to write.
	printc(">>>figuring out where we want to write\n");
	u32_t total_offset = 0;
	while (total_offset < t->offset && current != NULL)
	{
		printc(">>>>still thinking about it\n");
		total_offset += current->len;
		prev = current;
		current = current->next;
		// crap. Current will alwyas be NULL
	}

	// need to calculate proper start

	if (current == NULL)
	{
		// initial file creation

		printc(">>>case 0\n");
		// adding to start and can't override
		__file_data_t *new_node = malloc(sizeof(__file_data_t));
		new_node->cbuf_id = cb;
		new_node->cbuf_len = sz; // it reeally doesn't.
		new_node->start = start;
		new_node->len = sz;
		new_node->next = NULL;

		fso->data = new_node; // change start of file

		// set offset
		t->offset += sz;
		fso->size = t->offset;

		printc(">>>exiting case 0\n");
	}
	else if (t->offset == 0 && sz < current->len)
	{
		printc(">>>case 1\n");
		// adding to start and can't override
		__file_data_t *new_node = malloc(sizeof(__file_data_t));
		new_node->cbuf_id = cb;
		new_node->cbuf_len = sz; // it reeally doesn't.
		new_node->start = start;
		new_node->len = sz;
		new_node->next = current->next;

		// need to keep some of current
		current->len -= sz;

		fso->data = new_node; // change start of file

		// set offset
		t->offset += sz;
		fso->size = t->offset;
	}
	else if (t->offset == 0 && sz >= current->len)
	{
		printc(">>>case 2\n");
		// adding to start and overriding the current node.
		// may overflow into next nodes

	}
	else if (t->offset > current->start && sz <= current->len)
	{
		printc(">>>case 3\n");
		// this will require a memcpy at some stage
	}
	else if (t->offset > current->start && sz > current->len)
	{
		printc(">>>case 4\n");
		// will have to adjust next node
	}
	else if (current->next == NULL && t->offset == current->start + current->len)
	{
		printc(">>>case 5\n");
		// arguably the most likely case
		// just writing to the end of the file
		__file_data_t *new_node = malloc(sizeof(__file_data_t));
		new_node->cbuf_id = cb;
		new_node->cbuf_len = sz; // it reeally doesn't.
		new_node->start = start;
		new_node->len = sz;
		new_node->next = NULL;

		current->next = new_node;

		// set offset
		t->offset += sz;
		fso->size = t->offset;
	}
	else
	{
		printc(">>>case 6\n");
		ERR_THROW(-EINVAL, done);
	}
	
	// always set t->offset to the actual END of previous memory
	// so you don't end up pointing to null
	printc(">>doing an assert\n");
	assert(t->offset);

done:	
	UNLOCK();
	return ret;
}

int cos_init(void)
{
	lock_static_init(&fs_lock);
	torlib_init();

	fs_init_root(&root);
	root_torrent.data = &root;
	root.flags = TOR_READ | TOR_SPLIT;

	return 0;
}
