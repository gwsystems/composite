/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <torlib.h>
#include <errno.h>

/* Default torrent implementations */
__attribute__((weak)) int 
treadp(spdid_t spdid, int td, int *off, int *sz)
{
	return -ENOTSUP;
}

/* Utility functions */

COS_MAP_CREATE_STATIC(torrents);
struct torrent null_torrent, root_torrent;

int tor_cons(struct torrent *t, void *data, int flags)
{
	td_t td;
	assert(t);

	td        = (td_t)cos_map_add(&torrents, t);
	if (td == -1) return -1;
	t->td     = td;
	t->data   = data;
	t->flags  = flags;
	t->offset = 0;

	return 0;
}

struct torrent *tor_alloc(void *data, int flags)
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
void tor_free(struct torrent *t)
{
	assert(t);
	if (cos_map_del(&torrents, t->td)) BUG();
	free(t);
}

void torlib_init(void)
{
	cos_map_init_static(&torrents);
	/* save descriptors for the null and root spots */
	null_torrent.td = td_null;
	if (td_null != cos_map_add(&torrents, NULL)) BUG();
	root_torrent.td = td_root;
	if (td_root != cos_map_add(&torrents, &root_torrent)) BUG();
}
