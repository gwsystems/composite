#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include <thd.h>
#include "stm32f7xx_hal.h"

extern u32_t comp1_stack[1000];

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline u32_t
chal_cpu_fault_errcode(struct pt_regs *r) { return r->r14_lr; }

static inline u32_t
chal_cpu_fault_ip(struct pt_regs *r) { return r->r15_pc; }

extern unsigned int _c1_sdata;
extern unsigned int _c1_edata;
extern unsigned int __c1_bss_start__;
extern unsigned int __c1_bss_end__;

static inline void //__attribute__((optimize("O3")))
chal_user_upcall(void *ip, u16_t tid)
{
	long long total_swt_cycles = 0;
	long long start_swt_cycles = 0, end_swt_cycles = 0;
	int i;
	char str[100];

	/* TODO:remove MPU testing code */
//	rdtscll(start_swt_cycles);
//
//	for (i = 0 ; i < 10000 ; i++)
//	{
//		/* Try to do some arbitrary programming to MPU */
//		__asm__ __volatile__(//"push {r0-r9}; \n\t"
//				             "ldr r1,=0x20000000 \n\t" \
//				             "ldr r0,=0xE000ED9C \n\t" \
//				             "ldm r1!,{r2-r9}; \n\t" \
//							 "stm r0!,{r2-r9}; \n\t" \
//							 "ldm r1!,{r2-r9}; \n\t" \
//							 "stm r0!,{r2-r9}; \n\t" \
//							 //"pop {r0-r9}; \n\t"
//							 "dsb \n\t"
//							 :
//							 :
//							 : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","memory", "cc");
//	}
//	rdtscll(end_swt_cycles);
//	total_swt_cycles = (end_swt_cycles - start_swt_cycles);
//
//	sprintf(str,"MPU(Total:%d/Iter:%d ):%d\n",
//			(int)total_swt_cycles, (int)10000, (int)(total_swt_cycles / 10000));
//
//	LCD_ShowString(10,40,260,32,12,str);
//	while(1);
//
//	rdtscll(start_swt_cycles);
//	for (i = 0 ; i < 10000 ; i++)
//	{
	/* TODO:Preliminary MPU tests, merge into pgtbl operations later
	MPU_Region_InitTypeDef MPU_Initure;
	HAL_MPU_Disable();								           //Disable MPU before configuring it and enable it after the configuration
	MPU_Initure.Enable=MPU_REGION_ENABLE;			           //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER0;			           //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)&(_c1_sdata);	           //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_64KB;				       //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                         //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_FULL_ACCESS; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_CACHEABLE;              //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	MPU_Initure.Enable=MPU_REGION_ENABLE;			           //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER1;			           //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)0x08000000;	               //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_2MB;				       //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                         //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_FULL_ACCESS; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_CACHEABLE;              //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	MPU_Initure.Enable=MPU_REGION_ENABLE;			           //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER2;			           //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)0xE0000000;	               //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_512MB;				       //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                         //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_FULL_ACCESS; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_NOT_CACHEABLE;          //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);			           //Enable the MPU without 2820 with 2733
//	}
//	rdtscll(end_swt_cycles);
//    total_swt_cycles = (end_swt_cycles - start_swt_cycles) / 2LL;
//
//	sprintf(str,"MPU(Total:%d/Iter:%d ):%d\n",
//			(int)total_swt_cycles, (int)10000, (int)(total_swt_cycles / 10000));
//
//	LCD_ShowString(10,40,260,32,12,str);
//	while(1);*/

	/* Now we switch the execution to user space, and begin to use the PSP stack pointer */
	__asm__ __volatile__("ldr r0,=comp1_stack \n\t"
			     "add r0,#0x1000 \n\t"
			     "msr psp,r0 \n\t"
			     "mov r0,#0x02 \n\t"
			     "msr control,r0 \n\t"
			     "mov r0,#0x00 \n\t"
			     "mov r1,#0x00 \n\t"
	    		     ::: "memory", "cc");

    void(*ptr)(void)=(void(*)(void))(ip);
    /* Just call the component, and never returns */
    ptr();
}

static inline void
chal_cpuid(int code, u32_t *a, u32_t *b, u32_t *c, u32_t *d)
{  }

#endif /* CHAL_CPU_H */
