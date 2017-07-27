/* 
 * Memory management routines shamelessly ripped from dietlibc.  A
 * non-contiguous, multiple-free-list worst case n/2 internal
 * fragmentation allocator (for sizes > 16 bytes).  
 *
 * Ported to the Composite OS, and lock-free synchronization added by
 * Gabriel Parmer, gparmer@gwu.edu.
 *
 * stats reporting and debugging added by gparmer@gwu.edu on 3/26/09
 */

/*
 * malloc/free by O.Dreesen
 *
 * first TRY:
 *   lists w/magics
 * and now the second TRY
 *   let the kernel map all the stuff (if there is something to do)
 */

#include <mem_mgr_config.h>
#include "cbuf.h"
#include "printc.h"

#include <cos_component.h>
#include <cos_alloc.h>
#ifdef ALLOC_DEBUG
#define COS_FMT_PRINT
#include <print.h>
int alloc_debug = 0;
#endif
#include <string.h>

#ifdef USE_VALLOC
#include <valloc.h>
#endif

struct free_page {
	struct free_page *next;
};
static struct free_page page_list = {.next = NULL};

#define DIE() (*((int*)0) = 0xDEADDEAD)
#define massert(prop) do { if (!(prop)) DIE(); } while (0)

/* -- HELPER CODE --------------------------------------------------------- */

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)0)  //((void*)-1)
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct {
  cbuf_t cbuf_id;
  void*  next;
  size_t size;
} __alloc_t;

#define BLOCK_START(b)	(((void*)(b))-sizeof(__alloc_t))
#define BLOCK_RET(b)	(((void*)(b))+sizeof(__alloc_t))

#define MEM_BLOCK_SIZE	PAGE_SIZE
#define PAGE_ALIGN(s)	round_up_to_page(s)

#ifdef NIL
#define REGPARM(x) __attribute__((regparm(x)))
#else
#define REGPARM(x)
#endif

#ifdef USE_VALLOC
void *cos_get_vas_page(void)
{
	return valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
}

void cos_release_vas_page(void *p)
{
	valloc_free(cos_spd_id(), cos_spd_id(), p, 1);
}
#endif

/* -- SMALL MEM ----------------------------------------------------------- */

static __alloc_t* __small_mem[8];

#define __SMALL_NR(i)		(MEM_BLOCK_SIZE/(i))

#define __MIN_SMALL_SIZE	__SMALL_NR(256)		/*   16 /   32 */
#define __MAX_SMALL_SIZE	__SMALL_NR(2)		/* 2048 / 4096 */

#define GET_SIZE(s)		(__MIN_SMALL_SIZE<<get_index((s)))

#define FIRST_SMALL(p)		(((unsigned long)(p))&(~(MEM_BLOCK_SIZE-1)))

static inline int __ind_shift() { return (MEM_BLOCK_SIZE==4096)?4:5; }

static size_t REGPARM(1) get_index(size_t _size) {
	size_t idx, size;
	for (idx = 0, size = ((_size-1)&(MEM_BLOCK_SIZE-1))>>__ind_shift() ;  
	     size ; 
	     size>>=1, ++idx );
	return idx;
}

#if ALLOC_DEBUG >= ALLOC_DEBUG_STATS
struct mem_stats {
	unsigned long long alloc, free;
};
typedef enum { DBG_ALLOC, DBG_FREE } alloc_dbg_type_t;
static struct mem_stats __mem_stats[8];

void alloc_stats_print(void)
{
	int i;

	for (i = 0 ; i < 8 ; i++) {
		unsigned long long min;

		printc("cos_alloc: bin %d, alloc %lld, free %lld", 
		       i, __mem_stats[i].alloc, __mem_stats[i].free);
		min = (__mem_stats[i].alloc < __mem_stats[i].free) ? 
			__mem_stats[i].alloc : 
			__mem_stats[i].free;
		__mem_stats[i].free  -= min;
		__mem_stats[i].alloc -= min;
	}
}

static void alloc_stats_report(alloc_dbg_type_t type, int bin)
{
	switch (type) {
	case DBG_ALLOC:
		__mem_stats[bin].alloc++;
		break;
	case DBG_FREE:
		__mem_stats[bin].free++;
		break;
	}
}
#else
#define alloc_stats_report(t, b)
#endif

/* small mem */
static void __small_free(void*_ptr,size_t _size) REGPARM(2);

static inline void REGPARM(2) __small_free(void*_ptr,size_t _size) {
	__alloc_t* ptr=BLOCK_START(_ptr), *prev;
	size_t size=_size;
	size_t idx=get_index(size);
	
//	memset(ptr,0,size);	/* allways zero out small mem */
#if ALLOC_DEBUG >= ALLOC_DEBUG_ALL
	if (alloc_debug) printc("free (in %d): freeing %p of size %d and index %d.", 
				cos_spd_id(), ptr, size, idx);
#endif
	do {
		prev = __small_mem[idx];
		ptr->next=prev;
	} while (unlikely(cos_cmpxchg(&__small_mem[idx], (int)prev, (int)ptr) != (int)ptr));
	alloc_stats_report(DBG_FREE, idx);
}

static void* do_cbuf_alloc(size_t size, cbuf_t *cbid)
{
    if (!size) { return NULL; }

    return cbuf_alloc(size, cbid);
}

