_Composite_ Developer Manual v0.1 by Gabriel Parmer (`gparmer@gwu.edu`)
=======================================================================
  
  _Composite_ is in a constant state of flux.  This manual might be out
  of date.  Specific functions might not exist -- or might have
  changed, but the ideas will probably map across versions.  Please
  see the other files in the doc/ directory for the specifics
  regarding which files to create and modify to make a component or
  interface, how the build system is set up, what the directory
  structure is, how to execute the system, or how to debug.  This file
  should *only* be used once you understand these documents, and want
  to sit down and program!

  - Document Organization: This document is separated into three
    general sections: 

    1. Library functions that you can use in your program.  Note that
       they might not be as functional as their libc equivalents.
    2. The basic components that make up the current _Composite_
       ecosystem.  These provide basic services, and can be used by
       your component.
    3. _Composite_ kernel system calls.  These are not typically used
       in many programs, and are instead abstracted either by a
       library or another component.  However, for lower-level
       programming, or to understand existing code, we document the
       system call layer.

  - Please keep in mind that this documentation was written quickly
    (as it always is), so /any documentation bug-fixes would be
    appreciated/, and that /this documentation is not meant to be a
    replacement for code inspection/.  This documentation provides
    high-level context so that you can see a little better how things
    fit together.  Use the information in here, to grep through the
    source code to find examples of how the different APIs are used,
    and how these APIs are implemented.
  
Component Library Functions
---------------------------

   - `long cos_spd_id(void)` - retrieve the current spd identifier
     (i.e. component id).
   - `long cos_get_thd_id(void)` - retrieve the current thread
     identifier
   - `void *cos_get_heap_ptr(void)` - this should not be commonly
     used and is instead used internally by the heap management
     functions (malloc/free/get_free_page)
   - `void cos_set_heap_ptr(void *addr)` - mutator for the pointer
     to the end of the heap.  Again, this is not commonly used in
     component code.
   - `long cos_cmpxchg(volatile void *memory, long
                        anticipated, long result)` - A uni-processor
     compare and exchange implementation using restartable atomic
     sequences.  The memory location (`memory`), if set to
     `anticipated` is changed to `result`, atomically.  It returns the
     value of the memory after the atomic operation.

   - `void *cos_argreg_alloc(int sz)` - Currently, to pass arguments
     to other components, the "argument region" must be used.  This is
     essentially a page that travels with a thread as it executes
     between multiple components.  This function is used to allocate a
     region of size `sz` in that region.  Often, one will want to
     allocate the size of the data you want to pass plus
     `sizeof(struct cos_array)` (see below).  This results in a
     bounded array.
   - `int cos_argreg_free(void *p)` - Frees an "argument region"
     allocation.  The argument region must be allocated from and freed
     from as a stack.  If multiple allocations are made, they must be
     freed in the reverse order that they are allocated.
   - `int cos_argreg_arr_intern(struct cos_array *ca)`

     ```
     struct cos_array {
	int sz;
	char mem[0];
     }; 
     ```

     This function returns true or false depending on if the
     `struct cos_array` is allocated from within the "argument
     region".

   - `void *malloc(int sz)` - A malloc implementation that is
     thread-safe, but does _not_ handle allocations larger than a
     page.  There is not a technical limitation preventing allocations
     larger than a page, but a more intelligent allocator must be
     implemented.
   - `void *alloc_page(void)`, `void free_page(void *ptr)` - allocate
     and free a single page.

     
