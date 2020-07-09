# Debugging

## Print-based debugging

Though less than idea, print-based debugging is a conventional base-line.
Generally, you want to make sure that you use `printc` as it most directly prints out.
However, on real hardware, it uses serial, which has a very low throughput.
For this reason,

1. printing can significant change the non-functional behavior of the system, and
1. you must make an attempt to minimize the amount of data printed out.

A general technique that is *necessary* for benchmarks, and can be useful for some debugging, is to batch data to print, and actually send it to `printc` *after* the phenomenon you're measuring has completed.

## What code caused a fault?

If your code experiences a fault, how do you figure out what code caused the fault?
The fault will be reported as such^[Note that this is a page-fault (access to undefined memory), but faults will print out for other faults as well.]:

```
General registers-> EAX: 0, EBX: 0, ECX: 0, EDX: 0
Segment registers-> CS: 1b, DS: 23, ES: 23, FS: 23, GS: 33, SS: 23
Index registers-> ESI: 0, EDI: 0, EIP: 0x47800037, ESP: 40810ed8, EBP: 40810f10
Indicator-> EFLAGS: 3202
(Exception Error Code-> ORIG_AX: 6)
FAULT: Page Fault in thd 2 (not-present write-fault user-mode  ) @ 0x0, ip 0x47800037
```

This says that the fault happened while executing in thread number `2`.
The registers of that thread when the fault occurred are printed as `esi` through `ebp`.
`eip` is the instruction pointer.

Let's say this fault was in a `cpu` component.
We wish to figure out which line of code in the `*.o` component caused the fault.
For this we use `objdump`, a program that allows you to decompile a component, and look at its assembly source.
If the C code was compiled with debugging symbols by using the `-g` compiler flag (which we do by default for Composite), then it will also show C lines interwoven with assembly.
To make this code maximally readable, ensure that `src/components/Makefile.comp` includes:
```
OPT= -g -fvar-tracking
#OPT= -O3
```
If optimizations are used (e.g., `-O3`), the code is mangled in the name of efficiency.
To ensure that all code (outside of `libc`) uses these flags, make sure to `make clean; make`.

In the root directory, execute the following:

```
$ objdump -Srhtl c.o
```

(where `c.o` is the component in the build directory for the component).

You'll see the contents of the object file.
We know that the fault happened at instruction address `0x47800037` from the fault report.
Here we ignore the top bits of the address (as the component is offset into virtual memory), and do a search through the object file for the instruction addressed `37`.
We find the code:

``` c
void cos_init(void *arg)
{
  20:   55                      push   %ebp
  21:   89 e5                   mov    %esp,%ebp
  23:   83 ec 08                sub    $0x8,%esp
/.../spin.c:18
        assert(0);
  26:   c7 04 24 00 00 00 00    movl   $0x0,(%esp)
                        29: R_386_32    .rodata
  2d:   e8 20 00 00 00          call   52 <prints>
  32:   b8 00 00 00 00          mov    $0x0,%eax
  37:   c7 00 00 00 00 00       movl   $0x0,(%eax)
/.../spin.c:20
//      spin_var = *(int*)NULL;
        while (1) if (spin_var) other = 1;
  3d:   a1 00 00 00 00          mov    0x0,%eax
```

We can see that instruction `37` dereferenced a `NULL` pointer.
More importantly, if you look up in the code, we see that the code corresponds to line `18` in `/.../spin.c` which corresponds to

``` c
assert(0);
```

That line is within the `cos_init` function.
Because we compiled our components with debugging symbols, we can see the C code.
So we can see that the assert function caused this error.
Now you see the usefulness of assertion statements.
Instead of going through this whole process with objdump, you could have simply looked up in the log and found the following line:

```
assert error in @ spin.c:18.
```

Much easier than disassembling objects.
However, when a fault is caused by an error that didn't trigger an assert, you must use the above techniques to track down the error.

### Special faults

Look at the registers and code carefully when you get a fault.
Two specific cases, and their cause:

1. If a register includes the value `0xdeadbeef`, you can search through the Composite code to see how this could be caused.
	The most common cause of this is that you've *returned* from a thread that was created in a component.
	`sl` does not provide graceful thread teardown by default.
1. If the assembly instruction that is faulting is doing an operation using memory indexed by `%gs`, then you have a Thread Local Storage (TLS) issue.
	We do not, by default, set up TLS, and you can manually do that when new threads are created using `cos_thd_mod`.
	If you're using a library that relies on TLS in a service (not an application) component, support might be quite involved.
	If you can disable TLS through the configuration of the library, that is the first priority.
	Otherwise, talk to the core Composite team.
