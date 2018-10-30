This document is intended to serve as a reference to enable `KVM` based Qemu execution for Composite if you've faced some of the problems I've had.

### Qemu problems

In Composite system testing, we use Qemu for testing, especially unit-testing in many cases. 
However, when there is system-level testing or even unit-testing of either scheduling or multi-core (MC) related features, we just cannot rely on Qemu to do the right thing.

By default, Qemu seem to not **pin** a `Virtual CPU (VCPU)` to a `Physical CPU (PCPU)` and instead uses all the available PCPUs to load-balance.
I'm not quite sure if this is the main reason that it misses many of our timer interrupts that we program dynamically as `ONESHOTs`.
In our MC initialization code, we have a lot of serialization among cores or at least the other cores wait for core 0 to finish first. 
In these cases, Qemu just doesn't respond or interleave MC execution as well.
So, none of this is sufficient for a lot of our tests for MC and scheduling tests.

### Qemu with KVM

Qemu has an option to enable us to use hardware virtualization through KVM by using `-enable-kvm` option and use `-cpu host` to make it use host CPU emulation (perhaps for micro-architecture emulation).
This alone makes the system more predictable in virtualization environments.

**But**, you need 
1. Hardware virtualization enabled in BIOS. 
2. Accessing `MSR` registers make KVM throw `General Protection` faults to a VM and in composite, we literally have one place (other than the LAPIC TSC-DEADLINE mode, which is not emulated by KVM/Qemu) where we do a `readmsr` to figure out the CPU frequency.
   https://github.com/gwsystems/composite/blob/ppos/src/platform/i386/chal.c#L110
   So we fault during bootup.
   
   Assuming you're using Ubuntu 32bit 14.04, you'd at least be running Linux kernel 4.4.0-133-generic. 
   The KVM driver has a configuration to **ignore** `MSR` read/writes and setting that configuration is all we need to do to be able to run on KVM.
   This is the output of `modinfo kvm` on my PC, notice that there is a "parm: ignore_msrs:bool". 
   
```
$ modinfo kvm
filename:       /lib/modules/4.4.0-133-generic/kernel/arch/x86/kvm/kvm.ko
license:        GPL
author:         Qumranet
srcversion:     2F12DC75F7F0292A971FC91
depends:        irqbypass
retpoline:      Y
intree:         Y
vermagic:       4.4.0-133-generic SMP mod_unload modversions 686 retpoline 
parm:           allow_unsafe_assigned_interrupts:Enable device assignment on platforms without interrupt remapping support. (bool)
parm:           ignore_msrs:bool
parm:           min_timer_period_us:uint
parm:           kvmclock_periodic_sync:bool
parm:           tsc_tolerance_ppm:uint
parm:           lapic_timer_advance_ns:uint
parm:           halt_poll_ns:uint
parm:           halt_poll_ns_grow:int
parm:           halt_poll_ns_shrink:int
```
   
   You can either enable it through the sys-filesystem: `echo 1 > /sys/module/kvm/parameters/ignore_msrs`, which you'll have to do every time you reboot the host machine.
   Instead, you can have the configuration permanent by writing `options kvm ignore_msrs=Y` to ` /etc/modprobe.d/kvm.conf` (create it if it doesn't exist) and rebooting the machine once.
   
   Given that, you'll still see something like this in `dmesg` output:
```
[  227.807058] kvm [4125]: vcpu0 ignored rdmsr: 0xce

```
   It seems, on newer kernels, there is an option to turn that off as well. But I don't think it's worth our time to update kernel as we literally have **ONE** readmsr in our system.
   If you're curious, it is `options kvm report_ignored_msrs=N` and that would essentially stop KVM from spitting out those warning message to syslog or `dmesg` output.
   
   Got the KVM configuration info from [here](https://forum.proxmox.com/threads/ignore_msrs-for-host-cpu-being-ignored.42416/)
  