Component <-> Component Interface:
----------------------------------
  
   Components communicate with each other via invocations.  A
   component that is dependent on the other can make an invocation of
   a function in the exported interface of the other component.  That
   is to say, components can call functions exported in the interface
   for other components.  Here we discuss the foundational components
   that provide essential services in _Composite_.  We go from the
   higher-level ones most likely to be used, to the lower-level ones
   that provide more primitive services.  The interfaces for each of
   these components can be seen in the `.h` files in the
   subdirectories of `src/components/interface/`.

   A general pattern for many components is that the component
   provides some abstraction or object whose functionality can be
   harnessed via the component's API.  To create such an object
   (e.g. a lock, an event, a network connection, all of which we will
   call resources), or release one, many component interfaces provide
   the following functions

   + `open`, `create`, or `alloc` to create the resource, and return
     an integer identifier for it (similar to a file-descriptor, and
   + `close`, `release`, or `free` to release it.  In the future,

   these names should be unified to say `create` and `release`.
   `release` is more appropriate than `free` as the resources are
   often reference counted.

   Once a resource is allocated, it can be operated on.  The API for
   this is different for each component.  For the lock component, for
   example, one can `take` and `release` the lock, and for the event
   component, one can `trigger` the event, or `wait` on an event.

   This style of API is a consistent theme in _Composite_, however it
   is not universal.

**Requesting Memory, and Printing**

  The component interface for getting memory is `mem_mgr`, and for
  printing, `printc`.  These should most often be used indirectly
  through the library calls for `malloc` / `free` and `printf`.

**Locking**

  The interface specification for the lock component is in
  `src/components/interface/lock/lock.h` and
  `src/components/include/cos_synchronization.h`.  The lock
  component is an odd one in that some functionality is loaded (via
  cos_synchronization.c) into the client component.  This enables
  the "fast path" of taking and releasing locks when there is no
  contention to be fast.

  The lock API (the one the library exposes) includes

  - `lock_static_init(cos_lock_t)` which initializes a lock that
    is statically allocated (and calls `lock_component_alloc`).
  - `lock_release(cos_lock_t)` which will release the lock in
    the lock component.

  Once a lock is created, one can simply `lock_take(cos_lock_t)`
  and `lock_release(cos_lock_t)` to take and release the lock (up
  and down a semaphore) to provide a critical section.

**Event notification**

  Often a thread will want to wait for an event to happen in another
  component.  This can vary from waiting for a packet to arrive in a
  networking component, or waiting for data to arrive on an
  asynchronous IPC channel.  This drives the need for the `select` and
  `poll` system calls in UNIX.  In general there are two components,
  one with a thread that wishes to wait for a number (multiple)
  events, and the other component that can trigger events. The event
  notification component provides this functionality.
    
  The API for the event notification component can be found in
  `src/components/interface/evt/evt.h`.  We'll discuss a subset of the
  API.  As with the lock component, there are `evt_create` and
  `evt_free` to create and remove an event.  Each event is referenced
  by a `long` value.  The component that will wait for the event
  usually creates the event, and passes the event identifier to the
  component that will trigger the event.

  The component in which the event is detected (because an interrupt
  delivers a packet) will call `evt_trigger` to "trigger" the event.
  At this point, any threads waiting on that event will wake up and be
  notified of the event's activity.

  The component executing the thread that wishes to wait for the event
  will call `evt_wait` to wait (block) waiting for any of its events
  to be triggered.  It will wait on /all/ of the events that have been
  created by this thread.  The API might be changed in the future to
  allow the user to change which set of events to wait on.  Though the
  API allows one to prioritize events, we do not discuss that API
  here.

**Networking**

  _Composite_ includes the LWIP networking stack as a component.  The
  API is distinctly lower-level than the socket API, but facilities
  are provided to send and receive packets.  The API should support
  both TCP and UDP, but we haven't tested the UDP in a couple of
  years.  It is doubtful it works.  The API can be viewed in
  `src/components/interface/net_transport/net_transport.h`.

  To create a connection, one has three options:

  - `net_connection_t net_create_tcp_connection(spdid_t spdid, u16_t tid, long evt_id);`
  - `net_connection_t net_create_udp_connection(spdid_t spdid, long evt_id);`
  - `net_connection_t net_accept(spdid_t spdid, net_connection_t nc);`

  As always, the `net_connection_t` is just an integer identifying the
  connection.  When creating a TCP connection, one passes in the
  thread id of the thread that can manipulate that connection.  This
  restriction will likely be lifted in the future, but for now, it
  must be passed in.  The accept call is similar to the similarly
  named call in UNIX.  The network connection identified by `nc` will
  wait for connection requests, and create a new connection identified
  by the returned connection.  This call is asynchronous, which means
  that a thread that calls it will not block waiting for the
  connection to be made.  If another connection cannot be made
  immediately, it will return `-EAGAIN`.

  The event id passed into these calls is the event that is to be
  triggered when an event happens on this connection.

  A `close` call will close the connection.

  To build a TCP connection, as in UNIX, one must `listen`, `bind`,
  and possibly `connect` the connection.  The connect call is an
  outlier in _Composite_ interface functions in that it will block
  until the actual connection is created.  This should be changed in
  the future.

  To actually use the connection by retrieving and sending data, one
  uses `net_send` and `net_recv`.  The `data` argument must be in the
  argument region (see the library functions above).  These are,
  again, asynchronous, so threads will not block on them if there is
  no data, or buffer space.  Instead `-EAGAIN` will be called.
    
**Time management**

  Two components provide the ability to keep time, and block waiting
  for certain amounts of time to elapse.  The simpler of the two is
  `timed_blk` (`src/components/interface/timed_blk/timed_blk.h`).
  Simply, one can call `timed_event_block` and pass in the amount of
  time (in jiffies) one wishes to block.  The return value is the
  amount of time spent blocked.  You'll be waked up no sooner than at
  the end of that period.  The second function is `timed_event_wakeup`
  which can be used to prematurely wake up the thread specified as an
  argument.

  The more complicated time keeping component (actually usually
  provided by the same component) is the periodic timer, provided by
  `src/components/interface/periodic_wake/periodic_wake.h`.  This API
  generally allows a thread to be woken up periodically.  The thread
  creates a periodic timer using `periodic_wake_create` which
  specifies the periodicity of the timer in jiffies.  This timer can
  be deleted using `periodic_wake_remove`.  `periodic_wake_wait` will
  cause the thread to block waiting for a periodic event.  It will be
  woken up either immediately if that event has triggered and this
  thread didn't wait on it before it triggered, or when the periodic
  amount of time passes.  Other functions in the API provide
  information about deadline misses, lateness, etc...

**Scheduling**

  This API should hopefully not be needed/used that often.  Most
  timing, blocking, and critical section services are provided by the
  time management components, the event notification component, and
  the locking components, respectively, described above.  The `sched`
  interface (`src/components/interface/sched/sched.h`) contains many
  functions.  I will assume that the previous components are used, and
  will describe functions in the scheduler API that provide orthogonal
  functionality.  These boil down to:

  - `unsigned int sched_tick_freq(void);` - Get the number of
    jiffies in a second.
  - `int sched_create_thread(spdid_t spdid, struct cos_array
    *data);` - Create a new thread.  The `data` argument is an array
    in the argument region (see library functions above) that
    contains a textual representation of the priority of the created
    thread.

**Shared Memory**
    
  The `cbuf` API (spelled out in `src/components/include/cbuf.h`)
  includes the facilities for shared memory between components.  The
  API centers around three functions:

  - `void *cbuf_alloc(int size, cbuf_t *cb)` - allocate a shared
    memory region within this component of at least size `size`.
    Each shared memory region has an identifier associated with it
    that is returned in `cb`.  This is the token passed to other
    components that they can use to map in the cbuf.
  - `void cbuf_free(void *buf)` - Free up the cbuf
    located at `buf` to be used by a later allocation.  There is no
    need to "cache" cbufs, as the cbuf subsystem does that for you.
  - `void *cbuf2buf(cbuf_t cb, int len)` - This function is used for
    a component that has been passed a `cbuf_t` id, to map that
    buffer into this component.  The `len` is the advertised length
    of the buffer, and is used by a consistency check preventing a
    component from passing you a cbuf with an incorrect length.

Component <-> Kernel Interface:
-------------------------------

   The system-call interface between component and kernel is detailed
   in components/include/cos_component.h.  We will discuss the
   different kernel system calls in the following sections.  It should
   be noted that unless you're writing a very "low level" component,
   you should probably _not_ be using these functions.  Instead
   another component probably provides what you want.  See the
   previous section on component to component interactions.

**Functions for creating and manipulating components and capabilities**

- `int cos_spd_cntl(short int op, short int spd_id, long arg1, long
   arg2);` -- `op` is taken from <shared/cos_types.h> and defines the
   function of this system call:

   + `COS_SPD_CREATE`: The other arguments don't matter.  Create a
      new component, and return its `spd_id`.
   + `COS_SPD_DELETE`: `spd_id` is the spd to delete.
   + `COS_SPD_RESERVE_CAPS`: `spd_id` is the spd to reserve a
     span of capabilities for, and `arg1` is the number of
     capabilities to reserve.  Capabilities can only be allocated
     once they are reserved, and they can only be reserved before
     any capability allocations are made.
   + `COS\SPD_RELEASE_CAPS`: `spd_id` is the spd to release
     the capability reservation for.  This will deallocate all
     capabilities and de-reserve them.
   + `COS_SPD_LOCATION`: Set the virtual address location of
     component `spd_id`.  Currently, this is limited to an
     aligned 4M region.  `arg1` is the base address, and `arg2`
     is the size of the allocation (currently on 4M is supported).
   + `COS_SPD_UCAP_TBL`: Set the address, `arg1`, of the user
     capability list in component `spd_id`, of size related to
     the reservation made previously.
   + `COS_SPD_ATOMIC_SECT`: Set the `arg2` th restartable
     atomic section for component `spd_id`.  The base of the RAS
     is `arg1`.
   + `COS_SPD_UPCALL_ADDR`: `arg1` is the address of the
     upcall function in component `spd_id`.
   + `COS_SPD_ACTIVATE`: _IMPORTANT_ - This should only be
     called after the component has been created, its
     capabilities reserved, its location set, and the location of
     its capability table, upcall address, and atomic sections
     set.  This will activate the component so that threads can
     execute into it.  This is the "commit" instruction.

- `long cos_cap_cntl_spds(spdid_t cspd, spdid_t sspd, long arg);`
  Return the number of invocations between component `cspd` and
  `sspd`, and reset the count.

- `long cos_cap_cntl(short int op, spdid_t cspd, u16_t capid, long arg);`
  `op` is taken from <shared/cos_types.h> and defines the
   function of this system call:

   + `COS_CAP_SET_CSTUB`: Set component `cspd`'s address for capability
     `capid`'s client stub to `arg`.
   + `COS_CAP_SET_SSTUB`: Set component `cspd`'s address for capability
     `capid`'s server stub to `arg`.
   + `COS_CAP_SET_SERV_FN`: Set component `cspd`'s address
     for capability `capid`'s client function to be invoked to
     `arg`.
   + `COS_CAP_SET_FAULT`: Set component `cspd`'s handler for
     fault number `arg` to the capability `capid`.  The page
     fault, for example, is fault number 0.  When a fault occurs
     in the component, it will cause an invocation of the
     associated capability and will call a function of the
     prototype `void fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)`.
   + `COS_CAP_ACTIVATE`: _IMPORTANT_ - the three functions must
     be set for each capability before it can be activated.
     Once it is activated, it can be invoked by a thread.  This
     is the "commit" instruction.

**Scheduler functions**

Only a scheduler can actually usefully use these functions.  

- `int create_thread(int a, int b, int c);` 
  Create a new thread with a thread id returned by this
       syscall.  Its initial registers (`bx`, `di`, and `si`) are set
        to `a`, `b`, and `c`.  It will begin executing in the
        component that invokes this syscall.

- `int upcall(int spd_id);`
  The current thread will terminate execution in this component,
        and will make an upcall into component `spd_id`.

- `int sched_cntl(int operation, int thd_id, long option);`
  `operation` determines the function of the syscall:

   + `COS_SCHED_EVT_REGION`: Set the event region
         (page-aligned) within the calling scheduler to the address
          `option`.
   + `COS_SCHED_THD_EVT`: Associate a specific entry (number
          `option`) in the event region for the calling scheduler to
          the thread `thd_id`.
   + `COS_SCHED_PROMOTE_CHLD`: Set the component specified in
          `option` to be a child scheduler under the current
          scheduler.  Errors if the component already has a parent, or
          if the maximum hierarchy depth has been reached.
   + `COS_SCHED_GRANT_SCHED`: Grant access of the child
          scheduler `option` to schedule the thread `thd_id`.  That
          thread must be schedulable by the current scheduler.  The
          root scheduler is automatically granted schedulability of
          all threads.
   + `COS_SCHED_REVOKE_SCHED`: Remove previously granted
          scheduling permission of thread `thd_id` to child scheduler
          `option`.
   + `COS_SCHED_BREAK_PREEMPTION_CHAIN`: Complicated.  Default
          brand activation specifies that if the thread executing on
          the brand's behalf waits for the next event, the system will
          automatically switch back to the preempted thread.  That is
          not always what you want (i.e. if a thread was woken up with
          a higher priority than the preempted thread).  This call
          will prevent the automatic switch to the preempted thread.
          Instead, the scheduler will be upcalled.

- `int cos_thd_cntl(short int op, short int thd_id, long arg1, long arg2);`
  `op` defines the specific behavior of this system call.  Most
        functionality is for accessing thread register or execution
        state.  General introspection facilities.

   + `COS_THD_INV_FRAME`: retrieve the invoked component for
          thread `thd_id` at the `arg1` th position in its invocation
          frame.
   + `COS_THD_INVFRM_IP`, `COS_THD_INVFRM_SP`: retrieve a
          preempted thread, `thd_id`'s instruction pointer and stack
          pointer of the invocation at position `arg1` in its
          invocation frame.
   + `COS_THD_GET_XX`, where `XX` is `{IP, SP, FP, 1, 2, 3, 4,
          5, 6}`: Get the instruction pointer, stack pointer, frame
          pointer, or one of the 6 general purpose registers from a
          preempted thread `thd_id`.  Return `0` if the thread is not
          preempted.  If `arg1` is `1`, it will access the fault
          registers.
   + `COS_THD_SET_XX`: Where `XX` is defined above.  Set the
          register for a preempted thread, `thd_id`, to `arg1` if
          `arg2` is 0, and for the fault registers if `arg2` is 1.
   + `COS_THD_STATUS`: return the status flags of thread
          `thd_id`.

- `int cos_switch_thread(unsigned short int thd_id, unsigned short int flags);`
  Only a scheduler can invoke this system call.  Additionally,
        the scheduler must have been granted scheduling permission to
        schedule the current thread and `thd_id`.  The specific
        behavior of this system call are dependent on the `flags`
        passed to it.

   + `0`: This is the common case.  The intention is to switch
          from the current thread to `thd_id`.  If that thread has
          been preempted, then this might involve switching between
          components.
   + `COS_SCHED_SYNC_BLOCK` and `COS_SCHED_SYNC_UNBLOCK`:
          This is the wait-free synchronization primitive provided by
          the _Composite_ kernel.  `BLOCK` means that the current
          thread is attempting to take the scheduler critical section,
          and the kernel should switch immediately to the thread
          holding the critical section.  `UNBLOCK` is called by that
          thread that just released the critical section (CS), and will
          immediately switch to the thread waiting for the CS.
   + `COS_SCHED_CHILD_EVT`: Used for hierarchical scheduling.
          When switching to the child scheduler's event thread, this
          flag is used to set the `pending_cevt` flag in the child
          scheduler.  This is used to avoid race conditions, and
          ensures that the child scheduler knows of this event.
   + `COS_SCHED_TAILCALL`: When a brand is activated and its
          corresponding thread executes, then finishes, it can upcall
          into the scheduler -- a notification of it finishing.  We
          want that thread to go back to waiting for additional brand
          activations, but it must also switch to another thread that
          can make progress.  This flag carries out this process.  1)
          switch to `thd_id`, and 2) set the current thread to
          waiting for brand activations again (or execute one
          immediately if some are pending).
   + `COS_SCHED_BRAND_WAIT`: Only called by the timer tick
          thread.  This is equivalent to the timer saying "I'm done,
          and wait to wait for the next brand activation...but please
          switch to `thd_id`".  This call and `TAILCALL` are
          encapsulated in `cos_sched_base.c`, so you shouldn't have to
          worry about them...unless you're hacking the scheduler.

