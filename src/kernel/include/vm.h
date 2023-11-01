#pragma once

#include "captbl.h"
#include "thd.h"

struct cap_vm_vmcs {
	struct cap_header  h;
	vaddr_t page;
} __attribute__((packed));

struct cap_vm_msr_bitmap {
	struct cap_header  h;
	vaddr_t page;
} __attribute__((packed));

struct cap_vm_lapic_access {
	struct cap_header  h;
	vaddr_t page;
} __attribute__((packed));

struct cap_vm_lapic {
	struct cap_header  h;
	vaddr_t page;
} __attribute__((packed));

struct cap_vm_shared_mem {
	struct cap_header  h;
	vaddr_t page;
} __attribute__((packed));

struct cap_vm_vmcb {
	struct cap_header  h;
	struct cap_vm_vmcs *vmcs;
	struct cap_vm_msr_bitmap *msr_bitmap;
	struct cap_vm_lapic_access *lapic_access;
	struct cap_vm_lapic *lapic;
	struct cap_vm_shared_mem *shared_mem;
	struct cap_thd *handler_thd;
	u16_t vpid;
} __attribute__((packed));

static int
vm_vmcs_activate(struct captbl *t, capid_t cap, capid_t capin, vaddr_t page)
{
	struct cap_vm_vmcs *vmcs;
	int ret = 0;

	vmcs = (struct cap_vm_vmcs *)__cap_capactivate_pre(t, cap, capin, CAP_VM_VMCS, &ret);
	if (!vmcs) cos_throw(done, ret);
	vmcs->page = page;
	__cap_capactivate_post(&vmcs->h, CAP_VM_VMCS);

done:
	return ret;
}

static int
vm_msr_bitmap_activate(struct captbl *t, capid_t cap, capid_t capin, vaddr_t page)
{
	struct cap_vm_msr_bitmap *bitmap;
	int ret = 0;

	bitmap = (struct cap_vm_msr_bitmap *)__cap_capactivate_pre(t, cap, capin, CAP_VM_MSR_BITMAP, &ret);
	if (!bitmap) cos_throw(done, ret);
	bitmap->page = page;
	__cap_capactivate_post(&bitmap->h, CAP_VM_MSR_BITMAP);

done:
	return ret;
}

static int
vm_lapic_access_activate(struct captbl *t, capid_t cap, capid_t capin, vaddr_t page)
{
	struct cap_vm_lapic_access *lapic_access;
	int ret = 0;

	lapic_access = (struct cap_vm_lapic_access *)__cap_capactivate_pre(t, cap, capin, CAP_VM_LAPIC_ACCESS, &ret);
	if (!lapic_access) cos_throw(done, ret);
	lapic_access->page = page;
	__cap_capactivate_post(&lapic_access->h, CAP_VM_LAPIC_ACCESS);

done:
	return ret;
}

static int
vm_lapic_activate(struct captbl *t, capid_t cap, capid_t capin, vaddr_t page)
{
	struct cap_vm_lapic *lapic;
	int ret = 0;

	lapic = (struct cap_vm_lapic *)__cap_capactivate_pre(t, cap, capin, CAP_VM_LAPIC, &ret);
	if (!lapic) cos_throw(done, ret);
	lapic->page = page;
	__cap_capactivate_post(&lapic->h, CAP_VM_LAPIC);

done:
	return ret;
}

static int
vm_shared_mem_activate(struct captbl *t, capid_t cap, capid_t capin, vaddr_t page)
{
	struct cap_vm_shared_mem *shared_mem;
	int ret = 0;

	shared_mem = (struct cap_vm_shared_mem *)__cap_capactivate_pre(t, cap, capin, CAP_VM_SHARED_MEM, &ret);
	if (!shared_mem) cos_throw(done, ret);
	shared_mem->page = page;
	__cap_capactivate_post(&shared_mem->h, CAP_VM_SHARED_MEM);

done:
	return ret;
}

static int
vm_vmcb_activate(struct captbl *t, capid_t cap, capid_t capin, capid_t vmcs_cap, capid_t msr_bimap_cap, capid_t lapic_access_cap, capid_t lapic_cap, capid_t handler_thd_cap, capid_t shared_mem_cap, u16_t vpid)
{
	struct cap_vm_vmcs *vmcs;
	struct cap_vm_msr_bitmap *msr_bitmap;
	struct cap_vm_lapic_access *lapic_access;
	struct cap_vm_lapic *lapic;
	struct cap_vm_shared_mem *shared_mem;
	struct cap_thd *handler_thd;
	struct cap_vm_vmcb *vmcb;
	int ret = 0;

	vmcs = (struct cap_vm_vmcs *)captbl_lkup(t, vmcs_cap);
	if (unlikely(!vmcs || vmcs->h.type != CAP_VM_VMCS || !vmcs->page)) return -EINVAL;

	msr_bitmap = (struct cap_vm_msr_bitmap *)captbl_lkup(t, msr_bimap_cap);
	if (unlikely(!msr_bitmap || msr_bitmap->h.type != CAP_VM_MSR_BITMAP || !msr_bitmap->page)) return -EINVAL;

	lapic_access = (struct cap_vm_lapic_access *)captbl_lkup(t, lapic_access_cap);
	if (unlikely(!lapic_access || lapic_access->h.type != CAP_VM_LAPIC_ACCESS || !lapic_access->page)) return -EINVAL;

	lapic = (struct cap_vm_lapic *)captbl_lkup(t, lapic_cap);
	if (unlikely(!lapic || lapic->h.type != CAP_VM_LAPIC || !lapic->page)) return -EINVAL;

	shared_mem = (struct cap_vm_shared_mem *)captbl_lkup(t, shared_mem_cap);
	if (unlikely(!shared_mem || shared_mem->h.type != CAP_VM_SHARED_MEM || !shared_mem->page)) return -EINVAL;

	handler_thd = (struct cap_thd *)captbl_lkup(t, handler_thd_cap);
	if (unlikely(!handler_thd || handler_thd->h.type != CAP_THD || !handler_thd->t)) return -EINVAL;

	vmcb = (struct cap_vm_vmcb *)__cap_capactivate_pre(t, cap, capin, CAP_VM_VMCB, &ret);
	if (!vmcb) cos_throw(done, ret);

	vmcb->vmcs = vmcs;
	vmcb->msr_bitmap = msr_bitmap;
	vmcb->lapic = lapic;
	vmcb->lapic_access = lapic_access;
	vmcb->shared_mem = shared_mem;
	vmcb->handler_thd = handler_thd;
	vmcb->vpid = vpid;

	__cap_capactivate_post(&vmcb->h, CAP_VM_VMCB);

	return 0;

done:
	return ret;
}

static void
vm_cap_init(void)
{
	assert(sizeof(struct cap_vm_vmcb) <= __captbl_cap2bytes(CAP_VM_VMCB));
}
