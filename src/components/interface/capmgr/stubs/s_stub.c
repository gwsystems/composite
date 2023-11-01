#include <cos_stubs.h>
#include <capmgr.h>

COS_SERVER_3RET_STUB(arcvcap_t, capmgr_rcv_create)
{
	asndcap_t retasnd = 0;
	arcvcap_t ret;
	thdcap_t  thdcap;
	thdid_t   tid;

	ret = capmgr_rcv_create(p0, p1, &retasnd, &thdcap, &tid);
	*r1 = retasnd;
	*r2 = (thdcap << 16) | tid;

	return ret;
}

COS_SERVER_3RET_STUB(vaddr_t, capmgr_shared_kernel_page_create)
{
	vaddr_t resource = 0;
	vaddr_t ret;

	ret = capmgr_shared_kernel_page_create(&resource);
	*r1 = resource;

	return ret;
}

COS_SERVER_3RET_STUB(capid_t, capmgr_vm_lapic_create)
{
	vaddr_t page = 0;
	vaddr_t ret;

	ret = capmgr_vm_lapic_create(&page);
	*r1 = page;

	return ret;
}

COS_SERVER_3RET_STUB(capid_t, capmgr_vm_shared_region_create)
{
	vaddr_t page = 0;
	vaddr_t ret;

	ret = capmgr_vm_shared_region_create(&page);
	*r1 = page;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_vm_vcpu_create)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_vm_vcpu_create(p0, p1, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_create_thunk)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_thd_create_thunk(p0, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_thd_create_ext)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_thd_create_ext(p0, p1, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_initthd_create)
{
	thdid_t retthd = 0;
	thdcap_t ret;

	ret = capmgr_initthd_create(p0, &retthd);
	*r1 = retthd;

	return ret;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_initaep_create)
{
	spdid_t                 child  =  p0 >> 16;
	int                     owntc  = (p0 << 16) >> 16;
	cos_channelkey_t        key    =  p1 >> 16;
	u32_t                   ipimax = (p1 << 16) >> 16;
	u32_t                ipiwin32b = (u32_t)p2;
	struct cos_aep_info     aep;
	asndcap_t               snd = 0;
	thdcap_t                thd;

	thd = capmgr_initaep_create(child, &aep, owntc, key, ipimax, ipiwin32b, &snd);
	*r1 = (snd << 16)      | aep.tid;
	*r2 = (aep.rcv << 16) | aep.tc;

	return thd;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_aep_create_thunk)
{
	thdclosure_index_t      thunk  = (p0 << 16) >> 16;
	int                     owntc  =  p0 >> 16;
	cos_channelkey_t        key    =  p1 >> 16;
	u32_t                   ipimax = (p1 << 16) >> 16;
	u32_t                ipiwin32b = (u32_t)p2;
	struct cos_aep_info     aep;
	asndcap_t               snd;
	thdcap_t                thd;

	thd = capmgr_aep_create_thunk(&aep, thunk, owntc, key, ipimax, ipiwin32b);
	*r1 = aep.tid;
	*r2 = (aep.rcv << 16) | aep.tc;

	return thd;
}

COS_SERVER_3RET_STUB(thdcap_t, capmgr_aep_create_ext)
{
	struct cos_aep_info aep;
	spdid_t child = p0 >> 16;
	thdclosure_index_t idx = ((p0 << 16) >> 16);
	int owntc = p1;
	cos_channelkey_t key = p2 >> 16;
	microsec_t ipiwin = p3;
	u32_t ipimax = ((p2 << 16) >> 16);
	arcvcap_t extrcv = 0;
	thdcap_t ret;

	ret = capmgr_aep_create_ext(child, &aep, idx, owntc, key, ipiwin, ipimax, &extrcv);
	*r1 = aep.tid | (extrcv << 16);
	*r2 = (aep.rcv << 16) | aep.tc;

	return ret;
}

COS_SERVER_3RET_STUB(capid_t, capmgr_vm_vmcb_create)
{
	capid_t ret;
	capid_t dummy                 = (p0 >> 16 * 0) & 0xFFFF;
	capid_t vmcs_cap              = (p0 >> 16 * 1) & 0xFFFF;
	capid_t msr_bitmap_cap        = (p0 >> 16 * 2) & 0xFFFF;
	capid_t lapic_access_cap      = (p0 >> 16 * 3) & 0xFFFF;
	capid_t lapic_cap             = (p1 >> 16 * 0) & 0xFFFF;
	capid_t shared_mem_cap        = (p1 >> 16 * 1) & 0xFFFF;
	thdid_t handler_id            = (p1 >> 16 * 2) & 0xFFFF;
	u16_t vpid                    = (p1 >> 16 * 3) & 0xFFFF;

	ret = capmgr_vm_vmcb_create(vmcs_cap, msr_bitmap_cap, lapic_access_cap, lapic_cap, shared_mem_cap, handler_id, vpid);

	return ret;
}