- `int idle(void);`: Idle the system until an event arrives.  This
      can mean many things in a hijacked environment.

**Brand management and execution functions**

- `int cos_brand_cntl(int ops, unsigned short int bid, unsigned short int tid, spdid_t spdid);`
  The semantics of this call depend on the `ops` passed in.

   + `COS_BRAND_CREATE_HW` and `COS_BRAND_CREATE`: Create a
          brand associated with component `spdid`.  the `HW` specifier
          enables the brand to be wired to an interrupt source.
   + `COS_BRAND_ADD_THD`: Add a thread `tid` to a brand `bid`, so
          that when that brand is activated, that thread is executed.
          Multiple threads can be associated with a brand, and an
          arbitrary one of them -- that is not already active -- will
          be executed upon brand activation.

- `int cos_brand_upcall(short int thd_id, short int flags,
                              long arg1, long arg2);`
  Activate a brand `thd_id`, and pass the arguments `arg1` and
        `arg2` to the executed thread, unless there are no threads to
        execute in which case the arguments are silently dropped.
        Yes, silently dropped.  In the future, we will drop the
        ability to pass arguments.  This should be restricted so that
        any component _cannot_ activate any brand.  Certainly look for
        future changes.  This call might be deprecated completely so
        that interrupts can activate brands.

