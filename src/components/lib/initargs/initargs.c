#include <initargs.h>
#include <stdio.h>
#include <string.h>

extern struct initargs __initargs_root;

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
static char *
kv_key(struct kv_entry *kv, int *key_len)
{
	if (!kv) return NULL;
	*key_len = strlen(kv->key);
	return kv->key;
}

/* And the value, but only if it is a string (NULL otherwise) */
static char *
kv_value(struct kv_entry *kv)
{
	if (!kv) return NULL;
	switch (kv->vtype) {
	case VTYPE_STR: return kv->val.str;
	case VTYPE_ARR: return NULL;
	default: 	return NULL;
	}
}

static args_type_t
kv_type(struct kv_entry *kv)
{
	switch (kv->vtype) {
	case VTYPE_STR: return ARGS_VAL;
	case VTYPE_ARR: return ARGS_MAP;
	default:        return ARGS_ERR;
	}
}

/*
 * Operations on maps: get their size, index into them, and lookup an
 * entry by key.
 */

/* Length of the K/V map (= 1 for a K/V with a string value) */
static int
kv_len(struct kv_entry *kv)
{
	if (!kv) return 0;
	switch (kv->vtype) {
	case VTYPE_STR: return 1;
	case VTYPE_ARR: return kv->val.arr.sz;
	default:        return 0;
	}
}

/* Index into a K/V map to get the Nth value. */
static struct kv_entry *
__kv_index(struct kv_entry *kv, int idx)
{
	if (!kv) return NULL;
	if (idx >= kv_len(kv) || idx < 0) return NULL;
	if (kv->vtype == VTYPE_STR) return kv;
	return kv->val.arr.kvs[idx];
}

static int
kv_iter_next(struct kv_iter *i, struct kv_entry **ent)
{
	if (i->start->vtype != VTYPE_ARR) return 0;
	if (i->curr == i->len) return 0;
	*ent = __kv_index(i->start, i->curr++);

	return 1;
}

/*
 * Initialize the iterator through (hopefully) the K/V store.  Note:
 * this returns the first item in the K/V store
 */
static int
kv_iter(struct kv_entry *kv, struct kv_iter *i, struct kv_entry **ent)
{
	*i = (struct kv_iter){
		.start = kv,
		.curr  = 0,
		.len   = kv_len(kv)
	};

	return kv_iter_next(i, ent);
}

char *
args_key(struct initargs *arg, int *arg_len)
{
	switch (arg->type) {
	case ARGS_IMPL_KV:  return kv_key(arg->d.kv_ent, arg_len);
	case ARGS_IMPL_TAR: return tar_key(&arg->d.tar_ent, arg_len);
	default:            return NULL;
	}
}

char *
args_value(struct initargs *arg)
{
	switch (arg->type) {
	case ARGS_IMPL_KV:  return kv_value(arg->d.kv_ent);
	case ARGS_IMPL_TAR: return tar_value(&arg->d.tar_ent);
	default:            return NULL;
	}
}

int
args_len(struct initargs *arg)
{
	switch (arg->type) {
	case ARGS_IMPL_KV:  return kv_len(arg->d.kv_ent);
	case ARGS_IMPL_TAR: return tar_len(&arg->d.tar_ent);
	default:            return 0;
	}
}

int
args_iter_next(struct initargs_iter *i, struct initargs *arg)
{
	arg->type = i->type;
	switch (i->type) {
	case ARGS_IMPL_KV:  return kv_iter_next(&i->i.kv_i, &arg->d.kv_ent);
	case ARGS_IMPL_TAR: return tar_iter_next(&i->i.tar_i, &arg->d.tar_ent);
	default:            return 0;
	}
}

int
args_iter(struct initargs *arg, struct initargs_iter *i, struct initargs *ent)
{
	i->type = ent->type = arg->type;
	switch (arg->type) {
	case ARGS_IMPL_KV:  return kv_iter(arg->d.kv_ent, &i->i.kv_i, &ent->d.kv_ent);
	case ARGS_IMPL_TAR: return tar_iter(&arg->d.tar_ent, &i->i.tar_i, &ent->d.tar_ent);
	default:            return 0;
	}
}

typedef void (*args_visitor_fn_t)(struct initargs *ent, void *data);

