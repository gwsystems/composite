# What

The is the *Speck* version of the scheduler library.
It encodes some default behaviors (i.e. fixed capability offsets for rcv, tcap, initial thread), and handles most of the esoteric synchronization required by the interaction between user and kernel.
It has a plugin infrastructure with three main parts:

- *Scheduling policy* - encoded in `sl_mod_<name>.c` and `sl_mod_policy.h`.
  There will be a separate version of this per scheduling policy (each in a subdirectory as in the current `src/components/implementation/sched/fprr/` organization).
- *Allocation policy* - how the actual thread data-structure is allocated and referenced.
  This is encoded in `sl_thd_<name>_backend.c`.
- *Timer policy* - The policy for when timer interrupts are set to fire.
  This is encoded in `sl_timer_mod_<name>.c`.

# How

The API for each of these plugins is encoded in `sl_plugins.h`.
Though I'll try not to change this greatly, it likely will.
Of course the intention is that this is a stable API which can support a great many implementations behind it.
At the very least, it was designed with the following in mind:

- FPRR, EDF
- `ps`-based dynamic allocation and reclamation
- Oneshot and periodic timers
- Extensions to multicore

## Client API

The client API is relatively simple, and is populated with the `sl_*` functions.
The main (`cos_init`) thread should enter the event loop after initialization as such:

```
cos_meminfo_init(&cos_defcompinfo_curr_get()->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
cos_defcompinfo_init();

sl_init()
...
sl_sched_loop();
```

Other threads can be created using `sl_thd_alloc(fn, data)` within the same component, or using the `cos_defkernel_api` and `sl_thd_comp_alloc(target_comp)` to create execution in another component.
Threads will *not* be executed until their parameters are set; at least their priority, via `sl_thd_param_set`.

Once executing, threads can `sl_block(...)`, and `sl_wakeup(target_thd)` another blocked thread.
They can also leverage `sl_cs_enter` and `sl_cs_exit_schedule` for a critical section on this core to protect data-strutures.
Do note that most of the `sl_*` API does take the critical section itself, and recursive critical sections are not allowed.

The entire timing API is in the unit of finest granularity provided by the hardware (`cycles_t`).
There are conversion functions between that unit and microseconds (`sl_usec2cyc` and `sl_cyc2usec`).
We don't hide the internal, fine-grained unit to avoid costly translation between the units when that can be avoided.

# FIXME

- The current timing seems to be way off, but it is hard to tell if that is just qemu or not.
- `sl_thd_prio` needs integration with `param_set`.
- Needs more thorough testing

# TODO

- More policies in all dimensions
- Either add a queue of timers, or a facility for a separate thread/component to maintain that queue
- tcap modification facilities such as binding threads to specific tcaps
- tcap timeout handlers to suspend those threads (via policy)
- `aep` endpoints: asynchronous rcv + tcap + thread tuples with asynchronous activations; most of this should already work, but we need an API for this
- schedule not just threads, but also aep endpoints to enable hierarchical scheduling, and in doing so, integrate with APIs for tcap transfer (i.e. period-based?, larger?)
- a separate API to virtually "disable interrupts" might be necessary to support the likes of the rump kernel
- idle processing should be wrapped into the `sl_sched_loop` processing, so that when idle, `rcv` can be called, thus switching to a parent scheduler.  `rcv`ing on the root rcv end-point should likely idle the processor, which could get complicated.  In the mean time, returning immediately would help.  To make this work, 1. the sched loop needs to disambiguate between different types of activations, and 2. the `cos_sched_rcv` should have a flag to allow it to be called in blocking (idle) or non-blocking (scheduler event retrieval) modes.