- `int brand_wait(int thdid);`
  The current thread attempts to wait for an activation on brand
        `thdid`.  This thread will block unless there is a pending
        activation.

- `int brand_wire(long thd_id, long option, long data);`
  Associate a specific brand, `thd_id` with a hardware
        interrupt source.  `option` can be either `COS_HW_TIMER` or
        `COS_HW_NET`.  In the case of wiring to the networking
        interrupts, `data` is the port being branded to.

- `int cos_buff_mgmt(unsigned short int op, void *addr,
                           unsigned short int len, short int thd_id);`
  Currently, _Composite_ does not interface with devices
        directly.  It uses Linux device drivers and simply attempts to
        get the data from the device as early as possible (e.g. after
        IP for networking).  A means is required to move the data from
        the device into components.  That is what this system call
        does.  See
        `src/components/implementation/net_if/linux_if/netif.c` for an
        example use.  The semantics of this system call is dependent
        on the value of `op`.

   + `COS_BM_XMIT_REGION`:  The _Composite_ kernel assumes
          that if it touches user-level regions, those regions must
          never fault.  This option sets up a page that is shared
          between kernel and component that the component can place
          data into, and the kernel can read it out of to transmit.
   + `COS_BM_XMIT`: This call actually does the transfer of
          data.  It parses the transmit region, gets pointers to
          disparate buffers to transmit, does a mapping between them
          and kernel addresses, and copies them to the kernel.  This,
          then, uses gather semantics.  This interface relies on the
          format of the transmit page.  Please see the associated code
          for that format (i.e. see `cos_net_xmit_headers` and
          `gather_item`).
   + `COS_BM_RECV_RING`: To transfer data from the kernel to
          the components, a ring buffer is set up.  This, again is a
          single page shared between component and kernel that points
          to other page buffers (scatter).  See `ring_buff.c` for more
          details.

