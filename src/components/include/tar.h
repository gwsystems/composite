/**
 * Copyright 2018 by Gabriel Parmer, gparmer@gwu.edu
 *
 * All rights reserved.  Redistribution of this file is permitted
 * under the BSD 2 clause license.
 *
 * This library has the main goals of:
 * - parsing tar files for easy access via the K/V API of the args
 *   library.
 * - memory allocation freedom -- the client provides the transient
 *   memory used for tracking state through traversals
 * - iterator-based access to directories
 */

#ifndef TAR_H
#define TAR_H

#define TAR_RECORD_SIZE 512
#define TAR_SZ 12
#define TAR_NAME_SZ 100
#define TAR_KEY_SZ 32

/* A tar record is a 512 byte chunk */
struct tar_record {
	char name[TAR_NAME_SZ];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[TAR_SZ];
	char mtime[12];
	char checksum[8];
	char linkflag[1];
	char linkname[100];
	char pad[255];
};

struct tar_entry {
	int nesting_lvl;
	struct tar_record *record; /* mainly used for the path */
};

struct tar_iter {
	struct tar_entry entry;	/* store path + nesting level of this iteration */
	struct tar_record *iter_rec; /* ...and iterate through the tarball */
};

char *tar_key(struct tar_entry *ent, int *str_len);
char *tar_value(struct tar_entry *ent);
int tar_value_sz(struct tar_entry *ent);
int tar_len(struct tar_entry *ent);
int tar_is_value(struct tar_entry *ent);

/* create the iterator for a directory, and return the first entry */
int tar_iter(struct tar_entry *ent, struct tar_iter *i, struct tar_entry *first);
/* iterate and find the next entry */
int tar_iter_next(struct tar_iter *i, struct tar_entry *next);

struct tar_entry *tar_root(void);

#endif /* TAR_H */