int
args_foreach(struct initargs *ent, args_visitor_fn_t fn, void *data)
{
	struct initargs_iter i;
	struct initargs curr;
	int cont;

	if (!ent) return -1;
	for (cont = args_iter(ent, &i, &curr); cont; cont = args_iter_next(&i, &curr)) {
		fn(&curr, data);
	}

	return 0;
}

/*
 * Lookup a key in a K/V map and return the corresponding entry.  Note
 * that this lookup is somewhat unique in that it will lookup the key
 * as a string (null terminated), or as a part of a path (up to and
 * not including a '/').  This enables this function to be used to
 * walk through the data-structure guided by a path through the k/v.
 */
int
args_lkup_entry(struct initargs *arg, char *path, struct initargs *ret)
{
	struct initargs_iter i;
	struct initargs curr, start;
	unsigned int len, cont;
	char *slash, *key = path;

	if (!arg || !key || !ret) return -1;
	start = *arg;
	/* Iterate through the path... */
	do {
		int found = 0;

		slash = strchr(key, '/');
		len = slash ? (unsigned int)(slash - key) : strlen(key);

		/* ...and look the key up in the KV */
		for (cont = args_iter(&start, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
			int key_len;
			char *k = args_key(&curr, &key_len);

			if (strncmp(k, key, len) == 0) {
				if (strlen(key) == len) {
					*ret = curr;
					return 0;
				}
				start = curr;
				found = 1;
				break;
			}
		}
		if (!found) return -1;

		if (slash) key = slash + 1;
	} while (slash && *key != '\0');

	return -1;
}

int
args_type(struct initargs *ent)
{
	switch (ent->type) {
	case ARGS_IMPL_KV:  return kv_type(ent->d.kv_ent);
	case ARGS_IMPL_TAR: return tar_is_value(&ent->d.tar_ent) ? ARGS_VAL : ARGS_MAP;
	default:            return ARGS_ERR;
	}
}

int
args_get_entry_from(char *path, struct initargs *from, struct initargs *ent)
{
	return args_lkup_entry(from, path, ent);
}

char *
args_get_from(char *path, struct initargs *from)
{
	struct initargs ent;

	if (args_get_entry_from(path, from, &ent)) return NULL;

	return args_value(&ent);
}

/*
 * The "base-case" API where we need to do the initial lookup in the
 * KV map.  This requires basing the search in some structure:
 * __initargs_root.  This supports searching by a "path" through the
 * structure, which is just a /-separated set of keys used to lookup
 * in the corresponding maps.
 */
int
args_get_entry(char *path, struct initargs *ent)
{
	struct initargs tarroot;
	struct tar_entry *tarent;

	if (!args_get_entry_from(path, &__initargs_root, ent)) return 0;

	tarent = tar_root();
	if (!tarent) return -1;
	tarroot = (struct initargs) {
		.type = ARGS_IMPL_TAR,
		.d.tar_ent = *tarent
	};

	return args_get_entry_from(path, &tarroot, ent);
}

char *
args_get(char *path)
{
	struct initargs ent;

	if (args_get_entry(path, &ent)) return NULL;

	return args_value(&ent);
}

#ifdef ARGS_TEST

static struct kv_entry __initargs_autogen_6 = { key: "name", vtype: VTYPE_STR, val: { str: "call_args" } };
static struct kv_entry __initargs_autogen_7 = { key: "client", vtype: VTYPE_STR, val: { str: "2" } };
static struct kv_entry __initargs_autogen_8 = { key: "server", vtype: VTYPE_STR, val: { str: "1" } };
static struct kv_entry __initargs_autogen_9 = { key: "c_fn_addr", vtype: VTYPE_STR, val: { str: "23071121" } };
static struct kv_entry __initargs_autogen_10 = { key: "c_ucap_addr", vtype: VTYPE_STR, val: { str: "23130304" } };
static struct kv_entry __initargs_autogen_11 = { key: "s_fn_addr", vtype: VTYPE_STR, val: { str: "4210752" } };
static struct kv_entry *__initargs_autogen_5[] = {&__initargs_autogen_11, &__initargs_autogen_10, &__initargs_autogen_9, &__initargs_autogen_8, &__initargs_autogen_7, &__initargs_autogen_6};
static struct kv_entry __initargs_autogen_4 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 6, kvs: __initargs_autogen_5 } } };
static struct kv_entry __initargs_autogen_14 = { key: "name", vtype: VTYPE_STR, val: { str: "call_arg" } };
static struct kv_entry __initargs_autogen_15 = { key: "client", vtype: VTYPE_STR, val: { str: "2" } };
static struct kv_entry __initargs_autogen_16 = { key: "server", vtype: VTYPE_STR, val: { str: "1" } };
static struct kv_entry __initargs_autogen_17 = { key: "c_fn_addr", vtype: VTYPE_STR, val: { str: "23071121" } };
static struct kv_entry __initargs_autogen_18 = { key: "c_ucap_addr", vtype: VTYPE_STR, val: { str: "23130292" } };
static struct kv_entry __initargs_autogen_19 = { key: "s_fn_addr", vtype: VTYPE_STR, val: { str: "4210704" } };
static struct kv_entry *__initargs_autogen_13[] = {&__initargs_autogen_19, &__initargs_autogen_18, &__initargs_autogen_17, &__initargs_autogen_16, &__initargs_autogen_15, &__initargs_autogen_14};
static struct kv_entry __initargs_autogen_12 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 6, kvs: __initargs_autogen_13 } } };
static struct kv_entry __initargs_autogen_22 = { key: "name", vtype: VTYPE_STR, val: { str: "call_two" } };
static struct kv_entry __initargs_autogen_23 = { key: "client", vtype: VTYPE_STR, val: { str: "2" } };
static struct kv_entry __initargs_autogen_24 = { key: "server", vtype: VTYPE_STR, val: { str: "1" } };
static struct kv_entry __initargs_autogen_25 = { key: "c_fn_addr", vtype: VTYPE_STR, val: { str: "23071121" } };
static struct kv_entry __initargs_autogen_26 = { key: "c_ucap_addr", vtype: VTYPE_STR, val: { str: "23130256" } };
static struct kv_entry __initargs_autogen_27 = { key: "s_fn_addr", vtype: VTYPE_STR, val: { str: "4210560" } };
static struct kv_entry *__initargs_autogen_21[] = {&__initargs_autogen_27, &__initargs_autogen_26, &__initargs_autogen_25, &__initargs_autogen_24, &__initargs_autogen_23, &__initargs_autogen_22};
static struct kv_entry __initargs_autogen_20 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 6, kvs: __initargs_autogen_21 } } };
static struct kv_entry __initargs_autogen_30 = { key: "name", vtype: VTYPE_STR, val: { str: "call_3rets" } };
static struct kv_entry __initargs_autogen_31 = { key: "client", vtype: VTYPE_STR, val: { str: "2" } };
static struct kv_entry __initargs_autogen_32 = { key: "server", vtype: VTYPE_STR, val: { str: "1" } };
static struct kv_entry __initargs_autogen_33 = { key: "c_fn_addr", vtype: VTYPE_STR, val: { str: "23071121" } };
static struct kv_entry __initargs_autogen_34 = { key: "c_ucap_addr", vtype: VTYPE_STR, val: { str: "23130316" } };
static struct kv_entry __initargs_autogen_35 = { key: "s_fn_addr", vtype: VTYPE_STR, val: { str: "4210800" } };
static struct kv_entry *__initargs_autogen_29[] = {&__initargs_autogen_35, &__initargs_autogen_34, &__initargs_autogen_33, &__initargs_autogen_32, &__initargs_autogen_31, &__initargs_autogen_30};
static struct kv_entry __initargs_autogen_28 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 6, kvs: __initargs_autogen_29 } } };
static struct kv_entry __initargs_autogen_38 = { key: "name", vtype: VTYPE_STR, val: { str: "call_four" } };
static struct kv_entry __initargs_autogen_39 = { key: "client", vtype: VTYPE_STR, val: { str: "2" } };
static struct kv_entry __initargs_autogen_40 = { key: "server", vtype: VTYPE_STR, val: { str: "1" } };
static struct kv_entry __initargs_autogen_41 = { key: "c_fn_addr", vtype: VTYPE_STR, val: { str: "23071121" } };
static struct kv_entry __initargs_autogen_42 = { key: "c_ucap_addr", vtype: VTYPE_STR, val: { str: "23130280" } };
static struct kv_entry __initargs_autogen_43 = { key: "s_fn_addr", vtype: VTYPE_STR, val: { str: "4210656" } };
static struct kv_entry *__initargs_autogen_37[] = {&__initargs_autogen_43, &__initargs_autogen_42, &__initargs_autogen_41, &__initargs_autogen_40, &__initargs_autogen_39, &__initargs_autogen_38};
static struct kv_entry __initargs_autogen_36 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 6, kvs: __initargs_autogen_37 } } };
static struct kv_entry __initargs_autogen_46 = { key: "name", vtype: VTYPE_STR, val: { str: "call_three" } };
static struct kv_entry __initargs_autogen_47 = { key: "client", vtype: VTYPE_STR, val: { str: "2" } };
static struct kv_entry __initargs_autogen_48 = { key: "server", vtype: VTYPE_STR, val: { str: "1" } };
static struct kv_entry __initargs_autogen_49 = { key: "c_fn_addr", vtype: VTYPE_STR, val: { str: "23071121" } };
static struct kv_entry __initargs_autogen_50 = { key: "c_ucap_addr", vtype: VTYPE_STR, val: { str: "23130268" } };
static struct kv_entry __initargs_autogen_51 = { key: "s_fn_addr", vtype: VTYPE_STR, val: { str: "4210608" } };
static struct kv_entry *__initargs_autogen_45[] = {&__initargs_autogen_51, &__initargs_autogen_50, &__initargs_autogen_49, &__initargs_autogen_48, &__initargs_autogen_47, &__initargs_autogen_46};
static struct kv_entry __initargs_autogen_44 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 6, kvs: __initargs_autogen_45 } } };
static struct kv_entry __initargs_autogen_54 = { key: "name", vtype: VTYPE_STR, val: { str: "call" } };
static struct kv_entry __initargs_autogen_55 = { key: "client", vtype: VTYPE_STR, val: { str: "2" } };
static struct kv_entry __initargs_autogen_56 = { key: "server", vtype: VTYPE_STR, val: { str: "1" } };
static struct kv_entry __initargs_autogen_57 = { key: "c_fn_addr", vtype: VTYPE_STR, val: { str: "23071121" } };
static struct kv_entry __initargs_autogen_58 = { key: "c_ucap_addr", vtype: VTYPE_STR, val: { str: "23130244" } };
static struct kv_entry __initargs_autogen_59 = { key: "s_fn_addr", vtype: VTYPE_STR, val: { str: "4210512" } };
static struct kv_entry *__initargs_autogen_53[] = {&__initargs_autogen_59, &__initargs_autogen_58, &__initargs_autogen_57, &__initargs_autogen_56, &__initargs_autogen_55, &__initargs_autogen_54};
static struct kv_entry __initargs_autogen_52 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 6, kvs: __initargs_autogen_53 } } };
static struct kv_entry *__initargs_autogen_3[] = {&__initargs_autogen_52, &__initargs_autogen_44, &__initargs_autogen_36, &__initargs_autogen_28, &__initargs_autogen_20, &__initargs_autogen_12, &__initargs_autogen_4};
static struct kv_entry __initargs_autogen_2 = { key: "sinvs", vtype: VTYPE_ARR, val: { arr: { sz: 7, kvs: __initargs_autogen_3 } } };
static struct kv_entry __initargs_autogen_62 = { key: "2", vtype: VTYPE_STR, val: { str: "tests.unit_pingpong.ping" } };
static struct kv_entry __initargs_autogen_63 = { key: "1", vtype: VTYPE_STR, val: { str: "pong.pingpong.pong" } };
static struct kv_entry *__initargs_autogen_61[] = {&__initargs_autogen_63, &__initargs_autogen_62};
static struct kv_entry __initargs_autogen_60 = { key: "components", vtype: VTYPE_ARR, val: { arr: { sz: 2, kvs: __initargs_autogen_61 } } };
static struct kv_entry *__initargs_autogen_1[] = {&__initargs_autogen_60, &__initargs_autogen_2};
static struct kv_entry __initargs_autogen_0 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 2, kvs: __initargs_autogen_1 } } };

