/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/*
 * A simple ram file-system interface.  Essentially, just a
 * hierarchical tree that maintains data at each node.  Each parent
 * maintains a linked list of children, so walking through the
 * hierarchy is almost always O(C_p * P) where C_p is the number of
 * children for a parent p, and P is the length of the path.  We could
 * 1) sort the children, or 2) use a more intelligent data-structure
 * to index them, but I'm striving for simplicity here.
 */

#ifndef FS_H
#define FS_H

#ifdef LINUX_TEST
typedef unsigned long u32_t;
#include <assert.h>
#define BUG() assert(0);
#include <stdio.h>
#endif

#include <string.h>
#include <cos_list.h>

#ifndef FS_ALLOC
#define FS_ALLOC malloc
#define FS_FREE free
#endif

#ifndef FS_DATA_FREE
#define FS_DATA_FREE free
#endif

typedef enum {
	FSOBJ_FILE,
	FSOBJ_DIR,
	FSOBJ_ROOT,
} fsobj_type_t;

struct fsobj {
	char *        name;
	fsobj_type_t  type;
	u32_t         size, allocated, refcnt;
	int           flags; /* only defined in client code */
	char *        data;
	struct fsobj *next, *prev;
	struct fsobj *child, *parent; /* child != NULL iff type = dir */
};

#define ERR_HAND(errval, label) \
	do {                    \
		ret = errval;   \
		goto label;     \
	} while (0)

static void
fs_init_root(struct fsobj *o)
{
	o->name = "";
	o->type = FSOBJ_DIR;
	o->size = o->allocated = 0;
	o->refcnt              = 1;
	o->data                = NULL;
	INIT_LIST(o, next, prev);
	o->child = o->parent = NULL;
}

/* parent must be a directory: -1 otherwise */
static inline int
fsobj_child_add(struct fsobj *child, struct fsobj *parent)
{
	assert(child && parent);
	if (parent->type != FSOBJ_DIR) return -1;

	if (!parent->child)
		parent->child = child;
	else
		ADD_LIST(parent->child, child, next, prev);
	child->parent = parent;

	return 0;
}

/*
 * Construct a file system object.
 */
static inline int
fsobj_cons(struct fsobj *o, struct fsobj *parent, char *name, fsobj_type_t t, u32_t sz, char *data)
{
	assert(o && parent && name);
	assert(!(sz && (t == FSOBJ_DIR)));

	o->name      = name;
	o->type      = t;
	o->allocated = o->size = sz;
	o->refcnt              = 1;
	o->data                = data;
	INIT_LIST(o, next, prev);
	o->child = NULL;

	return fsobj_child_add(o, parent);
}

/*
 * The name can include zero or one '/'.  If it includes one, it must
 * in the final position (before '\0'), and that is used to denote
 * that we are creating a directory instead of a file.  name must be
 * null terminated.
 */
struct fsobj *
fsobj_alloc(char *name, struct fsobj *parent)
{
	char *        end, *chld_name;
	struct fsobj *chld = NULL;
	fsobj_type_t  t;
	int           len;

	end = strchr(name, '/');
	if (end) {
		len = end - name;
		/* only a single / allowed, at the end */
		if (end[1] != '\0') {
			printc("0\n");
			goto done;
		}
		t = FSOBJ_DIR;
	} else {
		len = strlen(name);
		t   = FSOBJ_FILE;
	}

	chld = FS_ALLOC(sizeof(struct fsobj));
	if (!chld) {
		printc("1\n");
		goto done;
	}
	chld_name = FS_ALLOC(len + 1);
	if (!chld_name) {
		printc("2\n");
		goto free1;
	}

	memcpy(chld_name, name, len);
	chld_name[len] = '\0';

	if (fsobj_cons(chld, parent, chld_name, t, 0, NULL)) {
		printc("3\n");
		goto free2;
	}
done:
	return chld;
free2:
	FS_FREE(chld_name);
free1:
	FS_FREE(chld);
	chld = NULL;
	goto done;
}

static void
fsobj_take(struct fsobj *o)
{
	assert(o->refcnt);
	o->refcnt++;
}

static inline struct fsobj *
fsobj_find_child(char *name, char *name_end, struct fsobj *dir)
{
	struct fsobj *sibling, *first;
	int           len;

	assert(dir && name);
	assert(dir->type == FSOBJ_DIR);
	if (!dir->child) return NULL;

	if (name_end)
		len = name_end - name;
	else
		len = strlen(name);
	first = sibling = dir->child;
	do {
		if (!strncmp(sibling->name, name, len)) return sibling;
		sibling = FIRST_LIST(sibling, next, prev);
	} while (sibling != first);

	return NULL;
}