static inline void* REGPARM(1) __small_malloc(size_t _size) {
	__alloc_t *ptr, *next;
	size_t size=_size;
	size_t idx;

	idx=get_index(size);
	do {
		ptr=__small_mem[idx];
#if ALLOC_DEBUG >= ALLOC_DEBUG_ALL
		if (alloc_debug) printc("malloc (in %d): head of list for size %d (idx %d) is %x", 
					cos_spd_id(), _size, idx, __small_mem[idx]);
#endif
		if (unlikely(ptr==0))  {	/* no free blocks ? */
			register int i,nr;
			__alloc_t *start, *second, *end;
		
			cbuf_t cbid;	
			start = ptr = do_cbuf_alloc(MEM_BLOCK_SIZE, &cbid);
			if (ptr==MAP_FAILED) return MAP_FAILED;
			ptr->cbuf_id = cbid;

			nr=__SMALL_NR(size)-1;
			for (i=0;i<nr;i++) {
				ptr->next=(((void*)ptr)+size);
				ptr=ptr->next;
			}
			end = ptr;
			end->next=0;
			/* Make malloc thread-safe with lock-free sync: */
			second = start->next;
			start->next = 0;
			do {
				ptr = __small_mem[idx];
				/* Hook a possibly existing list to
				 * the end of our new list */
				end->next = ptr;
			} while (unlikely(cos_cmpxchg(&__small_mem[idx], (long)ptr, (long)second) != (long)second));
#if ALLOC_DEBUG >= ALLOC_DEBUG_ALL
			if (alloc_debug) printc("malloc (in %d): returning memory region @ %x, size %d, index %d, (head of list is %x)", 
						cos_spd_id(), ptr, size, idx, __small_mem[idx]);
#endif
			return start;
		} 
		next = ptr->next;
	} while (unlikely(cos_cmpxchg(&__small_mem[idx], (long)ptr, (long)next) != (long)next));
	ptr->next=0;

#if ALLOC_DEBUG >= ALLOC_DEBUG_ALL
	if (alloc_debug) printc("malloc (in %d): returning memory region @ %x, size %d, index %d, (head of list is %x)", 
				cos_spd_id(), ptr, size, idx, __small_mem[idx]);
#endif
	/* 
	 * FIXME: This still suffers from the ABA problem -- still a
	 * race if ptr and next are removed from list, then ptr put
	 * back in between ptr=__small_mem[idx] and the cmpxchg.  next
	 * is used elsewhere, but we are still going to put it back at
	 * the head of the list.  This can be solved with another
	 * cos_cmpxchg loop to verify next as well.
	 */
	alloc_stats_report(DBG_ALLOC, idx);

	return ptr;
}

/* -- PUBLIC FUNCTIONS ---------------------------------------------------- */

static void do_cbuf_free(cbuf_t cbid)
{
    cbuf_free(cbid);
}

static void _alloc_libc_free(void *ptr) 
{
	if (ptr) {
		__alloc_t *pointer = BLOCK_START(ptr);
		size_t size = pointer->size;

		assert(size);
		if (size <= __MAX_SMALL_SIZE) {
			__small_free(ptr, size);
		} else {
			cbuf_free(pointer->cbuf_id);
		}
	}
}

void free(void *ptr) __attribute__((weak,alias("_alloc_libc_free")));

#ifdef WANT_MALLOC_ZERO
static __alloc_t zeromem[2];
#endif

static void* _alloc_libc_malloc(size_t size) {
  __alloc_t* ptr;
  size_t need;
#ifdef WANT_MALLOC_ZERO
  if (!size) return BLOCK_RET(zeromem);
#else
  if (!size) goto err_out;
#endif
  size+=sizeof(__alloc_t);
  if (unlikely(size<sizeof(__alloc_t))) goto err_out;

  if (size<=__MAX_SMALL_SIZE) {
    need=GET_SIZE(size);
    ptr=__small_malloc(need);
  } 
  else {
    need=PAGE_ALIGN(size);
    cbuf_t cbid;
    ptr = need ? do_cbuf_alloc(need, &cbid) : MAP_FAILED;
    ptr->cbuf_id = cbid;
  }
  if (ptr==MAP_FAILED) goto err_out;
  ptr->size=need;
  return BLOCK_RET(ptr);
err_out:
  return 0;
}

void* malloc(size_t size) __attribute__((alias("_alloc_libc_malloc")));

void *__libc_calloc(size_t nmemb, size_t _size)
{
	size_t tot = nmemb*_size;
	char *ret = malloc(tot);
	
	memset(ret, 0, tot);
	return ret;
}

void* calloc(size_t nmemb, size_t _size) __attribute__((weak,alias("__libc_calloc")));

/* gabep1 additions for allocations of pages. */

void *alloc_page(void)
{
	struct free_page *fp;
	void *a;

	fp = page_list.next;
	if (NULL == fp) {
		cbuf_t cbid;
		a = do_cbuf_alloc(PAGE_SIZE, &cbid);
	} 
	else {
		page_list.next = fp->next;
		fp->next = NULL;
		a = (void*)fp;
	}
	
	return a;
}

void free_page(void *ptr)
{
	struct free_page *fp;
	
	fp = (struct free_page *)ptr;
	fp->next = page_list.next;
	page_list.next = fp;

	return;
}

static void* _libc_realloc(void* ptr, size_t size) {
	if (ptr && size) {
		void* tmp = _alloc_libc_malloc(size);

		if (!tmp) goto err_out;

		memcpy(tmp, ptr, size);
		_alloc_libc_free(ptr);

		return tmp;
	}
	else if (ptr && !size) {
		_alloc_libc_free(ptr);
		ptr = NULL;
		return ptr;
	}
	else if (!ptr && size) {
		ptr = _alloc_libc_malloc(size);
		return ptr;
	}
	else {
		ptr = _alloc_libc_malloc(size);
		return ptr;
	}

err_out:
	return NULL;
}
void* realloc(void* ptr, size_t size) __attribute__((weak,alias("_libc_realloc")));