struct initargs __initargs_root = { type: ARGS_IMPL_KV, d: { kv_ent: &__initargs_autogen_0 } };

#include <stdio.h>
#include <stdlib.h>

int
expect(int boolean, char *test)
{
	if (boolean) printf("SUCCESS: %s\n", test);
	else         printf("FAILURE: %s\n", test);
	return boolean;
}

void
cnt_twos(struct initargs *ent, void *data)
{
	int *cnt = data;
	char *val = args_get_from("client", ent);
	expect(val != NULL,  "Looking up \"client\" key in \"sinvs\".");

	if (!strcmp("2", val)) (*cnt)++;
}

int
kv_test(void)
{
	struct initargs entry;
	char *key;
	int cnt = 0;

	key = args_get("components/1");
	if (expect(key != NULL, "arg_get \"components/1\".")) {
		expect(strcmp(key, "pong.pingpong.pong") == 0,
		       "path components/1 should resolve to pong.pingpong.pong.");
	}
	if (expect(args_get_entry("sinvs", &entry) == 0, "args_get_entry for sinvs.")) {
		expect(args_foreach(&entry, cnt_twos, &cnt) == 0, "args_foreach through sinvs.");
		expect(cnt == 7, "counted number of client id \"2\" should be 7.");
		expect(args_len(&entry) == 7, "Length of the sinvs array should be 7");
	}
	return 0;
}

