/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef TORLIB_H
#define TORLIB_H

#include <torrent.h>
#include <cos_map.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct torrent {
	td_t td;
	u32_t offset;
	int flags;
	long evtid;
	void *data;
};
extern cos_map_t torrents;
extern struct torrent null_torrent, root_torrent;

static inline struct torrent *
tor_lookup(td_t td)
{
	struct torrent *t;
	
	t = cos_map_lookup(&torrents, td);
	if (!t) return NULL;
	assert(t->td == td);

	return t;
}

static inline int 
tor_isnull(td_t td)
{
	return td == td_null;
}

static inline int
tor_is_usrdef(td_t td)
{
	return !(td == td_null || td == td_root);
}

int tor_cons(struct torrent *t, void *data, int flags);
struct torrent *tor_alloc(void *data, int flags);
void tor_free(struct torrent *t);
void torlib_init(void);

#endif
