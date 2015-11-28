/***
 * Copyright 2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Authors: Gabriel Parmer, gparmer@gwu.edu, 2015
 */

#ifndef PS_NS_H
#define PS_NS_H

/***
 * This file is nearly entirely glue.  Glue embedded radix tries with
 * slabs and smr.  This file represents the glue code at the
 * highest-levels.  The ps_ns.c file contains the glue code between
 * the slab allocator and the underlying system providing memory.
 * This enables sharing and movement of pieces of the namespace
 * between cores, and also manages the allocation/deallocation of the
 * ertrie.
 */

#include <ps_smr.h>
#include <errno.h>
#include <ertrie.h>

struct ps_ns {
	struct ps_mem m;
};

struct ps_slab *ps_slab_nsalloc(struct ps_mem *m, size_t sz, coreid_t coreid);
void ps_slab_nsfree(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid);
void ps_ns_init(struct ps_mem *m, void *ert, ps_lkupan_fn_t lkup, ps_expand_fn_t expand, size_t depth, ps_desc_t maxid, size_t range);

static inline int
__ps_ns_desc_isfree(void *slot) 
{ return __ps_mhead_isfree(__ps_mhead_get(slot)); }

#define __PS_NS_TYPE_CREATE(name, type, objsz, nobjord, depth, maxid)					\
__PS_PARSLAB_CREATE_AFNS(name, (ps_rndpow2(__ps_slab_objmemsz((objsz)))-sizeof(struct ps_mheader)),	\
			 (ps_rndpow2(__ps_slab_objmemsz((objsz))) * (1<<nobjord)), 0,			\
			 ps_slab_nsalloc, ps_slab_nsfree)						\
static inline ps_desc_t											\
ps_ns_desc_##name(void *slot)										\
{ return __ps_mhead_get(slot)->slab->start + ps_slab_objoff_##name(slot); }				\
static inline void *											\
ps_nsptr_lkup_##name(struct ps_ns *ns, ps_desc_t desc)							\
{													\
	struct ps_mheader *h = name##_lkup(ns->m.ns_info.ert, desc);					\
	if (unlikely(!h)) return NULL;									\
	return __ps_mhead_mem(h);									\
}													\
static inline void *											\
ps_nsptr_alloc_##name(struct ps_ns *ns, ps_desc_t *d)							\
{													\
	void *a = ps_##type##ptr_alloc_##name(&ns->m);							\
	if (unlikely(!a)) return NULL;									\
	*d = ps_ns_desc_##name(a);									\
													\
	return a;											\
}													\
static inline void *											\
ps_ns_alloc_##name(ps_desc_t *d)									\
{ return ps_nsptr_alloc_##name((struct ps_ns *)&__ps_mem_##name, d); }					\
static inline void											\
ps_nsptr_free_##name(struct ps_ns *ns, void *slot)							\
{ ps_##type##ptr_free_##name(&ns->m, slot); }								\
static inline void											\
ps_nsptr_freedesc_##name(struct ps_ns *ns, ps_desc_t d)							\
{													\
	void *m = ps_nsptr_lkup_##name(ns, d);								\
	if (m) ps_##type##ptr_free_##name(&ns->m, m);							\
}													\
static inline void											\
ps_ns_free_##name(void *slot)										\
{ ps_nsptr_free_##name((struct ps_ns *)&__ps_mem_##name, slot); }					\
static inline void											\
ps_ns_init_##name(struct parsec *ps, void *ert)								\
{													\
	ps_mem_init_##name(ps);													\
	ps_ns_init(&__ps_mem_##name, ert, (ps_lkupan_fn_t)name##_lkupan, 								\
		   (ps_expand_fn_t)name##_expandn, depth, maxid, 1<<nobjord);							\
}													\
static inline void											\
ps_ns_init_slab_##name(void *ert)									\
{													\
	ps_slab_init_##name();										\
	ps_ns_init(&__ps_mem_##name, ert, (ps_lkupan_fn_t)name##_lkupan,				\
		   (ps_expand_fn_t)name##_expandn, depth, maxid, 1<<nobjord);				\
}													\
static inline int											\
ps_nsptr_delete_##name(struct ps_ns *ns)								\
{ /* TODO: deallocate the ert */ return ps_memptr_delete_##name(&ns->m); }				\
static inline struct ps_ns *										\
ps_nsptr_create_##name(struct parsec *ps)								\
{													\
	struct ps_mem *m;										\
	struct ps_ns_ert_##name *e;									\
	if (ps) m = ps_memptr_create_##name(ps);							\
	else	m = ps_slabptr_create_##name();								\
	if (!m) return NULL;										\
	e = name##_alloc(NULL);										\
	if (!e) ps_memptr_delete_##name(m);								\
	ps_ns_init(m, e, (ps_lkupan_fn_t)name##_lkupan,							\
		   (ps_expand_fn_t)name##_expandn, depth, maxid, 1<<nobjord);				\
	return (struct ps_ns *)m;									\
}													\
static inline struct ps_ns *										\
ps_nsptr_create_slab_##name(void)									\
{ return ps_nsptr_create_##name(NULL); }

/* 
 * The ert functions when the namespace itself is SMRed, and the nodes
 * within it are interned into the structure of the leaf of the radix
 * trie.
 */
static inline void *
__ps_ns_alloc_intern(void *c, int sz, int isleaf)
{ 
	if (isleaf) return c; 	                /* passed in the slab */
	return ps_plat_alloc(sz, ps_coreid());  /* internal node */
}
static inline int
__ps_ns_setleaf_intern(struct ert_intern *e, void *data)
{ (void)e; (void)data; assert(0); return 0; }
static inline void *
__ps_ns_getleaf_intern(struct ert_intern *e, void *accum)
{ (void)e; (void)accum; assert(0); return NULL; }
static inline int
__ps_ns_resolve_intern(struct ert_intern *a, void *accum, int leaf, u32_t order, u32_t size)
{
	if (!leaf) return ert_defresolve(a, accum, leaf, order, size);
	return !__ps_mhead_isfree((void*)a);
}
static inline void 
__ps_ns_init_intern(struct ert_intern *a, int leaf)
{ 
	if (!leaf) ert_definit(a, leaf);
	return; 		/* already initialized by the slab allocator... */
}


#define PS_NS_CREATE(name, nodesz, depth, order, leaforder)								\
ERT_CREATE(name, ps_ns_ert_##name, depth, order, PS_WORD, leaforder,							\
	   ps_rndpow2(__ps_slab_objmemsz(nodesz)), NULL, __ps_ns_init_intern, ert_defget, ert_defisnull, ert_defset,	\
	   __ps_ns_alloc_intern, __ps_ns_setleaf_intern, __ps_ns_getleaf_intern, __ps_ns_resolve_intern)		\
__PS_NS_TYPE_CREATE(name, mem, nodesz, leaforder, depth, name##_maxid())

#define PS_NSSLAB_CREATE(name, nodesz, depth, order, leaforder)								\
ERT_CREATE(name, ps_ns_ert_##name, depth, order, PS_WORD, leaforder,							\
	   ps_rndpow2(__ps_slab_objmemsz(nodesz)), NULL, __ps_ns_init_intern, ert_defget, ert_defisnull, ert_defset,	\
	   __ps_ns_alloc_intern, __ps_ns_setleaf_intern, __ps_ns_getleaf_intern, __ps_ns_resolve_intern)		\
__PS_NS_TYPE_CREATE(name, slab, nodesz, leaforder, depth, name##_maxid())

#endif	/* PS_NS_H */