void
tar_test(void)
{
	char *val;
	struct initargs entry;

	val = args_get("dir2/subdir2/file3");
	if (expect(val != NULL, "Lookup of tar file dir2/subdir2/file3")) {
		expect(!strcmp(val, "file3 contents\n"), "checking contents of file3");
	}

	if (expect(args_get_entry("dir2", &entry) == 0, "Looking up the dir2 tar subdirectory")) {
		expect(args_type(&entry) == ARGS_MAP, "Checking that dir2 is a map");
		expect(args_len(&entry) == 2, "Checking that dir2 contains 2 entries");
		expect(!strcmp(args_get_from("subdir2/file4", &entry), "file4 contents\n"), "args_get_from to get and check file4 contents");
	}
}

int
args_test(void)
{
	kv_test();
	tar_test();
	return 0;
}

int
main(void)
{ return args_test(); }

#else

/* Default empty K/V map if we don't get them from the system specification. */
static struct kv_entry __initargs_default_empty = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 0, kvs: NULL } } };
struct initargs __initargs_root __attribute__((weak)) = { type: ARGS_IMPL_KV, d: { kv_ent: &__initargs_default_empty } };

#endif

/*** Testing this uses the following Makefile:

COMPDIR=/home/gparmer/research/composite/src/components

all: tartest argstest

tartest: tar_bin.o
	gcc -Wall -Wextra -DTAR_TEST -I$(COMPDIR)/include/ -g $(COMPDIR)/lib/tar.c tar_bin.o -o tartest

argstest: tar_bin.o
	gcc -Wall -Wextra -DARGS_TEST -I$(COMPDIR)/include/ -g $(COMPDIR)/lib/tar.c -c -o tar_test.o
	gcc -Wall -Wextra -DARGS_TEST -I$(COMPDIR)/include/ -g $(COMPDIR)/lib/initargs.c -c -o initargs_test.o
	gcc tar_test.o initargs_test.o tar_bin.o -g -o argstest

tar_bin.o: tartest.tar
	cp tartest.tar booter_bins.tar
	ld -r -b binary booter_bins.tar -o tar_bin.o
	rm booter_bins.tar

tartest.tar:
	mkdir dir1 dir2 dir3
	mkdir dir2/subdir1 dir2/subdir2 dir3/subdir3
	echo "file1 contents" > dir1/file1
	echo "file2 contents" > dir2/subdir1/file2
	echo "file3 contents" > dir2/subdir2/file3
	echo "file4 contents" > dir2/subdir2/file4
	echo "file5 contents" > dir3/subdir3/file5
	tar cvf tartest.tar dir1 dir2 dir3
	rm -rf dir1 dir2 dir3

clean:
	rm -rf *.o tartest.tar argstest tartest

***/
