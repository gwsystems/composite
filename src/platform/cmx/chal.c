#include <chal.h>
#include <user/cos_types.h>
#include "kernel.h"
#include "mem_layout.h"

u32_t free_thd_id = 1;
char timer_detector[PAGE_SIZE] PAGE_ALIGNED;
extern void *cos_kmem, *cos_kmem_base;

paddr_t chal_kernel_mem_pa;

void *
chal_pa2va(paddr_t address)
{ return (void*)(address/*+COS_MEM_KERN_START_VA*/); }

paddr_t
chal_va2pa(void *address)
{ return (paddr_t)(address/*-COS_MEM_KERN_START_VA*/); }

void *
chal_alloc_kern_mem(int order)
{ return mem_kmem_start(); }

void chal_free_kern_mem(void *mem, int order) {}

int
chal_attempt_arcv(struct cap_arcv *arcv)
{ return 0; }

void chal_send_ipi(int cpuid) {}

void
chal_khalt(void)
{ khalt(); }

void
chal_init(void)
{ chal_kernel_mem_pa = chal_va2pa(mem_kmem_start()); }

/* Generate the RASR data */
u32_t
chal_MPU_gen_RASR(struct pgtbl* table, u32_t size_order, u32_t num_order)
{
	u32_t RASR;
	u32_t count;

	/* Get the SRD part first */
	RASR=0;

	/* How many entries does the page table contain? */
	if(num_order==1)
	{
		for(count=0;count<2;count++)
		{
			if(((table->data[count]&PGTBL_PRESENT)!=0)&&((table->data[count]&PGTBL_LEAF)!=0))
				RASR|=(((u32_t)0x0F)<<(count*4+8));
		}
	}
	else if(num_order==2)
	{
		for(count=0;count<2;count++)
		{
			if(((table->data[count]&PGTBL_PRESENT)!=0)&&((table->data[count]&PGTBL_LEAF)!=0))
				RASR|=(((u32_t)0x03)<<(count*2+8));
		}
	}
	else
	{
		for(count=0;count<8;count++)
		{
			if(((table->data[count]&PGTBL_PRESENT)!=0)&&((table->data[count]&PGTBL_LEAF)!=0))
				RASR|=(((u32_t)1)<<(count+8));
		}
	}
	if(RASR==0)
		return 0;

	RASR=CMX_MPU_SRDCLR&(~RASR);
	RASR|=CMX_MPU_SZENABLE;
	/* Because what we type is always read-write, thus the RASR is always RW */
	RASR|=CMX_MPU_RW;
	/* Can we fetch instructions from there? - always cannot, we do not allow data execution */
	RASR|=CMX_MPU_XN;
	/* Is the area cacheable? - because what we type is SRAM, so it is always accessible */
	RASR|=CMX_MPU_CACHEABLE;
	/* Is the area bufferable? - always bufferable */
	RASR|=CMX_MPU_BUFFERABLE;
	/* What is the region size? */
	RASR|=CMX_MPU_REGIONSIZE(size_order, num_order);
        /* 1303fc1b */
	return RASR;
}

u32_t
chal_MPU_clear(struct pgtbl* table, u32_t start_addr, u32_t size_order)
{
	u32_t count;

	/* There are only 4 regions at the toplevel which is configurable */
	for(count=0;count<4;count++)
	{
		if((table->data[9]&(((u32_t)1)<<count))!=0)
		{
			/* We got one MPU region valid here */
			if((CMX_MPU_ADDR(table->data[count*2])==start_addr)&&
			   (CMX_MPU_SZORD(table->data[count*2+1])==size_order))
			{
				/* Clean it up and return */
				table->data[count*2]=CMX_MPU_VALID|count;
				table->data[count*2+1]=0;
				table->data[9]&=~(((u32_t)1)<<count);
				return 0;
			}
		}
	}

	return 0;
}

u32_t
chal_MPU_add(struct pgtbl* table, u32_t start_addr, u32_t size_order, u32_t MPU_RASR)
{
	u32_t count;
	/* The last empty slot */
	u32_t last_empty;

	/* Set these values to some overrange value */
	last_empty=16;

	/* We only set 4 regions */
	for(count=0;count<4;count++)
	{
		if((table->data[9]&(((u32_t)1)<<count))!=0)
		{
			/* We got one MPU region valid here */
			if((CMX_MPU_ADDR(table->data[count*2])==start_addr)&&   /* RBAR */
			   (CMX_MPU_SZORD(table->data[count*2+1])==size_order)) /* RASR */
			{
				/* Update the RASR - all flag changes except static are reflected here */
				table->data[count*2+1]=MPU_RASR;
				return 0;
			}
		}
		else
			last_empty=count;
	}

	/* Update unsuccessful, we didn't find any match. We will need a new slot.
	 * See if there are any new empty slots that we can use.
	 * See if the last empty slot is 0. If yes, we can only map dynamic pages */
	if(last_empty!=16)
	{
		/* Put the data to this slot */
		table->data[9]|=((u32_t)1)<<(last_empty);
		table->data[last_empty*2]=CMX_MPU_ADDR(start_addr)|CMX_MPU_VALID|last_empty;
		table->data[last_empty*2+1]=MPU_RASR;

		return 0;
	}

	/* All effort is futile. we report failure */
	return -1;
}

u32_t chal_MPU_update(struct pgtbl* table, u32_t Op_Flag)
{
	struct pgtbl* toplevel;
	u32_t MPU_RASR;

	/* Get the tables - does not allow update in the toplevel itself */
	if((table->type_addr&COS_PGTBL_TABLE)==0)
		return -1;
	else
	{
		/* We have a top-level */
		toplevel=(struct pgtbl*)(table->data[9]);
		if(toplevel==0)
			return -1;
	}

	if(Op_Flag==0)
	{
		/* Clear the metadata - this function will never fail */
		chal_MPU_clear(toplevel,COS_PGTBL_STARTADDR(table->type_addr), COS_PGTBL_SIZEORDER(table->type_addr));
	}
	else
	{
		/* See if the RASR contains anything */
		MPU_RASR=chal_MPU_gen_RASR(table,  COS_PGTBL_SIZEORDER(table->type_addr), COS_PGTBL_NUMORDER(table->type_addr));
		if(MPU_RASR==0)
		{
			/* All pages are unmapped. Clear this from the MPU data */
			chal_MPU_clear(toplevel, COS_PGTBL_STARTADDR(table->type_addr), COS_PGTBL_SIZEORDER(table->type_addr));
		}
		else
		{
			/* At least one of the pages are there. Map it */
			if(chal_MPU_add(toplevel, COS_PGTBL_STARTADDR(table->type_addr), COS_PGTBL_SIZEORDER(table->type_addr), MPU_RASR)!=0)
				return -1;
		}
	}

	return 0;
}

