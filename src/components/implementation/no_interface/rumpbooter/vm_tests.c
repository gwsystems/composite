#include "micro_booter.h"
#include "vk_types.h"
#include "cos2rk_rb_api.h"

void
test_shmem(int vm)
{
	char buf[50] = { '\0' };
	if (!vm) {
		int i = 0;
		while (i < COS2RK_VIRT_MACH_COUNT - 1) {
			memset(buf, '\0', 50);
			vaddr_t shm_addr = COS2RK_MEM_SHM_BASE + i * COS2RK_SHM_VM_SZ;
			sprintf(buf, "SHMEM %d to %d - %x", vm, i + 1, (unsigned int)cos_va2pa(&booter_info, (void *)shm_addr));
			strcpy((char *)shm_addr, buf);
			PRINTC("VM %d Wrote to %d: \"%s\" @ %x:%x\n", vm, i + 1, buf, (unsigned int)shm_addr, (unsigned int)cos_va2pa(&booter_info, (void *)shm_addr));
			i ++;
		}
	} else {
		int i = 0;
		//PRINTC("%d: read after delay\n", vm);
		for (i = 0; i < 99000; i ++) ;
		strncpy(buf, (char *)COS2RK_MEM_SHM_BASE, 49);
		PRINTC("VM %d Read: %s @ %x:%x\n", vm, buf, (unsigned int)COS2RK_MEM_SHM_BASE, (unsigned int)cos_va2pa(&booter_info, (void *)COS2RK_MEM_SHM_BASE));
	}
}

void
test_vmio(int vm)
{
	/* FIXME: Not tested with new end-points. need VM scheduling for it. */
	if (COS2RK_VIRT_MACH_COUNT > 1) {
		static int it = 0;
		char buf[50] = { '\0' };
		if (!vm) {
			int i = 1;
			while (i < COS2RK_VIRT_MACH_COUNT) {
				int j = 0;
				memset(buf, '\0', 50);
				asndcap_t sndcap = DOM0_CAPTBL_SELF_IOASND_SET_BASE + (i - 1) * CAP64B_IDSZ;
				vaddr_t shm_addr = COS2RK_MEM_SHM_BASE + (i - 1) * COS2RK_SHM_VM_SZ;
				sprintf(buf, "%d:SHMEM %d to %d - %x", it, vm, i, (unsigned int)cos_va2pa(&booter_info, (void *)shm_addr));
				PRINTC("Sending to %d\n", i);
				cos2rk_shm_write(buf, strlen(buf) + 1, vm, i);
				cos_asnd(sndcap, 1);
				PRINTC("Sent to %d: \"%s\" @ %x:%x\n", i, buf, (unsigned int)shm_addr, (unsigned int)cos_va2pa(&booter_info, (void *)shm_addr));
				i ++;
			}
		} else {
			int i = 0;
			int j;
			int tid, rcving, cycles;
			PRINTC("Receiving..\n");
			for(j = 0; j < (15*vm); j++){
				PRINTC("%d",j);
			}
			cos_rcv(VM_CAPTBL_SELF_IORCV_BASE, 0, 0, NULL);
			PRINTC("cos2rk_shm_read returned: %d\n", cos2rk_shm_read(buf, 0, vm));
			PRINTC("Recvd: %s @ %x:%x\n", buf, (unsigned int)COS2RK_MEM_SHM_BASE, (unsigned int)cos_va2pa(&booter_info, (void *)COS2RK_MEM_SHM_BASE));
		}
		it ++;
	}
}

void
test_vmio_events(void)
{
	PRINTC("Testing vmio events\n");
	if (vmid == 0) {
		int i;
		asndcap_t snd = DOM0_CAPTBL_SELF_IOASND_SET_BASE;
		
		for (i = 1; i < COS2RK_VIRT_MACH_COUNT; i ++) {
			snd += (i - 1) * CAP64B_IDSZ;
			cos_asnd(snd, 1);
			PRINTC("Sent to fvm%d\n", i); 
		} 
	} else {
		cos_asnd(VM_CAPTBL_SELF_IOASND_BASE, 1);
		PRINTC("Sent to vm0\n"); 
	}
	PRINTC("Test Done\n");
}
