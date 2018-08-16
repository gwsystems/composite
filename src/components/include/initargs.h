#ifndef INITARGS_H
#define INITARGS_H

#include <tar.h>

/*
 * A very simple nested K/V representation for retrieving initial
 * arguments to components.
 *
 * Conventions:
 * - The top K/V has a key "args" and contains only an array of the K/V arguments.
 * - If a map is just an array of values, then *each* key should be set to "_".
 * - Keys should not include '/' characters.
 */

typedef enum {
	ARGS_ERR = 0,
	ARGS_MAP,
	ARGS_VAL
} args_type_t;

typedef enum {
	ARGS_IMPL_KV,
	ARGS_IMPL_TAR
} args_impltype_t;

typedef enum {
	VTYPE_STR,
	VTYPE_ARR,
} kv_valtype_t;

struct kv_entry;
union kv_val {
	char *str;
	struct {
		int sz;
		struct kv_entry **kvs;
	} arr;
};

struct kv_entry {
	char *key;
	kv_valtype_t vtype;
	union kv_val val;
};

struct kv_iter {
	struct kv_entry *start;
	int curr, len;
};

/*
 * This is the structure that holds persistent data that the caller
 * must save to avoid memory allocation.
 */
struct initargs {
	args_impltype_t type;
	union {
		struct kv_entry *kv_ent;
		struct tar_entry tar_ent;
	} d;
};

struct initargs_iter {
	args_impltype_t type;
	union {
		struct kv_iter kv_i;
		struct tar_iter tar_i;
	} i;
};

/* Query the arguments, passing a path (/-delimited) */
char *args_get(char *path);
int args_get_entry(char *path, struct initargs *entry);
/* Access the k/v of a given entry */
 char *args_key(struct initargs *entry, int *str_len);
 char *args_value(struct initargs *entry);
/* Iterate through the entries, particularly in a map. */
int args_len(struct initargs *kv);
int args_iter(struct initargs *kv, struct initargs_iter *i, struct initargs *first);
int args_iter_next(struct initargs_iter *i, struct initargs *next);

#endif /* INITARGS_H */
