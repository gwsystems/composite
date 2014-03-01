/* Based on code from Pintos. See LICENSE.pintos for licensing information */

#ifndef USERPROG_TSS_H
#define USERPROG_TSS_H

#include "shared/cos_types.h"

/* The Task-State Segment (TSS).

   Instances of the TSS, an x86-specific structure, are used to
   define "tasks", a form of support for multitasking built right
   into the processor.  However, for various reasons including
   portability, speed, and flexibility, most x86 OSes almost
   completely ignore the TSS.  We are no exception.

   Unfortunately, there is one thing that can only be done using
   a TSS: stack switching for interrupts that occur in user mode.
   When an interrupt occurs in user mode (ring 3), the processor
   consults the ss0 and esp0 members of the current TSS to
   determine the stack to use for handling the interrupt.  Thus,
   we must create a TSS and initialize at least these fields, and
   this is precisely what this file does.

   When an interrupt is handled by an interrupt or trap gate
   (which applies to all interrupts we handle), an x86 processor
   works like this:

     - If the code interrupted by the interrupt is in the same
       ring as the interrupt handler, then no stack switch takes
       place.  This is the case for interrupts that happen when
       we're running in the kernel.  The contents of the TSS are
       irrelevant for this case.

     - If the interrupted code is in a different ring from the
       handler, then the processor switches to the stack
       specified in the TSS for the new ring.  This is the case
       for interrupts that happen when we're in user space.  It's
       important that we switch to a stack that's not already in
       use, to avoid corruption.  Because we're running in user
       space, we know that the current process's kernel stack is
       not in use, so we can always use that.  Thus, when the
       scheduler switches threads, it also changes the TSS's
       stack pointer to point to the new thread's kernel stack.
       (The call is in thread_schedule_tail() in thread.c.)

   See [IA32-v3a] 6.2.1 "Task-State Segment (TSS)" for a
   description of the TSS.  See [IA32-v3a] 5.12.1 "Exception- or
   Interrupt-Handler Procedures" for a description of when and
   how stack switching occurs during an interrupt. */
struct tss
  {
    u16_t back_link, :16;
    void *esp0;                         /* Ring 0 stack virtual address. */
    u16_t ss0, :16;                  /* Ring 0 stack segment selector. */
    void *esp1;
    u16_t ss1, :16;
    void *esp2;
    u16_t ss2, :16;
    u32_t cr3;
    void (*eip) (void);
    u32_t eflags;
    u32_t eax, ecx, edx, ebx;
    u32_t esp, ebp, esi, edi;
    u16_t es, :16;
    u16_t cs, :16;
    u16_t ss, :16;
    u16_t ds, :16;
    u16_t fs, :16;
    u16_t gs, :16;
    u16_t ldt, :16;
    u16_t trace, bitmap;
  };


struct tss;
void tss__init (void);
struct tss *tss_get (void);
void tss_update (void);

#endif
