#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

typedef unsigned char      u8_t;
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
#define unlikely(x) __builtin_expect(!!(x), 0)
#define LINUX_TEST
#include <kvtrie.h>
#define CAPTBL_ALLOCFN ct_alloc
#include <captbl.h>
#include <pgtbl.h>

#define NTESTS (4096)

struct pair {
	long  id;
	void *val;
};

int
in_pairs(struct pair *ps, int len, long id)
{
	for (; len >= 0; len--) {
		if (ps[len].id == id) return 1;
	}
	return 0;
}

int alloc_cnt = 0;
#include <sys/mman.h>
static void *
unit_allocfn(void *d, int sz, int last_lvl)
{
	int * mem = d;
	void *r;
	(void)d;
	(void)last_lvl;
	alloc_cnt++;
	if (mem) {
		assert(*mem == 0);
		*mem = 1;
	}
	// return malloc(sz);
	r = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return r; // mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static void
unit_freefn(void *d, void *m, int sz, int last_lvl)
{
	(void)d;
	(void)last_lvl;
	(void)sz;
	alloc_cnt--;
	// free(m);
	munmap(m, sz);
}

KVT_CREATE(unit, 2, 12, 8, unit_allocfn, unit_freefn);
KVT_CREATE(unit2, 3, 6, 12, unit_allocfn, unit_freefn);
KVT_CREATE(unit3, 4, 5, 5, unit_allocfn, unit_freefn);

ERT_CREATE_DEF(unit4, 3, 8, 4, 64, unit_allocfn);
ERT_CREATE_DEF(unit5, 1, 0, 14, 32, unit_allocfn);

/*
 * I separate this out so that we can easily confirm that the compiler
 * is doing the proper optimizations.
 */
void *
do_lookups(struct pair *ps, struct unit3_ert *v)
{
	void *        r;
	unsigned long id = ps->id;
	assert(v);
	assert(id < unit3_maxid());
	__asm__("nop; nop; nop");
	r = unit3_lkupp(v, id);
	__asm__("nop; nop; nop");
	return r;
}

void *
do_addr_lookups(struct pair *ps, struct unit4_ert *v)
{
	// if (unlikely((unsigned long)ps->id >= unit_maxid())) return NULL;
	int *p = unit4_lkup(v, ps->id);
	return (void *)*p;
}

int
do_add(struct pair *ps, struct unit_ert *v, void *d)
{
	return unit_add(v, ps->id, d);
}

typedef struct ert *(*alloc_fn_t)(void *memctxt);
typedef void (*free_fn_t)(struct ert *v);
typedef void *(*lkupp_fn_t)(struct ert *v, unsigned long id);
typedef int (*add_fn_t)(struct ert *v, long id, void *val);
typedef int (*del_fn_t)(struct ert *v, long id);
typedef void *(*lkup_fn_t)(struct ert *v, unsigned long id);
typedef void *(*lkupa_fn_t)(struct ert *v, unsigned long id, unsigned long *agg);
typedef void *(*lkupan_fn_t)(struct ert *v, unsigned long id, int dlimit, unsigned long *agg);
typedef int (*expandn_fn_t)(struct ert *v, unsigned long id, u32_t dlimit, unsigned long *accum, void *memctxt);
typedef int (*expand_fn_t)(struct ert *v, unsigned long id, unsigned long *accum, void *memctxt);

void
kv_test(int max, alloc_fn_t a, free_fn_t f, lkupp_fn_t lp, add_fn_t add, del_fn_t d, lkup_fn_t l, lkupa_fn_t la,
        lkupan_fn_t lan, expandn_fn_t en, expand_fn_t e)
{
	(void)l;
	(void)la;
	(void)lan;
	(void)en;
	(void)e;
	struct pair pairs[NTESTS];
	int         i;
	struct ert *dyn_vect;

	dyn_vect = (struct ert *)a(NULL);
	assert(dyn_vect);
	for (i = 0; i < NTESTS; i++) {
		do {
			pairs[i].id = rand() % max;
		} while (in_pairs(pairs, i - 1, pairs[i].id));
		pairs[i].val                  = malloc(10);
		*(unsigned int *)pairs[i].val = 0xDEADBEEF;
		assert(!add(dyn_vect, pairs[i].id, pairs[i].val));
		assert(lp(dyn_vect, pairs[i].id) == pairs[i].val);
	}
	for (i = 0; i < NTESTS; i++) {
		assert(lp(dyn_vect, pairs[i].id) == pairs[i].val);
	}
	for (i = 0; i < NTESTS; i++) {
		free(pairs[i].val);
		assert(!d(dyn_vect, pairs[i].id));
		pairs[i].id  = 0;
		pairs[i].val = NULL;
	}
	f(dyn_vect);
}

void
ert_test(int max, int depth, alloc_fn_t a, lkup_fn_t l, lkupa_fn_t la, lkupan_fn_t lan, expandn_fn_t en, expand_fn_t e)
{
	(void)l;
	(void)la;
	(void)lan;
	(void)e;
	struct pair   pairs[NTESTS];
	int           i;
	unsigned long mem;
	struct ert *  v;
	int           testnum = NTESTS;

	if (testnum > max) testnum = max;
	mem = 0;
	v   = (struct ert *)a(&mem);
	assert(v);
	assert(mem);
	for (i = 0; i < testnum; i++) {
		int           j, c;
		unsigned long accum;
		int *         val;

		do {
			pairs[i].id = rand() % max;
		} while (in_pairs(pairs, i - 1, pairs[i].id));
		val = l(v, pairs[i].id);
		assert(!val || !*val);
		for (j = 2; j <= depth; j++) {
			c   = alloc_cnt;
			mem = 0;
			assert(!en(v, pairs[i].id, j, &accum, &mem));
			assert(lan(v, pairs[i].id, j, &accum));
			if (j < depth && mem) {
				assert(!lan(v, pairs[i].id, j + 1, &accum));
			}
			assert(alloc_cnt == (mem ? c + 1 : c));
		}
		assert((val = l(v, pairs[i].id)));
		assert(*val == 0); /* init to NULL */
		*val = pairs[i].id;
		assert(l(v, pairs[i].id));
	}
}

void *
do_captbllkups(struct captbl *ct, unsigned long id)
{
	void *r;

	__asm__("nop; nop; nop");
	r = captbl_lkup(ct, id);
	__asm__("nop; nop; nop");
	return r;
}

void
ct_test(void)
{
	struct captbl *    ct;
	char *             p  = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	char *             p1 = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	int                ret;
	struct cap_header *c;

	assert(p);
	ct = captbl_alloc(p);
	assert(ct);
	captbl_init(p + (PAGE_SIZE / 2), 1);
	ret = captbl_expand(ct, 0, captbl_maxdepth(), p + (PAGE_SIZE / 2));
	assert(!ret);
	c = captbl_lkup(ct, 0);
	assert(!c);
	c = captbl_add(ct, 0, CAP_THD, &ret);
	assert(c && ret == 0 && c == (void *)(p + (PAGE_SIZE / 2)));
	c++;
	*(int *)c = 1;
	c         = captbl_lkup(ct, 0);
	assert(c && c == (void *)(p + (PAGE_SIZE / 2)));
	c++;
	assert(*(int *)c == 1);
	c = captbl_add(ct, 1, CAP_SINV, &ret);
	assert(!c && ret != 0);
	ret = captbl_del(ct, 0);
	assert(!ret);
	assert(!captbl_lkup(ct, 0));
	c = captbl_add(ct, 2, CAP_SINV, &ret);
	assert(c && ret == 0);
	assert(c == captbl_lkup(ct, 2));
	c = captbl_add(ct, 0, CAP_SINV, &ret);
	assert(c && ret == 0);
	assert(c == captbl_lkup(ct, 0));
	c = captbl_add(ct, 1, CAP_THD, &ret);
	assert(!c && ret != 0);

	/* test another cache line in the same last-level lookup table */
	c = captbl_add(ct, 4, CAP_THD, &ret);
	assert(c && ret == 0);
	assert(c == captbl_lkup(ct, 4));
	c = captbl_add(ct, 4, CAP_THD, &ret);
	assert(!c && ret != 0);
	c = captbl_add(ct, 6, CAP_SINV, &ret);
	assert(!c && ret != 0);
	ret = captbl_del(ct, 4);
	assert(!ret);
	assert(!captbl_lkup(ct, 4));
	assert(!captbl_lkup(ct, 6));
	c = captbl_add(ct, 6, CAP_SINV, &ret);
	assert(c && ret == 0);
	assert(c == captbl_lkup(ct, 6));
	c++;
	*(int *)c = 1;
	c         = captbl_lkup(ct, 6);
	assert(c);
	c++;
	assert(*(int *)c == 1);
	c = captbl_add(ct, 4, CAP_SINV, &ret);
	assert(c && ret == 0);

	/* test upper-level lookup failure */
	c = captbl_add(ct, 1 << 9, CAP_SINV, &ret);
	assert(!c && ret != 0);
	c = captbl_add(ct, 1 << 30, CAP_SINV, &ret);
	assert(!c && ret != 0);
	captbl_init(p1, 1);
	ret = captbl_expand(ct, (1 << 9) + 20, captbl_maxdepth(), p1);
	assert(!ret);
	c = captbl_add(ct, 1 << 9, CAP_SINV, &ret);
	assert(c && ret == 0);
	c = captbl_lkup(ct, 1 << 9);
	assert(c);

	/* test pruning, and re-expansion */
	assert(p1 == captbl_prune(ct, (1 << 9) + 20, 1, &ret));
	c = captbl_lkup(ct, 1 << 9);
	assert(!c);
	ret = captbl_expand(ct, (1 << 9) + 20, captbl_maxdepth(), p1);
	assert(!ret);
	c = captbl_lkup(ct, 1 << 9);
	assert(c);

	captbl_init(p1 + (PAGE_SIZE / 2), 1);
	ret = captbl_expand(ct, (1 << 9) * 4 + 3, captbl_maxdepth(), p1 + (PAGE_SIZE / 2));
	assert(!ret);
	c = captbl_add(ct, (1 << 9) * 4 + 60, CAP_SINV, &ret);
	assert(c && ret == 0);
	assert(c == captbl_lkup(ct, (1 << 9) * 4 + 60));
	ret = captbl_del(ct, (1 << 9) * 4 + 60);
	assert(!ret);
	assert(c);
}

void
pgt_test(void)
{
	char *  p1 = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	char *  p2 = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	char *  p3 = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	pgtbl_t pt;
	u32_t   flags = 0;

	pt = pgtbl_alloc(p1);
	assert(pt);

	pgtbl_init_pte(p2);
	/* In the real system, we want to memcpy the appropriate kernel page entries into the page table here. */
	assert(!pgtbl_intern_expand(pt, (void *)(1 << 24), p2, PGTBL_INTERN_DEF));
	assert(
	  !pgtbl_mapping_add(pt, (void *)((1 << 24)), (void *)0xADEAD000, PGTBL_PRESENT | PGTBL_USER | PGTBL_WRITABLE));
	flags = 0;
	assert((void *)0xADEAD000 == pgtbl_translate(pt, (void *)((1 << 24)), &flags));
	assert(flags == (PGTBL_PRESENT | PGTBL_USER | PGTBL_WRITABLE));

	/* detect conflicts? */
	pgtbl_init_pte(p3);
	assert(pgtbl_intern_expand(pt, (void *)(1 << 24), p3, PGTBL_INTERN_DEF));
	assert(pgtbl_mapping_add(pt, (void *)((1 << 24)), (void *)0xBDEAD000, PGTBL_PRESENT | PGTBL_USER));
	flags = 0;
	assert((void *)0xADEAD000 == pgtbl_translate(pt, (void *)((1 << 24)), &flags));
	assert(flags == (PGTBL_PRESENT | PGTBL_USER | PGTBL_WRITABLE));

	/* add second entry with the other "present" flags value */
	assert(!pgtbl_translate(pt, (void *)((1 << 27)), &flags));
	assert(!pgtbl_intern_expand(pt, (void *)(1 << 27), p3, PGTBL_INTERN_DEF));
	assert(!pgtbl_mapping_add(pt, (void *)((1 << 27)), (void *)0xCDEAD000, PGTBL_COSFRAME));
	flags = 0;
	assert((void *)0xCDEAD000 == pgtbl_translate(pt, (void *)((1 << 27)), &flags));
	assert(flags == PGTBL_COSFRAME);

	/* remove mappings? */
	assert(pgtbl_intern_prune(pt, (void *)(1 << 25)));
	assert(!pgtbl_mapping_del(pt, (void *)(1 << 27)));
	flags = 0;
	assert(NULL == pgtbl_translate(pt, (void *)((1 << 27)), &flags));
	assert(flags == 0);
	assert(p3 == pgtbl_intern_prune(pt, (void *)(1 << 27)));
	assert(!pgtbl_translate(pt, (void *)((1 << 27)), &flags));

	/* move mapping */
	assert(p2 == pgtbl_intern_prune(pt, (void *)(1 << 24)));
	assert(!pgtbl_translate(pt, (void *)((1 << 24)), &flags));
	assert(!pgtbl_intern_expand(pt, (void *)(1 << 26), p2, PGTBL_INTERN_DEF));
	flags = 0;
	assert((void *)0xADEAD000 == pgtbl_translate(pt, (void *)((1 << 26)), &flags));
	assert(flags == (PGTBL_PRESENT | PGTBL_USER | PGTBL_WRITABLE));

	assert(!pgtbl_mapping_del(pt, (void *)(1 << 26)));
	assert(NULL == pgtbl_translate(pt, (void *)((1 << 26)), &flags));
	assert(p2 == pgtbl_intern_prune(pt, (void *)(1 << 26)));
	assert(NULL == pgtbl_translate(pt, (void *)((1 << 26)), &flags));
}

int
main(void)
{
	printf("key-value tests:\n");
	kv_test(unit_maxid(), (alloc_fn_t)unit_alloc, (free_fn_t)unit_free, (lkupp_fn_t)unit_lkupp, (add_fn_t)unit_add,
	        (del_fn_t)unit_del, (lkup_fn_t)unit_lkup, (lkupa_fn_t)unit_lkupa, (lkupan_fn_t)unit_lkupan,
	        (expandn_fn_t)unit_expandn, (expand_fn_t)unit_expand);
	kv_test(unit2_maxid(), (alloc_fn_t)unit2_alloc, (free_fn_t)unit2_free, (lkupp_fn_t)unit2_lkupp,
	        (add_fn_t)unit2_add, (del_fn_t)unit2_del, (lkup_fn_t)unit2_lkup, (lkupa_fn_t)unit2_lkupa,
	        (lkupan_fn_t)unit2_lkupan, (expandn_fn_t)unit2_expandn, (expand_fn_t)unit2_expand);
	kv_test(unit3_maxid(), (alloc_fn_t)unit3_alloc, (free_fn_t)unit3_free, (lkupp_fn_t)unit3_lkupp,
	        (add_fn_t)unit3_add, (del_fn_t)unit3_del, (lkup_fn_t)unit3_lkup, (lkupa_fn_t)unit3_lkupa,
	        (lkupan_fn_t)unit3_lkupan, (expandn_fn_t)unit3_expandn, (expand_fn_t)unit3_expand);
	assert(alloc_cnt == 0);
	printf("\tSUCCESS\n");

	printf("ert tests:\n");
	ert_test(unit4_maxid(), 3, (alloc_fn_t)unit4_alloc, (lkup_fn_t)unit4_lkup, (lkupa_fn_t)unit4_lkupa,
	         (lkupan_fn_t)unit4_lkupan, (expandn_fn_t)unit4_expandn, (expand_fn_t)unit4_expand);
	ert_test(unit5_maxid(), 1, (alloc_fn_t)unit5_alloc, (lkup_fn_t)unit5_lkup, (lkupa_fn_t)unit5_lkupa,
	         (lkupan_fn_t)unit5_lkupan, (expandn_fn_t)unit5_expandn, (expand_fn_t)unit5_expand);
	printf("\tSUCCESS\n");

	printf("captbl tests:\n");
	ct_test();
	printf("\tSUCCESS\n");

	printf("pgtbl tests:\n");
	pgt_test();
	printf("\tSUCCESS\n");

	return 0;
}
