This document records some future work or potential fixes needed in the future.  

## The right option to create a VM component
For EPT, having a EPT cap or having a separate operation for PGTBL_CAP for EPT would be the best. And making the flags operations open to kernel api, thus different types of page tables can pass their own flags with the same code path.

For VM thd, there also needs a seprate path to figure out whether it is a vcpu thread or host thread. A separate VM_THD_CAP would be better since it can then be assigned to VMM to switch to. There should also be some new capabilities to some kernel pages, so that the creation code can combine them together and make it one capability for share.

However, that's not what the current code is. The current code uses a VM flag bit when creating EPT page tables within the `level` argument in order to reduce workload based on the current kernel and user level codebase.

## Linux image
The ideal choice would be to just use the compressed linux kernel and load it with Composite's ELF loader. But, currently we don't use that for simplicity. Instead, we add a simple booter(which doesn't modeif the kernel itself) in Linux to boot Linux.

## VM Logging System
The vmx log level in the kernel can be controled by `COS_VM_LOG_GLOBAL_LEVEL` flag. Set it to `debug` level will print most verbose information.

## EPT
EPT is the paging structure used to translate guest physical addresses into host physical addresses, thus providing guest physical address virtulization.

To simplify the implementation, we don't use the super EPT page tables feature and bits above the `MAXPHYADDR` as these bits are controled by their VM-execution controls. By default we don't enable those controls and thus they will be ignored by hardware.

Thus, we will only use 4K size mapping with EPT page tables. **This means we won't have 1G or 2M page size mapping.** In this case, the EPT page table walk through is similiar to the normal page table in that their page-walk length are both 4 (We also don't talk 5 level paging). **Both of them also use the same 12-MAXPHYADDR bits in one page table entry to reference to next level page table structure.**

## MSR emulation
We don't have MTRR MSR emulation, the Linux kernel can know this and handle them correctly by setting the MTRR MSRs to be 0.
