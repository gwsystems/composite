#ifndef COS_SPD_NAME_MAP_H
#define COS_SPD_NAME_MAP_H

#include "../../../include/consts.h"

/* the index of the component corresponds to its id */
static char *spd_name_map[MAX_NUM_SPDS+1] = 
{ "c0.o",
  "fprr.o",
  "wftest.o",
  "mpd.o",
  "l.o",
  "mm.o",
  "print.o",
  "te.o",
  "net.o",
  "e.o",
  "",
  NULL};

static inline int spd_name_map_id(char *name)
{
	char *n;
	int i;

	for (i = 0 ; i < MAX_NUM_SPDS && spd_name_map[i] != NULL ; i++) {
		char *c, *o;

		n = spd_name_map[i];
		c = &n[0];
		o = &name[0];
		/* avoiding libraries here as they are different for
		 * composite and for cos_loader. Don't want to load
		 * all of string.h in the component if I don't have
		 * to. This is not performance critical at all. */
		while (*c != '\0' && *o != '\0') {
			if (*c != *o) break;
			c++;
			o++;
		}
		/* Note that this is both sneaky and a kludge: the
		 * passed in char might still have additional string
		 * contents.  We are just looking for a prepended
		 * similarity. The reason for this is the string
		 * formatting done when processing the objects in
		 * cos_loader */
		if (*c == '\0') return i;
	}
	return -1;
}

static inline char *spd_name_map_name(int spdid)
{
	if (spdid >= MAX_NUM_SPDS) return NULL;
	return spd_name_map[spdid];
}

#endif /* COS_SPD_NAME_MAP_H */
