## res_spec

A simple specification for mostly rate-based resources.
This is used to express thread scheduling parameters, and should be more general to express timeliness constraints for other rate-based resources as well.

### Description

The goals of this simple library are to

1. Try and make an interface that can be *interpreted* as providing all different types of resource specifications.
    For example, we want to be able to specify periodicities, budgets, expected execution times, fixed priorities, dynamic priorities, etc...
2. Enable the parameters to be serialized into a register for ease of IPC in low-level services.
3. Few dependencies.
4. Provide an interface that client and server can share.

### Usage and Assumptions

See the `sched` interface for how/where this is used.
This library serializes each parameter into a 32-bit value so that it can be passed without shared memory to a server.
Each parameter has two parts:

1. A type in `sched_param_type_t`, for example `SCHEDP_PRIO` if you want to set a fixed priority.
2. The value associated with that type of parameter.

If we wanted to set a thread's priority to `10`, we'd construct the parameter as:

```c
u32_t param = SCHED_PARAM_CONS(SCHEDP_PRIO, 10);
```

...or, equivalently, and including passing to the scheduler:

```c
u32_t param = sched_param_cons(SCHEDP_PRIO, 10);
sched_thd_param_set(cos_thdid(), param);
```

To deserialize the parameters (for example, in the scheduler):

```c
sched_param_type_t type;
unsigned int prio;

sched_param_get(param, &type, &prio);
assert(prio == SCHEDP_PRIO);
assert(prio == 10);
```
