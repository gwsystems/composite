#include "kernel.h"
#include "comp.h"
#include <pgtbl.h>
#include <inv.h>

#define N_PHYMEM_PAGES COS_MAX_MEMORY /* # of physical pages available */

u8_t boot_comp_captbl[PAGE_SIZE*BOOT_CAPTBL_NPAGES] PAGE_ALIGNED;
extern u8_t *boot_comp_pgd;
u8_t *boot_comp_pte_vm;
u8_t *boot_comp_pte_km;
u8_t *boot_comp_pte_pm;
void *cos_kmem, *cos_kmem_base;
unsigned long sys_llbooter_sz;    /* how many pages is the llbooter? */
void *llbooter_kern_mapping;

int
kern_boot_comp(struct spd_info *spd_info)
{
        int ret;
        struct captbl *ct, *ct0;
        pgtbl_t pt, pt0;
        unsigned int i;
        void *kmem_base_pa;
	u32_t cr0;

        ct = captbl_create(boot_comp_captbl);
        assert(ct);

        /* expand the captbl to use multiple pages. */
        for (i = 1; i < BOOT_CAPTBL_NPAGES; i++) {
                captbl_init(boot_comp_captbl + i * PAGE_SIZE, 1);
                ret = captbl_expand(ct, (PAGE_SIZE*i - PAGE_SIZE/2)/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + PAGE_SIZE*i);
                assert(!ret);
                captbl_init(boot_comp_captbl + PAGE_SIZE + PAGE_SIZE/2, 1);
                ret = captbl_expand(ct, (PAGE_SIZE*i)/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + PAGE_SIZE*i + PAGE_SIZE/2);
                assert(!ret);
        }

        kmem_base_pa = chal_va2pa(cos_kmem_base);
        ret = retypetbl_retype2kern(kmem_base_pa);
        assert(ret == 0);

        //boot_comp_pgd = cos_kmem_base;
        cos_kmem_base = boot_comp_pgd;
        boot_comp_pte_vm = cos_kmem_base + PAGE_SIZE;
        boot_comp_pte_pm = cos_kmem_base + 2 * PAGE_SIZE;
        boot_comp_pte_km = cos_kmem_base + 3 * PAGE_SIZE;

        cos_kmem += 4*PAGE_SIZE;

        pt = pgtbl_create(boot_comp_pgd, chal_va2pa(boot_comp_pgd));

//      printk("pt %x, our pgd %x (%x), pte %x (%x)\n", pt, boot_comp_pgd, __pa(boot_comp_pgd), boot_comp_pte_vm, __pa(boot_comp_pte_vm));
        assert(pt);
        pgtbl_init_pte(boot_comp_pte_vm);
        pgtbl_init_pte(boot_comp_pte_pm);
        pgtbl_init_pte(boot_comp_pte_km);

        if (captbl_activate_boot(ct, BOOT_CAPTBL_SELF_CT)) cos_throw(err, -1);
        if (sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET)) cos_throw(err, -2);
        if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, pt, 0)) cos_throw(err, -3);
        if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_BOOTVM_PTE, (pgtbl_t)boot_comp_pte_vm, 1)) cos_throw(err, -4);
        if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_KM_PTE, (pgtbl_t)boot_comp_pte_km, 1)) cos_throw(err, -5);
        if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_PHYSM_PTE, (pgtbl_t)boot_comp_pte_pm, 1)) cos_throw(err, -6);

        /* construct the page tables */
        if (cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_BOOTVM_PTE, BOOT_MEM_VM_BASE)) cos_throw(err, -7);
        if (cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_KM_PTE, BOOT_MEM_KM_BASE)) cos_throw(err, -8);
        if (cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_PHYSM_PTE, BOOT_MEM_PM_BASE)) cos_throw(err, -9);

        sys_llbooter_sz = spd_info->mem_size / PAGE_SIZE;
        if (spd_info->mem_size % PAGE_SIZE) sys_llbooter_sz++;

        /* add the component's virtual memory at 4MB (1<<22) using "physical memory" starting at cos_kmem */
        for (i = 0 ; i < sys_llbooter_sz; i++) {
                u32_t addr = (u32_t)(chal_va2pa(cos_kmem) + i*PAGE_SIZE);
                u32_t flags;
                if ((addr - (u32_t)kmem_base_pa) % RETYPE_MEM_SIZE == 0) {
                        ret = retypetbl_retype2kern((void *)addr);
                        if (ret) {
                                printk("Retype paddr %x failed when loading llbooter. ret %d\n", addr, ret);
                                cos_throw(err, -10);
                        }
                }

                if (pgtbl_mapping_add(pt, BOOT_MEM_VM_BASE + i*PAGE_SIZE, addr, PGTBL_USER_DEF)) {
                        printk("Mapping llbooter %x failed!\n", addr);
                        cos_throw(err, -11);
                }
                assert(chal_pa2va((void *)addr) == pgtbl_lkup(pt, BOOT_MEM_VM_BASE+i*PAGE_SIZE, &flags));
        }

        llbooter_kern_mapping = cos_kmem;
        cos_kmem += sys_llbooter_sz*PAGE_SIZE;

        /* Round to the next memory retype region. Adjust based on
         * offset from cos_kmem_base*/
        if ((cos_kmem - cos_kmem_base) % RETYPE_MEM_SIZE != 0)
                cos_kmem += (RETYPE_MEM_SIZE - (cos_kmem - cos_kmem_base) % RETYPE_MEM_SIZE);

        /* add the remaining kernel memory @ 1.5GB*/
        /* printk("mapping from kmem %x\n", cos_kmem); */
        for (i = 0; (int)i < (COS_KERNEL_MEMORY - (cos_kmem - cos_kmem_base)/PAGE_SIZE); i++) {
                u32_t addr = (u32_t)(chal_va2pa(cos_kmem) + i*PAGE_SIZE);
                u32_t flags;

		printk("pgtbl_cosframe_add(%08x, %08x, %08x, %08x)\n", pt, BOOT_MEM_KM_BASE + i*PAGE_SIZE, addr, PGTBL_COSFRAME | PGTBL_USER_DEF);
                if (pgtbl_cosframe_add(pt, BOOT_MEM_KM_BASE + i*PAGE_SIZE,
                                      addr, PGTBL_COSFRAME | PGTBL_USER_DEF)) cos_throw(err, -12); /* FIXME: shouldn't be accessible */
                assert(chal_pa2va((void *)addr) == pgtbl_lkup(pt, BOOT_MEM_KM_BASE+i*PAGE_SIZE, &flags));
        }

        if (COS_MEM_START % RETYPE_MEM_SIZE != 0) {
                printk("Physical memory start address (%d) not aligned by retype_memory size (%lu).",
                       COS_MEM_START, RETYPE_MEM_SIZE);
                cos_throw(err, -13);
        }

        /* add the system's physical memory at address 2GB */
        for (i = 0 ; i < N_PHYMEM_PAGES ; i++) {
                u32_t addr = COS_MEM_START + i*PAGE_SIZE;
                u32_t flags;

                /* Make the memory accessible so we can populate memory without retyping. */
                if (pgtbl_cosframe_add(pt, BOOT_MEM_PM_BASE + i*PAGE_SIZE,
                                      addr, PGTBL_COSFRAME | PGTBL_USER_DEF)) cos_throw(err, -14);
                assert(chal_pa2va((void *)addr) == pgtbl_lkup(pt, BOOT_MEM_PM_BASE+i*PAGE_SIZE, &flags));
        }

        printk("Enabling paging\n");
        pgtbl_update(pt);
        printk("Switching cr0\n");
        asm volatile("mov %%cr0, %0" : "=r"(cr0));
        cr0 |= 0x80000000;
        asm volatile("mov %0, %%cr0" : : "r"(cr0));
        printk("Switched\n");

        return 0;
err:
        printk("Activating data-structure failed (%d).\n", ret);
        return ret;
}
