#ifndef INITARGS_H
#define INITARGS_H

/*
 * A very simple nested K/V representation for retrieving initial
 * arguments to components.
 *
 * Conventions:
 * - The top K/V has a key "args" and contains only an array of the K/V arguments.
 * - If a map is just an array of values, then *each* key should be set to "_".
 * - Keys should not include '/' characters.
 */

#include <string.h>

typedef enum {
	VTYPE_STR,
	VTYPE_ARR,
} args_valtype_t;

union args_val {
	const char *str;
	struct {
		const int sz;
		const struct initargs **kvs;
	} arr;
};

struct initargs {
	const char *key;
	const args_valtype_t vtype;
	const union args_val val;
};

struct initargs_iter {
	const struct initargs *start;
	int curr, len;
};

extern const struct initargs __initargs_top;

/*
 * Operations on a specific K/V entry.  If you know that it should
 * contain a K/V pair with a value that resolves to a string, you can
 * use args_value successfully.
 *
 * Note that all functions in this API can take the kv argument as
 * NULL to ease error checking to be only required on the final
 * values.
 */

/* The key associated with a specific entry */
const char *
args_key(const struct initargs *kv)
{
	if (!kv) return NULL;
	return kv->key;
}

/* And the value, but only if it is a string (NULL otherwise) */
const char *
args_value(const struct initargs *kv)
{
	if (!kv) return NULL;
	switch (kv->vtype) {
	case VTYPE_STR: return kv->val.str;
	case VTYPE_ARR: return NULL;
	default: 	return NULL;
	}
}

/*
 * Operations on maps: get their size, index into them, and lookup an
 * entry by key.
 */

/* Length of the K/V map (= 1 for a K/V with a string value) */
static int
__args_len(const struct initargs *kv)
{
	if (!kv) return 0;
	switch (kv->vtype) {
	case VTYPE_STR: return 1;
	case VTYPE_ARR: return kv->val.arr.sz;
	default:        return 0;
	}
}

/* Index into a K/V map to get the Nth value. */
static const struct initargs *
__args_index(const struct initargs *kv, int idx)
{
	if (!kv) return NULL;
	if (idx >= __args_len(kv) || idx < 0) return NULL;
	if (kv->vtype == VTYPE_STR) return kv;
	return kv->val.arr.kvs[idx];
}

const struct initargs *
args_iter_next(struct initargs_iter *i)
{
	if (i->curr == i->len) return NULL;
	return __args_index(i->start, i->curr++);
}

/*
 * Initialize the iterator through (hopefully) the K/V store.  Note:
 * this returns the first item in the K/V store
 */
const struct initargs *
args_iter(const struct initargs *kv, struct initargs_iter *i)
{
	*i = (struct initargs_iter){
		.start = kv,
		.curr  = 0,
		.len   = __args_len(kv)
	};
	return args_iter_next(i);
}

/*
 * Lookup a key in a K/V map and return the corresponding entry.  Note
 * that this lookup is somewhat unique in that it will lookup the key
 * as a string (null terminated), or as a part of a path (up to and
 * not including a '/').  This enables this function to be used to
 * walk through the data-structure guided by a path through the k/v.
 */
const struct initargs *
args_lkup_entry(const struct initargs *kv, char *path)
{
	struct initargs_iter i;
	const struct initargs *curr;
	unsigned int len;
	char *slash, *key = path;

	if (!kv || !key) return NULL;
	/* Iterate through the path... */
	do {
		slash = strchr(key, '/');
		len = slash ? (unsigned int)(slash - key) : strlen(key);

		/* ...and look the key up in the KV */
		for (curr = args_iter(kv, &i) ; curr ; curr = args_iter_next(&i)) {
			const char *k = args_key(curr);

			if (strncmp(k, key, len) == 0 && strlen(k) == len) return curr;
		}

		if (slash) key = slash + 1;
	} while (slash && *key != '\0');

	return NULL;
}

/*
 * The "base-case" API where we need to do the initial lookup in the
 * KV map.  This requires basing the search in some structure:
 * __initargs_top.  This supports searching by a "path" through the
 * structure, which is just a /-separated set of keys used to lookup
 * in the corresponding maps.
 */
static const struct initargs *
args_get_entry(char *path)
{
	return args_lkup_entry(&__initargs_top, path);
}

static const char *
args_get(char *path)
{
	return args_value(args_get_entry(path));
}

#endif /* INITARGS_H */
