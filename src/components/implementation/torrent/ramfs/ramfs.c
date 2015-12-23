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

struct file_data {
	cbuf_t cbuf_id;
	int cbuf_len;
	int start;
	int len;
	void* next;
};

int file_data_init(struct file_data **fd, cbuf_t cbuf_id, int cbuf_length, int start, int len, void* next)
{
	assert(*fd);
	(*fd)->cbuf_id = cbuf_id;
	(*fd)->cbuf_len = cbuf_length;
	(*fd)->start = start;
	(*fd)->len = len;
	(*fd)->next = next;

	return 0;
}

int file_data_alloc(struct file_data **fd, cbuf_t cbuf_id, int cbuf_length, int start, int len, void* next)
{
	*fd = malloc(sizeof(struct file_data));
	if (!*fd) return -1;
	
	return file_data_init(fd, cbuf_id, cbuf_length, start, len, next);
}

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
	struct file_data *current, *prev;
	u32_t total_offset = 0;
	
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

	current = (struct file_data*) fso->data;

	/* trying to read a file with no data */
	if (!current) ERR_THROW(-EINVAL, done);

	/* find where we are reading from */
	while (total_offset < t->offset && current != NULL) {
		total_offset += current->len;

		prev = current;
		current = current->next;
	}

	/* Probably want to bomb out smarter but... for now */
	if (!current) ERR_THROW(-EINVAL, done); 

	/* current should now be equal to the file offset. */
	*off = current->start;
	*sz  = current->len;
	ret  = current->cbuf_id;

	t->offset += current->len;

	cbuf_send(current->cbuf_id);

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
twritep(spdid_t spdid, td_t td, int cb, int start, int sz)
{
	int ret = -1, left;
	struct torrent *t;
	struct fsobj *fso;

	if (!sz) {return -EINVAL;}

	if (tor_isnull(td)) {return -EINVAL;}

	LOCK();

	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	fso = t->data;
	assert(fso->size <= fso->allocated);
	assert(t->offset <= fso->size);

	unsigned int total_offset = 0;
	struct file_data *current = (struct file_data*) fso->data;
	
	/* file does not exist yet */
	if (current == NULL) {
		printc(">>>case 0: initial file creation\n");
		struct file_data *new_node;
		file_data_alloc(&new_node, cb, sz, start, sz, NULL);

		fso->data = new_node;

		t->offset += sz;
		fso->size = t->offset;

		ret = sz;

		goto done;
	}
	
	unsigned int curr_off = 0;
	struct file_data *prev = NULL;

	/* iterate through and try to find where to write to */
	while (current != NULL &&
		(total_offset + current->len) <= t->offset) {
		total_offset += current->len;

		prev = current;
		current = current->next;
	}

	curr_off = t->offset - total_offset;

	if (current == NULL) {
		printc(">>>case 2\n");
		// arguably the most likely case
		// just writing to the end of the file
		struct file_data *new_node;
		file_data_alloc(&new_node, cb, sz, start, sz, NULL);

		prev->next = new_node;

		t->offset += sz;
		fso->size = t->offset;
	}
	else if (sz < current->len - curr_off) {
		printc(">>>case 3\n");
		unsigned int offset_into_cbuf = current->start + curr_off;
		int split_len = current->len - offset_into_cbuf - sz;
		int copy_start = current->start + offset_into_cbuf + sz;
		cbuf_t split_cb;
		char *cb_ptr = cbuf_alloc(split_len, &split_cb);
		if (!cb_ptr) { ERR_THROW(-EINVAL, done); }
		char *buf = cbuf2buf(current->cbuf_id, current->len);
		memcpy(cb_ptr, buf + copy_start, split_len);

		// create a node for this new cbuf.
		struct file_data *split_node;
		file_data_alloc(&split_node, split_cb, split_len, 0, split_len, current->next);

		// now create a node for the cbuf they passed in.
		struct file_data *new_node;
		file_data_alloc(&new_node, cb, sz, start, sz, split_node);

		// modify current
		current->len -= (sz + split_len);
		current->next = new_node;
	}
	else if (sz > current->len) {
		printc(">>>case 4\n");

		struct file_data* ptr_node = current->next;
		struct file_data* head_node = current;
		if (curr_off == 0)
		{
			ptr_node = current;
			head_node = prev;
		}

		int bytesOverwritten = current->len - curr_off;
		current->len -= bytesOverwritten;

		while (ptr_node != NULL && bytesOverwritten + ptr_node->len
			< sz) {
			bytesOverwritten += ptr_node->len;
			struct file_data *temp_ptr = ptr_node;
			ptr_node = ptr_node->next;
			free(temp_ptr);
		}

		if (bytesOverwritten < sz && ptr_node != NULL) {
			ptr_node->start += (sz - bytesOverwritten);
		}

		struct file_data *new_node;
		file_data_alloc(&new_node, cb, sz, start, sz, ptr_node);

		head_node->next = new_node;
	}
	else {
		printc(">>>case 5: did not match anything!!!\n");
		printc("t->offset: %d\n", t->offset);
		ERR_THROW(-EINVAL, done);
	}

	// It should never equal anything else?
	ret = sz;

done:	
	// print current state
	current = (struct file_data*) fso->data; // start of file
	while (current != NULL)
	{
		char *full_buf = cbuf2buf(current->cbuf_id, current->cbuf_len);
		char *buf = malloc(sizeof(char) * current->len);
		memcpy(buf, full_buf + current->start, current->len);

		struct file_data *next = current->next;
		cbuf_t next_id = (next != NULL) ? next->cbuf_id : -1;

		printc("cbuf_id: %d start: %d size: %d next: %d data: [%s]\n", current->cbuf_id, current->start, current->len, next_id, buf);
		current = current->next;
	}
	
	// always set t->offset to the actual END of previous memory
	// so you don't end up pointing to null
	assert(t->offset);
	
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
