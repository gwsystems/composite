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

*Using `addr2line`.*
If you quickly want to figure out in which function, and on which line the error occurred, `addr2line` can be your quick fix.
If the component that experienced the failure is that associated with the composition script `chantest` variable, and the fault was at `0x47800037` (as above), the following will give function and line information:

```
$ addr2line -fi -e system_binaries/global.chantest/tests.chan.global.chantest 0x47800037
__chan_init_with
/home/gparmer/data/research/composite/src/components/lib/chan/./chan_private.h:83
```

The fault was on line `83` in `chan_private` in the `__chan_init_with` function.

*Using `objdump` to get full binary information.*
If you need more information about the fault, for example, what type of operation caused the fault, or which instructions are responsible, then `objdump` can let you introspect into the binary.
This is a great way to learn the correspondance between C and assembly as it will print them interspersed.

In the root directory, execute the following:

```
$ objdump -Srhtl system_binaries/global.chantest/tests.chan.global.chantest
```

(where `system_binaries/global.chantest/tests.chan.global.chantest` is the component in the build directory for the component).

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

## GDB and QEMU debugging

Using GDB with QEMU, we can step through the kernel code line by line and check its memory status efficiently.

To start to use GDB, we should add `-S` and `-s` options to QEMU, `-S` make QEMU stop when startup,
and `-s` open the `gdbserver` for QEMU on TCP port 1234. After that, we then can start GDB by typing `gdb` in the command line to open it.

After GDB starts, it will prompt the banner and gdb command line:

```bash
GNU gdb (Ubuntu 8.1.1-0ubuntu1) 8.1.1
Copyright (C) 2018 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word".
(gdb)
```

We need to add symbol table first by `file`:

```
(gdb) file system_binaries/cos_build-test/cos.img
Reading symbols from system_binaries/cos_build-test/cos.img...done.
(gdb)
```

And connect to QEMU gdbserver by `target`:

```
(gdb) target remote :1234
Remote debugging using :1234
0x0000fff0 in ?? ()
(gdb)
```

Setup a breakpoint to stopped, we choose `kmain()` as the breakpoint:

```
(gdb) b kmain
Note: breakpoint 1 also set at pc 0xc0107291.
Breakpoint 10 at 0xc0107291: file kernel.c, line 104.
```

And enter `c` (continue) to start CPU, it should then stop at `kmain()`:

```
(gdb) c
Continuing.

Breakpoint 1, kmain (mboot=<unavailable>, mboot_magic=<unavailable>,
    esp=<unavailable>) at kernel.c:104
104	{
(gdb)
```

We can open GNU TUI by entering `ctrl-x a` to track the code line by line:

```
   ┌──kernel.c─────────────────────────────────────────────────────────────────┐
   │99              assert(STK_INFO_SZ == sizeof(struct cos_cpu_local_info));  │
   │100     }                                                                  │
   │101                                                                        │
   │102     void                                                               │
   │103     kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp)       │
B+>│104     {                                                                  │
   │105     #define MAX(X, Y) ((X) > (Y) ? (X) : (Y))                          │
   │106             unsigned long max;                                         │
   │107                                                                        │
   │108             tss_init(INIT_CORE);                                       │
   │109             gdt_init(INIT_CORE);                                       │
   │110             idt_init(INIT_CORE);                                       │
   │111                                                                        │
   └───────────────────────────────────────────────────────────────────────────┘
remote Thread 1 In: kmain                                  L104  PC: 0xc0107291
(gdb)
```

By typing `s` (step) or `n` (next), we can stepping into next line and function,
or to run through next line.
