#ifndef INITARGS_H
#define INITARGS_H

#include <tar.h>
#include <initargs.h>

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
	ARGS_KV,
	ARGS_TAR
} args_type_t;

typedef enum {
	VTYPE_STR,
	VTYPE_ARR,
} kv_valtype_t;

union kv_val {
	const char *str;
	struct {
		const int sz;
		const struct initargs **kvs;
	} arr;
};

struct kv_entry {
	const char *key;
	const kv_valtype_t vtype;
	const union kv_val val;
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
	args_type_t type;
	union {
		struct kv_entry *kv_ent;
		struct tar_entry tar_ent;
	} d;
};

struct initargs_iter {
	args_type_t type;
	union {
		struct kv_iter kv_i;
		struct tar_iter tar_i;
	} i;
};

/* Query the arguments, passing a path (/-delimited) */
const char *args_get(const char *path, int *str_len);
int args_get_entry(const char *path, struct initargs *entry);
/* Access the k/v of a given entry */
const char *args_key(const struct initargs *entry, int *str_len);
const char *args_value(const struct initargs *entry);
/* Iterate through the entries, particularly in a map. */
int args_len(const struct initargs *kv);
/*
 * Both of the following return 1 if an entry is found, and it is
 * populated in first, 0 otherwise.
 */
int args_iter(const struct initargs *kv, struct initargs_iter *i, struct initargs *first);
int args_iter_next(struct initargs_iter *i, struct initargs *next);

#endif /* INITARGS_H */
