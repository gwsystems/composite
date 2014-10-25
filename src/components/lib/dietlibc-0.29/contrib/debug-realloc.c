#include <unistd.h>
#include <sys/mman.h>

static struct malloced {
  unsigned long len,maplen;
  struct malloced* next;
}* root=0;

void* malloc(size_t size) {
  char* n,* m;
  unsigned long s=size+sizeof(struct malloced);
  s=(s+4096+4095)&~4095;
  n=mmap(0,s,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if (n==MAP_FAILED) return 0;
  m=n-size;  while (m<n) m+=4096;
//  mprotect(m,size+sizeof(struct malloced),PROT_READ|PROT_WRITE);
  mprotect(n,((unsigned long)m&4095)+sizeof(struct malloced)+(size&~4095),PROT_READ|PROT_WRITE);
  ((struct malloced*)n)->len=size;
  ((struct malloced*)n)->maplen=s;
  ((struct malloced*)n)->next=root;
  root=(struct malloced*)n;
  return m;
}

void free(void* ptr) {
  struct malloced** x=&root;
  while (*x) {
    if (((char*)(*x))+sizeof(struct malloced)<(char*)ptr && 
	((char*)(*x))+4096>(char*)ptr) {
      struct malloced* old=*x;
      *x=(*x)->next;
      mprotect(old,old->maplen,PROT_NONE);
      return;
    }
    x=&(*x)->next;
  }
  abort();
}

void *realloc(void *ptr, size_t size) {
  unsigned long oldsize=0;
  struct malloced** x=&root;
  char* fnord;
  if (ptr) {
    while (*x) {
      if (((char*)(*x))+sizeof(struct malloced)<(char*)ptr && 
	  ((char*)(*x))+4096>(char*)ptr) {
	oldsize=(*x)->len;
	break;
      }
      x=&(*x)->next;
    }
  }
  fnord=malloc(size);
  memcpy(fnord,ptr,size>oldsize?oldsize:size);
  if (oldsize) free(ptr);
  return fnord;
}
