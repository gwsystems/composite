## DPDK

### Description
This is Composite's version DPDK port, used as a NIC manager.
### Usage and Assumptions
Composite does some hacks within DPDK, currently it only supports single thread, without interrupts and huge page capabilities.

## How to compile DPDK with Musl libc in Composite?

### Remove shared library support in DPDK meson build system
- The DPDK library config file is: `dpdk/lib/meson.build`
- We need to remove shared lib support in this muson.build file, by just deleting lines with keywords related with "shared". 
- Notice we cannot delete this line : `dpdk_libraries = [shared_lib] + dpdk_libraries`, we need to change it to `dpdk_libraries = dpdk_libraries`
- The reason behind this is: DPDK meson build system will try to build share libraries (those `.so` files) which will link syscall function objects. However, in Composite, we have modified the syscall functions in Musl so that they are pointed to `__cos_syscall`. Thus, when linking the shared libraries, linker cannot find the object files contained `__cos_syscall`.
- Also, we don't need share libraries in Composite DPDK, remove them in the meson.build file could save a lot compiling time.

### Use a cross compile config file 
- Use this cross compile config file : `src/components/lib/dpdk/cross_x86_64_composite_with_musl`
- Reason: Before the real configure and compiling process, the meson build system will try to do a compiler sanity check which tries to compile a simple C application to test if compiling toolchain could work as expected. However, based on the `__cos_syscall` problem above, it is not possible to build a linux application as the linker cannot find the `__cos_syscall`. There is no way to avoid this if you are using native build (Meson automatically does this). Thus, you have to use a cross compiling config file, in which you can specify `skip_sanity_check = false` to skip the sanity check (This function is supported in **Meson 0.56.0** and higher). Also, in this file you could specify the c compiler to `musl-gcc`.


### Modify another meson.build file in DPDK
- Location: `dpdk/config/meson.build`
- Remove the `meson.is_cross_build()` if condition statement.
- Reason: Because we are using the cross compile config, the DPDK meson build configuration refused to accept the `-Dmachine=default` config if we don't do this modification. It will throw build error like this: `ERROR: Could not get define '__SSE4_2__'`. I believe this is a bug of the DPDK meson build configuration code. Thus, to avoid this, we have to make the `-Dmachine=default` config work as mentioned above.


### Remove the DPDK `RTE_BACKTRACE` config
- Commented the `#define RTE_BACKTRACE 1` in `config/rte_config.h`
- Reason: this will avoid comping some linux specific headers, as we don't need them in Composite.

### Copy necessary header files to the Musl libc include path
- cd src/components/lib/libc/musl-1.2.0/include
- cp -r /usr/include/linux/ ./
- cp -r /usr/include/asm-generic/ ./
- cp -r /usr/include/x86_64-linux-gnu/asm/ ./
- cp -r /usr/include/x86_64-linux-gnu/sys/queue.h ./sys
- Reason: DPDK meson build system does not support a standfree compiling configuration, there are only three platform options: `linux`, `fresbsd` and `windows`. Thus we have to choose `linux` here. Many code in DPDK rely on some linux system headers. We copy them here into the Musl libc path because 1. We use `-nostdinc -nostdlib` flags so `musl-gcc` will not search the system libc headers but search the Musl libc includ path. 2. Putting those files inside Musl libc include path can reslove compiling warnings and some other definition problems in those header files. 

### Small tips
- If you are just compiling DPDK with Musl libc in Linux system rather than Composite, you don't need do the steps above except the last one (You always need to copy necessary linux system header files to the Musl libc include path).