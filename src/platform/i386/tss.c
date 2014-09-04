/* Based on code from Pintos. See LICENSE.pintos for licensing information */

#include "tss.h"
#include "gdt.h"

/* from pintos vaddr.h, for thread_current... */
#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

/* Page offset (bits 0:12). */
#define PGSHIFT 0                          /* Index of first offset bit. */
#define PGBITS  12                         /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)              /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS)   /* Page offset bits (0:12). */

static inline void *pg_round_down (const void *va) {
  return (void *) ((u32_t) va & ~PGMASK);
}


/* from pintos threads.c, for thread_current() to work... */
static inline void *
running_thread (void)
{
  u32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

static inline void *
thread_current (void)
{
  void *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  //ASSERT (is_thread (t));
  //ASSERT (t->status == THREAD_RUNNING);

  return t;
}


/* Kernel TSS. */
static struct tss tss;

/* Initializes the kernel TSS. */
void
tss__init (void) 
{
  /* Our TSS is never used in a call gate or task gate, so only a
     few fields of it are ever referenced, and those are the only
     ones we initialize. */
  //tss = palloc_get_page (PAL_ASSERT | PAL_ZERO);
  tss.ss0 = SEL_KDSEG;
  tss.bitmap = 0xdfff;
  tss_update ();
}

/* Returns the kernel TSS. */
struct tss *
tss_get (void) 
{
  //ASSERT (tss != NULL);
  return &tss;
}

/* Sets the ring 0 stack pointer in the TSS to point to the end
   of the thread stack. */
void
tss_update (void) 
{
  //ASSERT (tss != NULL);
  tss.esp0 = (u8_t *) thread_current () + PGSIZE;
}