**Mutable protection domains management**

Mutable Protection Domains (MPD) are a novel aspect of _Composite_,
but they do add complexity.  They enable protection domain boundaries
between components to be removed and created dynamically as the system
executes.  In conjunction with monitoring information about which
communication paths (capability invocations) between components are
most frequent, this enables the system to maximize reliability while
meeting performance/predictability constraints.

For details about implementation, interface justifications, and
applications, please see the MPD paper.

 - `int mpd_cntl(int operation, spdid_t composite_spd, spdid_t
      composite_dest);`
   The operation to be performed is dependent on `operation`.

   + `COS_MPD_SPLIT`: `spd1` is one component in a protection
          domain that includes multiple components, including `spd2`.
          `spd2` is the component that is to be separated and put in a
          separate protection domain from the rest.  This call simply
          "splits" that component out of that protection domain and
          into its own.
   + `COS_MPD_MERGE`: `spd1` and `spd2` are components within
          separate protection domains, and each protection domain can
          contain multiple components.  This call with "merge" those
          two protection domains to remove protection boundaries by
          placing all components in each protection domains into one
          large protection domain containing all components.

- `void cos_mpd_update(void);`  Due to invocations operating on stale protection domain
        mappings, we must do garbage collection (of sorts) on
        protection domains.  This call makes that reference counting
        easier and allows components to provide "hints" to expedite
        protection domain changes.  Please see the MPD paper for
        details.  This is complicated.

