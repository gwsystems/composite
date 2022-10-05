# System Composition

To create an executable system, it requires

1. each component to be compiled with their depended-on libraries, and interfaces (including all of those that are transitively depended on),
1. each of those components must be configured with information about the resources they must manage, and other components to which they must provide service,
1. components in charge of creating other components, must be given access to their elf objects,
1. the components must be configured into a DAG of synchronous invocations, and
1. they are be compiled into the kernel to create a bootable image.

This process is implemented by the `composer` which is guided by a composition script.
The composition script provides information about component inter-dependencies so that abstractions and layers are *designed*, while specializing the services to exactly those needed by your system's goals.
A single component (in the future, multiple components) is charged with creating the other components, called the `constructor`.
The `constructor` solves the problem of how to boot up a system with many different components in a system with a simple kernel.
The kernel is charged with booting a *single* component (the `constructor`), and the `constructor`'s job is to build the rest of the system's components.
To understand the fundamental mechanisms involved in system composition, lets them one at a time.

## Component Compilation

The build system enables the compilation of a *single* component using the `component` directive (`make component...`).
The compilation can be driven by the ease-of-use wrapper: `./cos build <composition script> test`.
The build system compiles the component and links into it all its libraries, the client-side code of interfaces it depends on, and the server-side code of interfaces it implements^[Library code is linked in from the `src/components/lib/*/*.a` file and, in rare cases, the object file identified by `grep OBJECT_OUTPUT src/components/lib/*/Makefile`.
The latter is only used when it includes code that must be compiled into the component, but that might not include symbols that are undefined in the component, for example, for the library that provides the `_start`-equivalent function (`component`).
The client interface code is found in `src/components/interface/*/stubs/cosrt_c_stub.o` which is derived from `src/components/interface/*/stubs/{c_stub.c,stubs.S}`.
The server interface code is found in `src/components/interface/*/stubs/cosrt_s_stub.o` which is derived from `src/components/interface/*/stubs/{s_stub.c,stubs.S}`.].
This includes the transitive closure of all depended-on interfaces and libraries.
Only after this is done, it links the `libc` into the object.

