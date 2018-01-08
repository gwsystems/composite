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

//#define UNIX_TEST
#ifdef UNIX_TEST
#define PAGE_SIZE 4096
#include <sys/mman.h>
#else
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

extern void *mman_get_page(spdid_t spd, void *addr, int flags);
extern void mman_release_page(spdid_t spd, void *addr, int flags);
#endif

#define DIE() (*((int*)0) = 0xDEADDEAD)
#define massert(prop) do { if (!(prop)) DIE(); } while (0)

/* -- HELPER CODE --------------------------------------------------------- */

#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct {
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

#ifdef UNIX_TEST
static inline REGPARM(1) void *do_mmap(size_t size) { 
	return mmap(0, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, (size_t)0); 
}
/*static inline*/ REGPARM(2) int do_munmap(void *addr, size_t size) {
	return munmap(addr, size);
}
#else 

static inline REGPARM(1) void *do_mmap(size_t size) {
	void *hp, *ret;
	unsigned long p;
	size_t s = round_up_to_page(size);

#ifdef USE_VALLOC
	hp = valloc_alloc(cos_spd_id(), cos_spd_id(), s/PAGE_SIZE);
	if (!hp) return MAP_FAILED;
#else
	massert(size <= PAGE_SIZE);
	//hp = cos_get_prealloc_page();
	//if (!hp) 
	hp = cos_get_vas_page();
#endif
	for (p = (unsigned long)hp ; 
	     p < (unsigned long)hp + s ; 
	     p += PAGE_SIZE) {
		ret = (void*)mman_get_page(cos_spd_id(), (void*)p, MAPPING_RW);
		if (unlikely(!ret)) {
			for (p -= PAGE_SIZE ; hp <= (void*)p ; p -= PAGE_SIZE) {
				mman_release_page(cos_spd_id(), (void*)p, 0);
			}
#ifdef USE_VALLOC
			if (unlikely(valloc_free(cos_spd_id(), cos_spd_id(), hp, s/PAGE_SIZE))) DIE();
#else
			cos_release_vas_page(hp);
#endif
			return MAP_FAILED;
		}
	}
#if ALLOC_DEBUG >= ALLOC_DEBUG_ALL
	if (alloc_debug) printc("malloc in %d: mmapped region into %x", cos_spd_id(), ret);
#endif
	return hp;
}

/* remove qualifiers to make debugging easier */
/*static inline*/ REGPARM(2) int do_munmap(void *addr, size_t size) {
#ifndef USE_VALLOC
	  /* not supported and given that we can't allocate > PAGE_SIZE, this should never happen */
	  DIE();
#else
	  unsigned long p;

	  massert((unsigned long)addr == round_to_page((unsigned long)addr)); 
	  for (p = (unsigned long)addr ; p < ((unsigned long)addr + size) ; p += PAGE_SIZE) {
		  mman_release_page(cos_spd_id(), (void*)p, 0);
	  }
	  if (valloc_free(cos_spd_id(), cos_spd_id(), addr, round_up_to_page(size)/PAGE_SIZE)) DIE();
#endif
  return 0;
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
			
			start = ptr = do_mmap(MEM_BLOCK_SIZE);
			if (ptr==MAP_FAILED) return MAP_FAILED;

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
		//__small_mem[idx]=ptr->next;
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

static void _alloc_libc_free(void *ptr) {
  register size_t size;
  if (ptr) {
    size=((__alloc_t*)BLOCK_START(ptr))->size;
    if (size) {
      if (size<=__MAX_SMALL_SIZE)
	__small_free(ptr,size);
      else
	do_munmap(BLOCK_START(ptr),size);
    }
  }
}
//void __libc_free(void *ptr) __attribute__((alias("_alloc_libc_free")));
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
  } else {
    need=PAGE_ALIGN(size);
    ptr = need ? do_mmap(need) : MAP_FAILED;
  }
  if (ptr==MAP_FAILED) goto err_out;
  ptr->size=need;
  return BLOCK_RET(ptr);
err_out:
  //(*__errno_location())=ENOMEM;
  return 0;
}
//void* __libc_malloc(size_t size) __attribute__((alias("_alloc_libc_malloc")));
void* malloc(size_t size) __attribute__((weak,alias("_alloc_libc_malloc")));