**Virtual memory management**

- `int cos_mmap_cntl(short int op, short int flags, short int dest_spd, vaddr_t dest_addr, long mem_id);`
  This system call enables the mapping of physical frames to
        virtual pages in separate components.  When the same physical
        frame is mapped into two components, that page is shared
        memory.  The action performed by this system call is dependent
        on `op`.

   + `COS_MMAP_GRANT`: The physical frame identified by
          `mem_id` is mapped into virtual address `dest_addr` of
          component `dest_spd`.  Physical frames are viewed as an
          array of frames, and `mem_id` is simply the offset into
          that array.  TODO: This call should be restricted in that
          only one component should be allowed to make it.
   + `COS_MMAP_REVOKE`: Remove the virtual mapping at
          `dest_addr` in component `dest_spd`, and return the
          `mem_id` that was located there.

**Other functions**

- `int stats(void);`  Print out the event counters within the kernel.

- `int print(char* str, int len);`  Print to dmesg the given string.  Don't use this directly.
        Call the print component.

**Future _Composite_ functionality**
    
- `int cos_vas_cntl(short int op...)`
  Currently, all components share the same virtual address
        space.  This call with be necessary to create new virtual
        address spaces, map components into them, and allocate
        portions of the virtual memory to them.  This is not done
        currently, but needs to be done.  The first step is to enable
        components to use multiple virtual regions (all a multiple of
        4M large).  The second step is to enable the creation of
        multiple virtual address spaces.

