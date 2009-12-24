#include <sys/cdefs.h>
#include <stdlib.h>
#include <assert.h>

/* comments:
     1. insertion sort sofort, nicht nachträglich
     2. threshold = 16
 */

static inline void iswap(void *a,void *b,size_t size) {
  register char *x=a;
  register char *y=b;
  register char *z=x+size;
  while (x<z) {
    register char tmp=*x;
    *x=*y;
    *y=tmp;
    ++x; ++y;
  }
}

static inline void swap(void *base,size_t size,size_t a,size_t b) {
  iswap((char*)base+a*size,(char*)base+b*size,size);
}

#if 0
extern int array[];

void dumparray() {
  printf("array now {%d,%d,%d,%d,%d}\n",array[0],array[1],array[2],array[3],array[4]);
}
#endif

void isort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void isort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
  size_t i;
  while (__likely(nmemb>1)) {
    char *min=base;
    char *tmp=min+size;
    for (i=1; i<nmemb; ++i) {
      if (__unlikely(compar(tmp,min)<0))
	min=tmp;
      tmp+=size;
    }
    iswap(min,base,size);
    base=(void*)((char*)base+size);
    nmemb-=1;
  }
}

#if OLD_AND_SLOW_FOR_MAKE
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
#ifdef DEBUG
  char *dbase=base;
  char *dmax=base+(nmemb-1)*size;
  char dmemb=nmemb;
#endif
//  static int level=0;
  char* v;	/* pivot */
  char* mid, *max, *min;
  size_t lmemb;

#if 0
  int left,right;
  left=(int*)base-array;
  right=left+nmemb-1;
  ++level;
  { int i; for (i=0; i<level; ++i) printf("  "); }
  printf("qsort: level %d; base=%p, %dx%d; array[%d..%d]\n",level,base,nmemb,size,left,right);
  assert(left>=0 && right<=1000);
#endif
  if (nmemb<=8) {
//    --level;
    return isort(base,nmemb,size,compar);
  }
  {
    mid=(char*)base+(nmemb/2)*size;
    max=(char*)base+(nmemb-1)*size;

    if (compar(base,max)<0)	/* a[left] < a[right] */
      if (compar(base,mid)<0)	/* a[left] < a[med] */
	if (compar(max,mid)<0)	/* a[left] < a[right] < a[med] */
	  v=max;
	else			/* a[left] < a[med] < a[right] */
	  v=mid;
      else			/* a[med] < a[left] < a[right] */
	v=base;
    else			/* a[right] < a[left] */
      if (compar(base,mid)<0)	/* a[right] < a[left] < a[med] */
	v=base;
      else			/* a[right] < a[left] && a[med] < a[left] */
	if (compar(max,mid)<0)	/* a[right] < a[med] < a[left] */
	  v=mid;
	else
	  v=max;
//    printf("%d %d %d -> median %d\n",*(int*)base,*(int*)mid,*(int*)max,*(int*)v);
  }
  if (v != max)
    iswap(v,max,size);
  v=max;
  min=base; lmemb=0;
  for (;;) {
    while (__likely(compar(min,v)<0)) { min+=size; ++lmemb; }
    while (__likely(compar(max-=size,v)>0)) ;
    if (min>=max) break;
    iswap(min,max,size);
  }
  iswap(min,v,size);
#ifdef DEBUG
//    { int i; for (i=0; i<level; ++i) printf("  "); }
//    printf("-=< base=%p, min=%p, nmemb=%d, lmemb=%d (%d)\n",base,min,nmemb,lmemb,(min-(char*)base)/size);
    assert(lmemb==((min-(char*)base)/size));
#endif
  if (min>(char*)base+size) {
#ifdef DEBUG
    assert(base==dbase);
#endif
//    { int i; for (i=0; i<level; ++i) printf("  "); }
//    printf("+-left %d [%d..%d] of [%d..%d]\n",level+1,left,left+lmemb,left,right);
    qsort(base,lmemb,size,compar);
  }
  if (nmemb>lmemb+1) {
//    { int i; for (i=0; i<level; ++i) printf("  "); }
//    printf("+-right %d [%d..%d] of [%d..%d]\n",level+1,left+lmemb,right,left,right);
    qsort(min+size,nmemb-lmemb-1,size,compar);
  }
//  --level;
}
#else

static inline char* idx(void* base,size_t size,size_t x) {
  return ((char*)base)+x*size;
}

static void Qsort(void *base, size_t nmemb, size_t size,long l,long r,
		  int (*compar)(const void *, const void *)) {
  long i,j,k,p,q;
  char* v;
  if (r-l<10) {
    isort(idx(base,size,l),r-l+1,size,compar);
    return;
  }
  v=idx(base,size,r);
  i=l-1; j=r; p=l-1; q=r;
  for (;;) {
    while (compar(idx(base,size,++i),v)<0) ;
    while (compar(idx(base,size,--j),v)>0) if (j==l) break;
    if (i>=j) break;
    swap(base,size,i,j);
    if (compar(idx(base,size,i),v)==0) { ++p; swap(base,size,p,i); }
    if (compar(idx(base,size,j),v)==0) { --q; swap(base,size,q,j); }
  }
  swap(base,size,i,r); j=i-1; ++i;
  for (k=l; k<p; ++k,--j) swap(base,size,k,j);
  for (k=r-1; k>q; --k,++i) swap(base,size,k,i);
  if (j>l) Qsort(base,nmemb,size,l,j,compar);
  if (r>i) Qsort(base,nmemb,size,i,r,compar);
}

void qsort(void *base, size_t nmemb, size_t size,
	   int (*compar)(const void *, const void *)) {
  Qsort(base,nmemb,size,0,nmemb-1,compar);
}
#endif
