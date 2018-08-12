/**
 * Copyright 2018 by Gabriel Parmer, gparmer@gwu.edu
 *
 * All rights reserved.  Redistribution of this file is permitted
 * under the BSD 2 clause license.
 */
#include <stdio.h>

#include <tar.h>
#include <string.h>
#ifdef TAR_TEST
#define round_to_pow2(x, pow2) (((unsigned long)(x)) & (~(pow2 - 1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x) + pow2 - 1, pow2))
#else
#include <const.h>
#endif

/* We are at the end of an archive when we have two empty records. */
static inline int
tar_end(struct tar_record *r)
{
	int i;

	if (r == NULL) return 1;
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

	i = strnlen(oct, TAR_SZ) - 1;
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
tar_next_record(struct tar_record *r)
{
	int sz, n_records;

	if (tar_end(r)) return NULL;

	sz = oct2dec(r->size);
	n_records = round_up_to_pow2(sz, TAR_RECORD_SIZE) / TAR_RECORD_SIZE;

	return &r[n_records + 1];
}

static int
tar_valid(struct tar_entry *ent)
{
	return !(!ent || !ent->record || tar_end(ent->record));
}

/*
 * Return the key, and its length for a given nesting level, defined by ent.
 */
static char *
tar_nesting(int nesting_lvl, char *path, int *key_len)
{
	char *key, *end;
	int   i;

	key = path;
	for (i = 0 ; i < nesting_lvl ; i++) {
		key = strchr(key, '/');
		if (!key) return NULL;
		key++;		/* one past the / */
	}
	end = strchr(key, '/');
	if (!end) *key_len = strlen(key);
	else      *key_len = end - key;

	return key;
}

static char *
tar_path(struct tar_entry *ent)
{
	if (!tar_valid(ent) || ent->record == NULL) return NULL;

	return ent->record->name;
}

/*
 * Compare the path specified by the nesting level of ent, to the
 * path, to see if they match (thus could be part of the same array).
 * terminal returns if the found key is terminal (a directory or a
 * file, NOT an intermediate directory *holding* the final directory
 * or file).
 *
 * API mimics strcmp, 0 == match
 */
static int
tar_pathcmp(struct tar_entry *ent, const char *path)
{
	char *p, *end;
	const char *key;
	int ignore, cmp;
	size_t len;

	if (!tar_valid(ent)) return -1;
	/*
	 * Everything matches at the root, and not special casing this
	 * messes with the +1 logic below.
	 */
	if (ent->nesting_lvl == 0) {
		cmp = 0;
		key = path;
		goto done;
	}

	p   = tar_path(ent);
	key = tar_nesting(ent->nesting_lvl, path, &ignore);
	if (!key) return -1;
	if (*key == '\0') return 1; /* if we have the actual directory, avoid it */
	/* length from the start of the path, including / or \0 */
	len = (int)(key - path);
	cmp = strncmp(p, path, len);
done:
	end   = strchr(key, '/');
	/*
	 * We have a match if 1. the strings match, and 2. we have a
	 * file (no /), or we have a directory (nothing after the /).
	 */
	return !((cmp == 0) && (end == NULL || end[1] == '\0'));
}

/*
 * Find the first entry that matches a path/nesting while starting
 * looking at iter, and expanding from there.  The found entry will be
 * returned.  iter is updated to be passed in directly the next time
 * this is called.
 */
static struct tar_record *
tar_path_iter_next(struct tar_entry *path, struct tar_record **iter)
{
	struct tar_record *r;

//	printf("tar_path_iter_next: starting path %s\n", path->record->name);
	if (!tar_valid(path) || iter == NULL || *iter == NULL) return NULL;

	for (r = *iter; r && !tar_end(r) && tar_pathcmp(path, r->name); r = tar_next_record(r)) ;
	if (tar_end(r)) return NULL;
	*iter = tar_next_record(r);
//	printf("\tmatches: %s\n", r->name);

	return r;
}

const char *
tar_key(struct tar_entry *ent, int *str_len)
{
	return tar_nesting(ent->nesting_lvl, ent->record->name, str_len);
}

const char *
tar_value(struct tar_entry *ent)
{
	if (!tar_valid(ent)) return NULL;
	if (!tar_is_file(ent->record)) return NULL;

	return (char *)&ent->record[1];
}

int
tar_value_sz(struct tar_entry *ent)
{
	if (!tar_valid(ent)) return 0;
	if (!tar_is_file(ent->record)) return 0;

	return oct2dec(ent->record->size);
}

int
tar_iter_next(struct tar_iter *i, struct tar_entry *next)
{
	struct tar_record *r;

	if (!i || !tar_valid(&i->entry)) return 0;

//	printf("iter_next: iter with %s\n", i->entry.record->name);
	r = tar_path_iter_next(&i->entry, &i->iter_rec);
	if (!r) return 0;

	next->nesting_lvl = i->entry.nesting_lvl;
	next->record = r;

//	printf("iter_next: return record %p, %s\n", next->record, next->record->name);

	return 1;
}

/*
 * Assume: the directory entry in the tarball appears *before* any of
 * the entries of that directory. I believe this assumption makes
 * sense, but I can see no guarantee it is always true.  If the
 * assumptions is wrong, we will skip all of those entries in a
 * directory that come *before* the directory record.
 */
int
tar_iter(struct tar_entry *ent, struct tar_iter *i, struct tar_entry *first)
{
	if (!tar_valid(ent)) return 0;
	if (!tar_is_dir(ent->record)) return 0;

	i->entry    = *ent;
	i->iter_rec = ent->record;
	i->entry.nesting_lvl++;	/* look *in* the entry */

	return tar_iter_next(i, first);
}

int
tar_len(struct tar_entry *start)
{
	int cont = 0, cnt = 0;
	struct tar_iter i;
	struct tar_entry entry;	/* data placed in here; don't care about it for calculating len */
	for (cont = tar_iter(start, &i, &entry); cont ; cont = tar_iter_next(&i, &entry)) {
		cnt++;
	}

	return cnt;
}

extern struct tar_record _binary_booter_bins_tar_start[];
extern struct tar_record _binary_booter_bins_tar_end[];

struct tar_entry __tar_root = {
	.nesting_lvl = -1, 	/* tar archives don't start paths with a '/' */
	.record = _binary_booter_bins_tar_start
};

struct tar_entry *
tar_root(void)
{
	if (_binary_booter_bins_tar_end - _binary_booter_bins_tar_start < 2) return NULL;

	return &__tar_root;
}

#ifdef TAR_TEST

#include <stdio.h>
#include <stdlib.h>

typedef void (*visitor_fn_t)(struct tar_entry *ent, void *data);

void
foreach(struct tar_entry *root, visitor_fn_t fn, void *data)
{
	struct tar_entry ent;
	struct tar_iter i;
	int cont;

	for (cont = tar_iter(root, &i, &ent); cont ; cont = tar_iter_next(&i, &ent)) {
		fn(&ent, data);
	}
}

#define PATH_LEN 2
char *path[PATH_LEN] = {"dir2", "subdir2"};
struct file_cont {
	char filename[16];
	char contents[32];
	int found;
};
struct file_cont file_contents[] = {
	{.filename = "file3", .contents = "file3 contents\n", .found = 0},
	{.filename = "file4", .contents = "file4 contents\n", .found = 0},
	{.filename = "", .contents = "", .found = 0}
};
int failure = 0;

void
path_recur(struct tar_entry *ent, void *off)
{
	int nesting = (int)off;
	int sz;
	char *key = tar_key(ent, &sz);

	if (nesting > PATH_LEN) return;
	if (tar_is_dir(ent->record)) {
		if (strncmp(path[nesting], key, sz)) return;
		foreach(ent, path_recur, (void*)(nesting+1));
	} else if (tar_is_file(ent->record)) {
		int i;

		for (i = 0; strlen(file_contents[i].filename) > 0; i++) {
			if (strcmp(file_contents[i].filename, key)) continue;
			if (strcmp(file_contents[i].contents, tar_value(ent))) {
				printf("FAILURE: file %s found, but expected contents:\n%s\nversus contents found:\n%s",
				       key, file_contents[i].contents, tar_value(ent));
				failure = 1;
			}
			file_contents[i].found = 1;
		}
	} else {
		printf("Error: found a non-directory/non-file entry: %s\n", key);
	}
}

int
main(void)
{
	struct tar_entry *root = tar_root();
	int i;

	if (tar_len(root) != 3) {
		printf("FAILURE: top level should have three objects, but reports %d.\n",
		       tar_len(root));
		failure = 1;
	}
	foreach(root, path_recur, (void *)0);
	for (i = 0; strlen(file_contents[i].filename) ; i++) {
		if (!file_contents[i].found) {
			printf("FAILURE: did not find file dir2/subdir2/%s in the tarball.\n",
				file_contents[i].filename);
			failure = 1;
		}
	}

	if (!failure) {
		printf("SUCCESS: all tests passed.\n");
	}

	return 0;
}

/*** Makefile used to test (yes, it is ugly): ***

COMPDIR=/home/gparmer/research/composite/src/components

all: tar_bin.o
	gcc -Wall -Wextra -DTAR_TEST -I$(COMPDIR)/include/ $(COMPDIR)/lib/tar.c -c -o tar_test.o
	gcc tar_test.o tar_bin.o -o a.out

tar_bin.o: tartest.tar
	cp tartest.tar booter_bins.tar
	ld -r -b binary booter_bins.tar -o tar_bin.o
	rm booter_bins.tar

tartest.tar:
	mkdir dir1 dir2 dir3
	mkdir dir2/subdir1 dir2/subdir2
	echo "file1 contents" > dir1/file1
	echo "file2 contents" > dir2/subdir1/file2
	echo "file3 contents" > dir2/subdir2/file3
	echo "file4 contents" > dir2/subdir2/file4
	echo "file5 contents" > dir3/file5
	tar cvf tartest.tar dir1 dir2 dir3
	rm -rf dir1 dir2 dir3

clean:
	rm -rf *.o tartest.tar a.out

*** end Makefile used to test ***/

#endif