/*
 * Remove this object, and any children from the tree. You _must_ pass
 * in the parent, unless this is a root node, and it is assumed that
 * you currently have access to it.  This is to abide by a lock
 * hierarchy.  You should hold any locks to parent before calling this
 * so that we avoid deadlock.
 *
 * Does not deallocation, and does not touch the removed subtree.
 */
static inline void
fsobj_rem(struct fsobj *o, struct fsobj *parent)
{
	assert(o);
	assert(parent || !o->parent);

	if (parent && parent->child == o) {
		struct fsobj *sibling = FIRST_LIST(o, next, prev);

		if (EMPTY_LIST(sibling, next, prev))
			parent->child = NULL;
		else
			parent->child = sibling;
	}
	REM_LIST(o, next, prev);
	o->parent = NULL;

	return;
}

/*
 * Deallocate the memory...should be disconnected at this point from
 * all all fsobjs.  This should almost never be called directly.
 * Instead use fsobj_release in conjunction with fsobj_take.
 */
static void
fsobj_free(struct fsobj *o)
{
	assert(o && o->name);
	assert(!o->parent && !o->child);
	FS_FREE(o->name);
	if (o->data) FS_DATA_FREE(o->data);
	FS_FREE(o);
}

static inline void __fsobj_free_hier(struct fsobj *o);

static void
fsobj_release(struct fsobj *o)
{
	assert(o->refcnt);
	o->refcnt--;
	if (!o->refcnt) {
		/* free the subtree if we are the only reference */
		if (o->child) __fsobj_free_hier(o);
		fsobj_free(o);
	}
}

/*
 * Free an entire tree: detach from current tree, and free the
 * memory. Assumption: o->parent == NULL.
 */
static inline void
__fsobj_free_hier(struct fsobj *o)
{
	struct fsobj *next;

	assert(!o->parent);
	/*
	 * This is complicated because we avoid recursion, yet
	 * linearize a hierarchy.
	 */
	next = o->child;
	while (o->child) {
		struct fsobj *curr = next;

		assert(curr);
		/* find a leaf, or a obj that should persist. */
		while (curr->child && curr->refcnt >= 1) {
			curr = curr->child;
		}
		next = FIRST_LIST(curr, next, prev);
		/* no siblings, go back to parent */
		if (EMPTY_LIST(curr, next, prev)) {
			next = curr->parent;
			assert(next);
		}
		fsobj_rem(curr, curr->parent);
		fsobj_release(curr);
	}
	fsobj_rem(o, NULL);
}

static inline void
fsobj_free_hier(struct fsobj *o)
{
	__fsobj_free_hier(o);
	fsobj_release(o);
}

/*
 * Takes absolute paths (starting at the "root" argument...which can
 * actually be a subdir), starting either with / or not; assumes \0
 * terminated string, that will be overwritten (/ -> \0).  Return the
 * path's parent in "parent", and the pointer to the subpath that
 * failed in subpath.
 */
static struct fsobj *
fsobj_path2obj(char *path, int len, struct fsobj *root, struct fsobj **parent, char **subpath)
{
	char *        next;
	struct fsobj *dir = root;

	assert(path && root && len >= 0);

	*parent = NULL;
	do {
		while (*path == '/') path++;
		*subpath = path;
		if (*path == '\0') break;

		next = strchr(path, '/');
		/* next == NULL means there is no next entry */
		*parent = dir;
		dir     = fsobj_find_child(path, next, dir);
		if (next) next++;
		if (!dir) return NULL;

		path = next;
	} while (path);

	return dir;
}

/*
 * Iterate through a directory's children.  If child == NULL, return
 * the first child of dir.  Otherwise, return the next one after
 * child.  Return NULL when we have returned all of the children.
 *
 * When a child is returned, note that it is still part of the list of
 * children.  As with pretty much any iterator, deleting a child, and
 * then asking for the next child by passing it in will always result
 * in NULL.  Point: be careful when deleting while iterating.  The
 * most common solution is to get the next, and then delete the
 * previous.
 */
static struct fsobj *
fsobj_dir_next(struct fsobj *dir, struct fsobj *child)
{
	struct fsobj *next;

	assert(dir);

	if (!child) return dir->child;
	/* something strange has happened since last iteration */
	if (child->parent != dir) return NULL;

	next = FIRST_LIST(child, next, prev);
	/* done going through children */
	if (next == dir->child) return NULL;

	return next;
}

#endif /* FS_H */
