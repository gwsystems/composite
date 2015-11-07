#ifndef PS_NS_H
#define PS_NS_H

#include <ps_smr.h>
#include <errno.h>
/* #include <ertrie.h> */

struct ps_ns {
	struct ps_mem m;
};

struct ps_slab *ps_slab_nsalloc(struct ps_mem *m, size_t sz, coreid_t coreid);
void ps_slab_nsfree(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid);
void ps_ns_init(struct ps_mem *m, ps_desc_t maxid, size_t range);

static inline int
__ps_ns_desc_isfree(void *slot) 
{ return __ps_mhead_isfree(__ps_mhead_get(slot)); }

#define __PS_NS_TYPE_CREATE(name, type, objsz, nobjord)							\
__PS_PARSLAB_CREATE_AFNS(name, (ps_rndpow2(__ps_slab_objmemsz((objsz)))-sizeof(struct ps_mheader)),	\
			 (ps_rndpow2(__ps_slab_objmemsz((objsz))) * (1<<nobjord)), 0,			\
			 ps_slab_nsalloc, ps_slab_nsfree)						\
static inline ps_desc_t											\
ps_ns_desc_##name(void *slot)										\
{ return __ps_mhead_get(slot)->slab->start + ps_slab_objoff_##name(slot); }				\
static inline void *											\
ps_nsptr_alloc_##name(struct ps_ns *ns, ps_desc_t *d)							\
{													\
	void *a = ps_##type##ptr_alloc_##name(&ns->m);							\
	if (unlikely(!a)) return NULL;									\
	*d = ps_ns_desc_##name(a);									\
	return a;											\
}													\
static inline void *											\
ps_ns_alloc_##name(ps_desc_t *d)									\
{ return ps_nsptr_alloc_##name((struct ps_ns *)&__ps_mem_##name, d); }					\
static inline void											\
ps_nsptr_free_##name(struct ps_ns *ns, void *slot)							\
{ ps_##type##ptr_free_##name(&ns->m, slot); }								\
static inline void											\
ps_ns_free_##name(void *slot) { ps_nsptr_free_##name((struct ps_ns *)&__ps_mem_##name, slot); }		\
static inline void											\
ps_ns_init_##name(struct parsec *ps)									\
{													\
	ps_mem_init_##name(ps);										\
	ps_ns_init(&__ps_mem_##name, 1024*1024 /*name_maxid()*/, 1<<nobjord);				\
}													\
static inline void											\
ps_ns_init_slab_##name(void)										\
{													\
	ps_slab_init_##name();										\
	ps_ns_init(&__ps_mem_##name, 1024*1024 /*name_maxid()*/, 1<<nobjord);				\
}													\
static inline struct ps_ns *										\
ps_nsptr_create_##name(struct parsec *ps)								\
{													\
	struct ps_mem *m;										\
	if (ps) m = ps_memptr_create_##name(ps);							\
	else	m = ps_slabptr_create_##name();								\
	if (m) ps_ns_init(m, 1024*1024 /*name_maxid()*/, 1<<nobjord);					\
	return (struct ps_ns *)m;									\
}													\
static inline struct ps_ns *										\
ps_nsptr_create_slab_##name(void)									\
{ return ps_nsptr_create_##name(NULL); }								\
static inline void											\
ps_nsptr_delete_##name(struct ps_ns *ns)								\
{ ps_memptr_delete_##name(&ns->m); }


#define PS_NS_PARSLAB_CREATE(name, objsz, nobjord) __PS_NS_TYPE_CREATE(name, mem, objsz, nobjord)
#define PS_NS_SLAB_CREATE(name, objsz, nobjord)    __PS_NS_TYPE_CREATE(name, slab, objsz, nobjord)

/* #define PS_NS_CREATE(name, leafsz, leaforder, depth, internorder) ERT_CREATE_DEF(name, 3, 1024, , ) */

/* #ifdef NIL */
/* #define PS_NS_CREATE(name, sz, allocsz)					\ */
/* ERT_CREATE(name, ps_ns, 3, PS_PAGE_SIZE/PS_WORD, PS_WORD,	        \ */
/* 	   allocsz/PS_RNDPOW2(__ps_slab_objmemsz(sz))),			\ */
/* 	   PS_RNDPOW2(__ps_slab_objmemsz(sz)), NULL,			\ */
/* 	   ert_definit, ert_defget, ert_defisnull, ert_defset, allocfn, ert_defsetleaf, ert_defgetleaf, ert_defresolve) \ */
/* PS_SLAB_CREATE(name, PS_RNDPOW2(__ps_slab_objmemsz(sz)) - sizeof(struct ps_mheader), allocsz, 0) */
/* #endif */

/* void */
/* __ps_ns_free(struct ps_ns *ns, ps_desc_t id) */
/* { */
	
/* } */

#endif	/* PS_NS_H */