void *__libc_calloc(size_t nmemb, size_t _size)
{
	size_t tot = nmemb*_size;
	char *ret  = malloc(tot);
	
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
		a = do_mmap(PAGE_SIZE);
	} else {
		page_list.next = fp->next;
		fp->next       = NULL;
		a              = (void*)fp;
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

/* end gabep1 additions */


#ifdef NIL

void* __libc_calloc(size_t nmemb, size_t _size);
void* __libc_calloc(size_t nmemb, size_t _size) {
  register size_t size=_size*nmemb;
  if (nmemb && size/nmemb!=_size) {
    (*__errno_location())=ENOMEM;
    return 0;
  }
  return malloc(size);
}
void* calloc(size_t nmemb, size_t _size) __attribute__((weak,alias("__libc_calloc")));

void* __libc_realloc(void* ptr, size_t _size);
void* __libc_realloc(void* ptr, size_t _size) {
  register size_t size=_size;
  if (ptr) {
    if (size) {
      __alloc_t* tmp=BLOCK_START(ptr);
      size+=sizeof(__alloc_t);
      if (size<sizeof(__alloc_t)) goto retzero;
      size=(size<=__MAX_SMALL_SIZE)?GET_SIZE(size):PAGE_ALIGN(size);
      if (tmp->size!=size) {
	if ((tmp->size<=__MAX_SMALL_SIZE)) {
	  void *new=_alloc_libc_malloc(_size);
	  if (new) {
	    register __alloc_t* foo=BLOCK_START(new);
	    size=foo->size;
	    if (size>tmp->size) size=tmp->size;
	    if (size) memcpy(new,ptr,size-sizeof(__alloc_t));
	    _alloc_libc_free(ptr);
	  }
	  ptr=new;
	}
	else {
	  register __alloc_t* foo;
	  size=PAGE_ALIGN(size);
	  foo=mremap(tmp,tmp->size,size,MREMAP_MAYMOVE);
	  if (foo==MAP_FAILED) {
retzero:
	    (*__errno_location())=ENOMEM;
	    ptr=0;
	  }
	  else {
	    foo->size=size;
	    ptr=BLOCK_RET(foo);
	  }
	}
      }
    }
    else { /* size==0 */
      _alloc_libc_free(ptr);
      ptr = NULL;
    }
  }
  else { /* ptr==0 */
    if (size) {
      ptr=_alloc_libc_malloc(size);
    }
  }
  return ptr;
}
void* realloc(void* ptr, size_t size) __attribute__((weak,alias("__libc_realloc")));

#endif

/* void */
/* __alloc_libc_initilize(void) */
/* { */
/* 	vaddr_t start, end; */
/* 	unsigned int extent; */
/* 	int i; */

/* 	start = round_to_pgd_page((unsigned long)&start); */
/* 	end = (vaddr_t)cos_get_heap_ptr(); */
/* 	extent = (end-start)/PAGE_SIZE; */

/* 	__alloc_page_map.next = NULL; */
/* 	__alloc_page_map.start_addr = (void*)start; */
/* 	__alloc_page_map.nused = extent; */
/* 	for (i = 0 ; i < extent ; i++) { */
/* 		bitmap_set(&__alloc_page_map.page_used[0], i); */
/* 	} */
/* } */

/********************* testing code on unix ***********************/

#ifdef UNIX_TEST

#define ITER 100000
#define PTRS_LIVE 100
#define SIZE_LB 4
#define SIZE_UB 8192
#define SIZE_GET ((rand() % (SIZE_UB-SIZE_LB)) + SIZE_LB)

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

int find_ptr(long *ptrs, int sz, int empty)
{
	int i; 

	for (i = 0 ; i < sz ; i++) {
		if (empty && ptrs[i] == 0) return i;
		else if (!empty && ptrs[i] != 0) return i;
	}

	return -1;
}

void malloc_rand(long *ptrs, int sz)
{
	int idx = find_ptr(ptrs, sz, 1);
	long *ptr;
	int asz;

	if (idx < 0) printf("Hmm, index errors when mallocing.\n");

	ptr = &ptrs[idx];

	asz = SIZE_GET;
	*ptr = (long)malloc(asz);
	if (*ptr == 0) {
		printf("could not malloc!\n");
	}

	*(int*)*ptr = asz;
	memset((char*)(*ptr)+sizeof(int), rand() % 256, asz-sizeof(int));
	printf("+");
	
	return;
}

void free_rand(long *ptrs, int sz)
{
	int i, idx = find_ptr(ptrs, sz, 0), asz;
	long *ptr;
	char c, *arr;

	if (idx < 0) printf("Hmm, index errors when freeing.\n");

	ptr = &ptrs[idx];
	asz = *(int*)(*ptr);
	arr = ((char*)(*ptr)) + sizeof(int);
	c = *arr;

	for (i = 0 ; i < asz-sizeof(int) ; i++) {
		if (((char *)arr)[i] != c) {
			printf("we have a problem.\n");
			break;
		}
	}

	free((void*)*ptr);
	*ptr = 0;
	printf("-");

	return;
}

int main(void)
{
	long ptrs[PTRS_LIVE] = {0, };
	int i, live = 0;

	memset(ptrs, 0, sizeof(long) * PTRS_LIVE);
	srand(time(NULL));

	for (i = 0 ; i < ITER ; i++) {
		int alloc;

		switch (live) {
		case 0:
			alloc = 1;
			break;
		case PTRS_LIVE:
			alloc = 0;
			break;
		default:
			if (rand() % 2) {
				alloc = 1;
			} else {
				alloc = 0;
			}
		}

		if (alloc) {
			malloc_rand(ptrs, PTRS_LIVE);
			live++;
		} else {
			free_rand(ptrs, PTRS_LIVE);
			live--;
		}
	}
	return 0; 
}

#endif
