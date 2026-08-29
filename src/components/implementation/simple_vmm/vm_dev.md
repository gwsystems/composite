# VM Development Guide

### The guest Linux image

The guest is an **unmodified upstream Linux kernel**. It is built by the
[cos-vmimg](https://github.com/esmakokten/cos-vmimg) submodule at
`src/components/implementation/simple_vmm/vmimg`, which downloads a stock kernel
tarball, verifies its checksum, and builds it with a BusyBox initramfs linked in.

## How to build the system

### Build the guest Linux VM

```shell
git submodule update --init --recursive
./cos build
```

That is the whole procedure. `simple_vmm/vmm/Makefile` builds the image from the
submodule and installs it as `guest/vmlinux.img`.

### Choosing which guest to boot

The composition script names the image, so switching guests is a `./cos compose`
rather than a rebuild:

```toml
[[components]]
name = "vmm"
img  = "simple_vmm.vmm"
constants = [{variable = "VM_GUEST_IMAGE",
              value = "\"guest/vmlinux-vmexit-bench-5.15.107.img\""}]
```

The escaped quotes matter: the composer substitutes the value verbatim into a
`#define`, so without them it is not a string literal and the component will not
compile. If the named image is not in `guest/` yet, it is built on demand — and
only it, not the other recipes.

The available images are the recipes in `vmimg/recipes/`, each a small TOML
naming the programs, kernel modules and init script to include. Adding one means
adding a recipe and a program; see `vmimg/README.md`.

`./cos build` on its own does not build a guest kernel — there is no composition
at that point, so it embeds a placeholder. `./cos compose` recompiles the
component against the image its script names, building that image if needed. So
a Composite build for a system with no VM in it costs nothing extra.

A `guest/vmlinux.manifest` is installed beside the image recording the kernel
version, the config and initramfs hashes, and the program list, so an image in a
build tree can always be identified.

**Kernel version.** It comes from whichever cos-vmimg commit the submodule is
pinned to — currently the `kernel-5.15.107` tag. cos-vmimg's `main` tracks the
current kernel and older lines are frozen as tags, so moving the guest forward
is a deliberate submodule bump rather than something that changes underneath
this tree.

**Testing the guest without Composite.** The same image boots under plain QEMU,
which is much faster to iterate on:

```shell
cd src/components/implementation/simple_vmm/vmimg
make run RECIPE=shell
```

### Build the Composite

- **Where is the hypervisor code**?

	There are three components to support vmx in Composite:

	1. `src/platform/x86_64/vmx`: this is the kernel support for vmx.
	2. `src/components/lib/vmrt`: this is the vm lib for user level manipulating vm operation in Composite.
	3. `src/components/implementation/simple_vmm`: this is a simple hypervisor implementation based on the vmrt.

- **Where is the VM (guest) image and how does Composite hypervisor load it**?

	The guest image consists of two parts: the guest bootloader and the guest Linux. Thus the hypervisor needs to load both of them and let the guest bootloader to find guest Linux and load it.

	The guest Linux image is built by the `vmimg` submodule and installed to
	`src/components/implementation/simple_vmm/vmm/guest/vmlinux.img` as part of
	`./cos build`. It is a build output and is not tracked in git.

	The guest bootloader is here: `src/components/implementation/simple_vmm/vmm/guest/guest_realmode.S`. It will then be compiled to this binary file: `src/components/implementation/simple_vmm/vmm/guest/guest.img`.

	Now we have both the `guest.img` (the guest bootloader) and the `vmlinux.img` (the stock Linux kernel image).

	The two guest images will then be included into the simple vmm component. 

	The hypervisor will then read these two images and load them into VM's virtual physical address page by page.

- **How to build the Composite hypervisor?**
	```shell
	git submodule update --init --recursive
	./cos init x86_64
	./cos build
	./cos compose composition_scripts/vmm_simple_test.toml vm
	```
	Note that `./cos build` exits 0 even when a component fails to compile, so
	check its output for `error:` rather than relying on the exit status.
- **How to run the system?**
	To run the system on Qemu:
	```shell
	./cos run vm
	```

## Debugging

### Debugging the guest Linux in Composite

You can use the kernel's `printk` function to print information you want to know what is happening in the VM, for example:
```c
	printk(KERN_INFO "%s at %u in ()\n", __FILE__, __LINE__);
	pr_info("%s at %u in (%s)\n", __FILE__, __LINE__, __func__);
```

You can also use `vmcall` inside the linux kernel to force it out the VM:
```c
	asm volatile("vmcall");
```
This will cause VM exit and then you can do some hacking.

### Debugging the guest Linux in Qemu
Since it is in the Qemu, you can use either the print functions within the Linux kernel or GDB support by Qemu to hack the kernel.