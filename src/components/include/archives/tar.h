/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef TAR_H
#define TAR_H

#ifdef LINUX_TEST
#define round_to_pow2(x, pow2) (((unsigned long)(x)) & (~(pow2 - 1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x) + pow2 - 1, pow2))
#include <malloc.h>
#include <stdio.h>
#else
#include <cos_alloc.h>
#endif

#include <fs.h>
#include <string.h>
#define TAR_RECORD_SIZE 512

/* A tar record is a 512 byte chunk */
struct tar_record {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char checksum[8];
	char linkflag[1];
	char linkname[100];
	char pad[255];
};

/* We are at the end of an archive when we have two empty records. */
static inline int
tar_end(struct tar_record *r)
{
	int i;

	for (i = 0; i < TAR_RECORD_SIZE * 2; i++) {
		if (((char *)r)[i]) return 0;
	}

	return 1;
}

static inline int
oct2dec(char *oct)
{
	int i, base;
	int tot;

	//	i = strnlen(oct, TAR_RECORD_SIZE+1)-1; //dielibc...really: no strnlen???
	i = strlen(oct) - 1;
	if (i == TAR_RECORD_SIZE) return -1;

	for (base = 1, tot = 0; i >= 0; i--, base *= 8) {
		char val = oct[i];

		if (val > '7' || val < '0') return -1;
		val = val - '0';
		tot = tot + (val * base);
	}

	return tot;
}

/* return the next record, or NULL if THIS record doesn't exist. */
static inline struct tar_record *
tar_parse_record(struct tar_record *r, struct fsobj **o, struct fsobj *root)
{
	int           len, sz, records_sz;
	char *        name;
	fsobj_type_t  t;
	struct fsobj *parent, *new, *p;

	if (tar_end(r)) return NULL;

	len = strlen(r->name); // again: dietlibc has no strnlen...
	if (len > 100) return NULL;

	if (r->name[len - 1] == '/') {
		t = FSOBJ_DIR;
		/* remove trailing /s */
		do {
			r->name[--len] = '\0';
		} while (len && r->name[len - 1] == '/');
	} else {
		t = FSOBJ_FILE;
	}

	name = strrchr(&r->name[0], '/');
	if (name) {
		char *fail_path;
		int   len;

		*name = '\0';
		name++;
		len    = (int)(name - r->name[0]);
		parent = fsobj_path2obj(&r->name[0], len, root, &p, &fail_path);
	} else {
		name   = &r->name[0];
		parent = root;
	}
	assert(parent);

	sz = oct2dec(r->size);
	assert(!(sz && (t == FSOBJ_DIR)));
	assert(!((sz == 0) && (t == FSOBJ_FILE)));
	records_sz = round_up_to_pow2(sz, TAR_RECORD_SIZE) / TAR_RECORD_SIZE;

	new = FS_ALLOC(sizeof(struct fsobj));
	if (!new) return NULL;
	if (fsobj_cons(new, parent, name, t, sz, sz ? (char *)&r[1] : NULL)) BUG();
	*o = new;

	return &r[records_sz + 1];
}

#endif /* TAR_H */
