# Runscripts in *Composite*

Runscripts serve a simple purpose:  they specify which components you want to use in your system, and how they should be connected together (i.e. which are dependent on which).  Their intention is to create a directed, acyclic graph of components that will be booted into a functional system.

**Background.** A *Composite* system boots up in multiple stages.

1. The boot-loader creates the memory and initial data-structures for the low-level booter component (llboot).  That component contains an image of all other components in the system.
2. Control is passed to llboot, and it creates a fixed set of other often requisite components.  These include a scheduler (e.g. fprr), the memory mapper (mm), a console printer (print), and the second stage booter (boot).  These are created, and initialized.
3. When the booter is initialized, it, like llboot, contains an image of the rest of the system.  It creates the rest of the components, and links them together with communication capabilities, and initializes them with targeted upcalls.

Lets look at a simple example of a runscript, given this:

```
#!/bin/sh
./cos_loader \
"c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\
!cpu.o, ;(!cpu1.o=cpu.o), :\
c0.o-llboot.o;\
fprr.o-print.o|[parent_]mm.o|[faulthndlr_]llboot.o;\
mm.o-[parent_]llboot.o|print.o;\
boot.o-print.o|fprr.o|mm.o|llboot.o;\
cpu.o-print.o|fprr.o|mm.o;\
cpu1.o-print.o|fprr.o|mm.o\
" ./gen_client_stub
```

All runscripts simply run the cos_loader program if you're booting into *Composite* from Linux.  The first argument to that program is the system specification.  It includes three main sections.  First components loaded by llboot, then components loaded by boot, then the list of dependencies between components (which follows the `:`).

All this runscript does is specify the 5 initial components (and c0 which I'll ignore here).  The order these are specified is important and shouldn't change (e.g. the scheduler should always follow llboot).  Each of these components are created by the normal build process (search for "mm.o" in the `src/components/implementation/` directory).  The `*` is used to specify which components must be schedulers.  Note the `, ;` separate each component.  This is legacy, but is necessary currently.

Next, we have a series of components prepended with `!`.  That specifies that they will be loaded by the boot component.  The `(a.o=b.o)` syntax specifies that we're creating a component called `a.o` that is functionally identical to `b.o`.  So in this example we have two "cpu" components that are being loaded.  These should likely just include infinite loops, thus is a simple test for round-robin scheduling.

Following the `:`, we have the list of dependencies specified as `a.o-b.o|c.o`.  The component (that must be in the initial list before the `:`) on the left of the `-` depends on functions in interfaces exported by the components on the right of the `-`, speparated by `|`s.  In this case, component `a.o` wants to invoke functions in both `b.o` and `c.o`.  

The syntax `[parent_]` is necessary to enable a component that exports an interface, to also be able to depend on and invoke functions in components that provide the same interface.  For example, if `mm.o` provides the `mman_alias` function, and `mm.o-hiermm.o`, where `hiermm.o` also exports `mman_alias`, then any call in `mm.o` to that function would be recursive *within* `mm.o`.  How can we invoke the same function in `hiermm.o`?  We can change the dependency to `mm.o-[parent_]hiermm.o` which specifies that any calls to `parent_mman_alias` in `mm.o` should go to the corresponding function in `hiermm.o`.

**Note:**
If the runscript syntax looks insane, we're with you. Efforts are being made to fix it.