At this point, we have a `.o` file that includes all of the code for the component, and all of the library and interface code for it to interact with the surrounding system.
However, this is not sufficient.
Each component should know meta-data about itself (e.g. its heap-pointer, its component id), and about components it is responsible for exporting services to (e.g. the other component's capability-table frontier, their component id, if they require initialization, etc...).
Only after the `composer` generates all of this information and compiles it into the component can we create an executable.

## Synchronous Invocations and Inter-Component Dependencies

A key part of system creation is the generation of the synchronous invocation (`sinv`) callgates between components that encode their functional dependencies.
The `composer` derives these dependencies from the graph provided in the composition script.
It then parses the component's elf files and determines which functions are depended on and which are exported (by looking at the `__cosrt_c_*` and `__cosrt_s_*` symbols).
Looking at a client *c* that depends on server *s* for the *name* interface, it will match up all of the symbols in *c* matching `__cosrt_c_name_*` with those in *s* matching `__cosrt_s_name_*`.
If any client functions do not find a *s* with corresponding server interfaces, an error will be reported.

Once it finds such matches, it will track which clients have which functions (at which addresses) mapping to which server functions (at which addresses).
The `composer` cannot create the `sinv` resources itself as it is part of the build system, and must instead add configuration information for the `constructor` to query.
When the `constructor` starts executing (remember, it is loaded by the kernel), it loads this information, and creates the corresponding `sinv` resources after creating the components.

But how does the `composer` convey this meta-data to the `constructor`?
The initial arguments (`initargs` library) provides a means to query a hierarchical key-value store that is used by components to retrieve meta-data.
The `composer` generates the `initargs` data-structure for the constructor that encodes all of the `sinv` information for it to create the appropriate functional dependencies.
`initargs` can contain values that are large blobs, and indeed this is how the elf objects of the rest of the components are passed into the `constructor`.

## Service Resources and Information

Certain interfaces are special an imply that the server providing that interface requires some special access to the resources of their clients.
For example,

- A server that provides the `init` interface requires access to the `comp` capability of each of its clients (that use it for the `init` interface).
- The `capmgr` interface implies that the server requires access to the capability table and page-table of the client.
- The `capmgr_create` is identical.
    Some components do not explicitly ask for service from the `capmgr`, but still require capability services for initialization and creation of the initial thread.
- The `kernel` *variant* of an interface means that the client depending on it depends only on the kernel, and not another component to provide the implementation of the interface.
	In this case, only the client libraries are compiled with the component.

Similar to the `sinv` interface, the `composer` must orchestrate different components having access to the `comp`, `captbl`, and `pgtbl` capabilities.
Also similarly, it uses the `initargs` to convey which capabilities, in which components, the `constructor` should place into the capability tables when booting up.

## Making a Bootable Image

To create a bootable image, we need to

1. make the final executable binary for each component by combining it with its `initarg` meta-data,
2. create a tarball containing each of the component's executables (except for the `constructor`),
3. compile that tarball, and all of the `initargs` metadata into the `constructor` executable binary, and
4. compile the `constructor` into the kernel.

And we're done!

- Boot into the kernel,
- it creates the `constructor`,
- which creates the rest of the components, and
- follows the instructions in its metadata to give appropriate access to `comp`, `captbl`, and `pgtbl` resources.

## Investigating a Composed System

The results of the build procedure are ultimately the kernel in `cos.img`.
In the following, I'll use the following as an example: `./cos build composition_scripts/capmgr_ping_pong.toml example`.
Lets check out some of the artifacts of the build procedure.

```
$ ./cos compose composition_scripts/capmgr_ping_pong.toml example
[cos executing] src/composer/target/debug/compose composition_scripts/capmgr_ping_pong.toml example
Compiling component global.ping with the following command line:
        make -C src COMP_INTERFACES="" COMP_IFDEPS="pong/stubs+init/stubs+capmgr_create/stubs" COMP_LIBDEPS="" COMP_INTERFACE=tests COMP_NAME=unit_pingpong COMP_VARNAME=global.ping COMP_OUTPUT=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.ping/tests.unit_pingpong.global.ping COMP_BASEADDR=0x1600000 COMP_INITARGS_FILE=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.ping/initargs.c  component
Compiling component global.pong with the following command line:
        make -C src COMP_INTERFACES="pong/stubs" COMP_IFDEPS="init/stubs+capmgr_create/stubs" COMP_LIBDEPS="" COMP_INTERFACE=pong COMP_NAME=pingpong COMP_VARNAME=global.pong COMP_OUTPUT=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.pong/pong.pingpong.global.pong COMP_BASEADDR=0x00400000 COMP_INITARGS_FILE=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.pong/initargs.c  component
Compiling component global.capmgr with the following command line:
        make -C src COMP_INTERFACES="capmgr/stubs+init/stubs+memmgr/stubs+capmgr_create/stubs" COMP_IFDEPS="init/stubs+addr/stubs" COMP_LIBDEPS="" COMP_INTERFACE=capmgr COMP_NAME=simple COMP_VARNAME=global.capmgr COMP_OUTPUT=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.capmgr/capmgr.simple.global.capmgr COMP_BASEADDR=0x00400000 COMP_INITARGS_FILE=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.capmgr/initargs.c  component
Compiling component global.booter with the following command line:
        make -C src COMP_INTERFACES="init/stubs+addr/stubs" COMP_IFDEPS="init/kernel" COMP_LIBDEPS="" COMP_INTERFACE=no_interface COMP_NAME=llbooter COMP_VARNAME=global.booter COMP_OUTPUT=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.booter/no_interface.llbooter.global.booter COMP_BASEADDR=0x00400000 COMP_INITARGS_FILE=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.booter/initargs.c  component
Compiling component global.booter with the following command line:
        make -C src COMP_INTERFACES="init/stubs+addr/stubs" COMP_IFDEPS="init/kernel" COMP_LIBDEPS="" COMP_INTERFACE=no_interface COMP_NAME=llbooter COMP_VARNAME=global.booter COMP_OUTPUT=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.booter/no_interface.llbooter.global.booter COMP_BASEADDR=0x00400000 COMP_INITARGS_FILE=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.booter/initargs_constructor.c COMP_TAR_FILE=/home/ycombinator/repos/composite/system_binaries/cos_build-example/global.booter/initfs_constructor.tar  component
Compiling the kernel the following command line:
        make -C src KERNEL_OUTPUT="/home/ycombinator/repos/composite/system_binaries/cos_build-example/cos.img" CONSTRUCTOR_COMP="/home/ycombinator/repos/composite/system_binaries/cos_build-example/constructor" plat
System object generated:
        /home/ycombinator/repos/composite/system_binaries/cos_build-example/cos.img
```

Lets check it out!

`$ cd system_binaries/cos_build-example/`

The kernel is `cos.img`, so lets look for the `constructor`.

```
$ nm cos.img | grep _binary_constructor
c24292dc D _binary_constructor_end
0017f2dc A _binary_constructor_size
c22aa000 D _binary_constructor_start
```

We can see that the elf binary for the constructor (which includes all other components in a tarball) is *0x4292dc - 0x2aa000*, or about 1.5 MB.

If we look around a little more, we can see:

```
$ ls
constructor  cos.img  global.booter  global.capmgr  global.ping  global.pong  kernel_compilation.log
```

The `global.*` part of each of these is just the scope, and we only support global scopes in the current composition scripts.
The `booter` component is the `constructor`, `capmgr` is the capability manager, and then we have both `ping` and `pong`.
The `kernel_compilation.log` is, of course, the compilation log from building the kernel.

### Client Component `ping`

The `ping` component is the client-side of the functional invocation of `pong`.

```
$ ls global.ping/
compilation.log  initargs.c  initargs.o  tests.unit_pingpong.global.ping
```

We have the compilation log for the build system's compilation of the component, the initial arguments metadata and the final binary.

```c
$ cat global.ping/initargs.c
#include <initargs.h>
static struct kv_entry *__initargs_autogen_3[] = {};
static struct kv_entry __initargs_autogen_2 = { key: "execute", vtype: VTYPE_ARR, val: { arr: { sz: 0, kvs: __initargs_autogen_3 } } };
static struct kv_entry __initargs_autogen_4 = { key: "captbl_end", vtype: VTYPE_STR, val: { str: "44" } };
static struct kv_entry __initargs_autogen_5 = { key: "compid", vtype: VTYPE_STR, val: { str: "4" } };
static struct kv_entry *__initargs_autogen_1[] = {&__initargs_autogen_5, &__initargs_autogen_4, &__initargs_autogen_2};
static struct kv_entry __initargs_autogen_0 = { key: "_", vtype: VTYPE_ARR, val: { arr: { sz: 3, kvs: __initargs_autogen_1 } } };

struct initargs __initargs_root = { type: ARGS_IMPL_KV, d: { kv_ent: &__initargs_autogen_0 } };
```

Here we can get a sense of the data-structure behind the hierarchical key-value store for the initial arguments.
The `pong` component doesn't have much interesting information here, but we can see that the last used slot in the capability table is at capability `44`, and that the component id is `4`.

Lets see `ping`'s relevant client symbols used for the functional coordination between ping and pong.

```
$ nm global.ping/tests.unit_pingpong.global.ping | grep __cosrt_c
01600a90 T __cosrt_c_cosrtdefault
01600ab0 T __cosrt_c_cosrtretdefault
01600370 T __cosrt_c_init_done
01610000 D __cosrt_comp_info
016002b0 T __cosrt_c_pong_argsrets
01600310 T __cosrt_c_pong_ids
016002e0 T __cosrt_c_pong_subset
```

We can see that some of the main functions we depend on for execution that are provided by other components appear here.

- `__cosrt_c_init_done` is part of the `init` interface and specifies that we want to have a thread that initializes us, often provided by the scheduler.
- `__cosrt_c_pong_ids` is the `pong_ids` function that we're calling from ping to pong.

Also included is the user-level capability structure that provides a level of indirection like the PLT upon the client calling `pong_ids`.

```
nm global.ping/tests.unit_pingpong.global.ping | grep __cosrt_ucap_pong_ids
161108c D __cosrt_ucap_pong_ids
```

where this `ucap` structure is used in `pong_ids` (aliased to `__cosrt_extern_pong_ids`)

```
1600368 <pong_ids>:
 1600368:       b8 8c 10 61 01          mov    $0x161108c,%eax
 160036d:       ff 20                   jmp    *(%eax)
 160036f:       90                      nop
 ```

You can see that this address here matches the `ucap`.
The `ucap` holds a function pointer that is invoked via the `jmp` that most often directly points to `__cosrt_c_pong_ids`.

Lets back up a little bit.
When the client calls `pong_ids`, it gets redirected to this small little assembly stub that accesses the `ucap` structure and calls a function pointer that holds the client stub.
It also passes the address of the `ucap` to the stub.
Within this `ucap` structure, we hold the capability id to the `sinv` that we need to invoke to properly call the `pong_ids` function in the server.

### Server Component `pong`

On the server side, in `pong`, we see

```
$ nm global.pong/pong.pingpong.global.pong | grep __cosrt_s
...
00400560 T __cosrt_s_cstub_pong_ids
...
004004f0 T __cosrt_s_pong_ids
...
```

We can see where the `pong_ids` function starts its execution in this component with `__cosrt_s_pong_ids`.
That assembly code calls the C function that performs any argument manipulation (deserialization) necessary in `__cosrt_s_cstub_pong_ids`.

### `constructor` Component

```
$ ls global.booter/
compilation.log              initargs.c              initargs_constructor.o  initfs_constructor.tar    no_interface.llbooter.global.booter
constructor_compilation.log  initargs_constructor.c  initargs.o              initfs_constructor.tar.o
```

Here we additionally see the tarball, the tarball compiled into an object file, and multiple initargs files and compilation logs.
I won't go into why there are two of each of these here, and we'll just look at the `constructor` versions.

The `initargs_constructor.c` file is large, so I won't show it here, but it includes each of the components, each synchronous invocation callgate that must be created, and the set of resources each component should have access to.

We can see the tarball that includes all other components in the final binary as

```
$ nm global.booter/no_interface.llbooter.global.booter | grep binary
00535000 D _binary_crt_init_tar_end
0011c400 A _binary_crt_init_tar_size
00418c00 D _binary_crt_init_tar_start
```

## Composition Scripts
