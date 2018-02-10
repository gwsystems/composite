DPDK to Composite
=============================

Changes with descriptions (if necessary)
-----------------------
## Composite ##
1. linker script -- added GOT symbol which may not be necessary anymore
2. Created [dpdk component](../src/components/implementation/no_interface/dpdk_init) --
	- contains helper functions for mem management, threads.
	- acts as booter component
	- calls eal init and will eventually setup ethernet devices
	- Will become the nf manager component
3. [pci library](../src/components/implementation/no_interface/dpdk_init/pci.c)--
	- contains config read and write
	- brute force scan function
4. Increased the thread size in [consts.h](https://github.com/rskennedy/composite/blob/ce2b18603ee8fde495ee34061363cf909abdc41d/src/kernel/include/shared/consts.h#L54)

## DPDK ##
1. [config](https://github.com/rskennedy/dpdk/tree/exp-linuxapp/config) choices are made in `config_minimal` and
	`defconfig-i686-native-cosapp-gcc`
2. Makefiles in `mk/` directory --
	- `cos-machine/rte.vars.mk` sets machine type to be 32 bit machine
		without floating point instructions
3. Forced linking in [drivers (em1000)](https://github.com/rskennedy/dpdk/blob/exp-linuxapp/drivers/net/e1000/em_ethdev.c#L1867) and devices [(bus)](https://github.com/rskennedy/dpdk/blob/exp-linuxapp/lib/librte_eal/common/eal_common_pci.c#L576)
4. Replaced pthreads lib with sl
5. Faked successful init of alarm, logging, interrupt handler thread, etc.
6. PCI -- rewrote scan and probe functions, resource mapping functions

To Do List
-----------------------
1. Support memzones and mempools --
	- this involves getting dpdk's [heap allocator](https://github.com/rskennedy/dpdk/blob/exp-linuxapp/lib/librte_eal/common/malloc_heap.c) working (or tearing it out)
	- locks -- two options here are a tear out or to plug in cos locks
	- enable hugepages backed by runyu's new api
2. Understand the codebase better --
	- how does linking work? [External v. lib v. app](http://dpdk.org/doc/guides/prog_guide/dev_kit_build_system.html)
3. Fix rte_free -- currently page faults
4. Support TLS somehow -- needed for TX and RX threads
5. Clean up PCI functions -- RSK's responsibility

