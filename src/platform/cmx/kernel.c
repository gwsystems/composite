#include "assert.h"
#include "kernel.h"
#include "string.h"

#include <captbl.h>
#include <retype_tbl.h>
#include <component.h>
#include <thd.h>
#include "stm32f7xx_hal.h"
#include "core_cm7.h"
#include "boot_comp.h"

int var=0;

u32_t Test_Array[1024] __attribute__((aligned(0x1000)));

void
kern_memory_setup(void)
{
	/* Some memory initialization with fixed parameters */
}

void
timer_init(void)
{
	/*
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;
	SysTick->LOAD=1000;
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk;
*/
	/* var=SysTick_Config(1000); */
	/* The systick IRQ has a very high priority */
	//NVIC_SetPriority(SysTick_IRQn,0x00);
}

u8_t MPU_Set_Protection(u32_t baseaddr,u32_t size,u32_t rnum,u32_t ap)
{
	MPU_Region_InitTypeDef MPU_Initure;

	HAL_MPU_Disable();								        //Disable MPU before configuring it and enable it after the configuration

	MPU_Initure.Enable=MPU_REGION_ENABLE;			        //Enable this area
	MPU_Initure.Number=rnum;			                    //Set the protection area
	MPU_Initure.BaseAddress=baseaddr;	                    //Set the base address
	MPU_Initure.Size=size;				                    //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                      //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)ap;		            //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	//Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;       //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_NOT_CACHEABLE;       //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;         //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                     //Initialize the MPU
	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);			        //Enable the MPU
    return 0;
}

void
mpu_init(void)
{
	MPU_Set_Protection((u32_t)Test_Array, MPU_REGION_SIZE_1KB,MPU_REGION_NUMBER0,MPU_REGION_FULL_ACCESS);//MPU_REGION_PRIV_RO_URO
	Test_Array[0]=0;
	Test_Array[1]=1;
}

void MemManage_Handler(void)
{
	var=1;
}

void SysTick_Handler(void)
{
	var=1;
}

extern void cos_init(void);

void
main(void)
{
	cos_init();
	/* Currently these are fixed for cortex-Mx */
	//timer_init();
    mpu_init();
	/* Test the timer */
	/*while(1)
	{
		if(var==1)
			var=0;
	}*/

	kern_memory_setup();

	chal_init();
	cap_init();
	ltbl_init();
	retype_tbl_init();
	comp_init();
	thd_init();
	//paging_init();

	kern_boot_comp();
	kern_boot_upcall();
	/* should not get here... */
	khalt();
}

void
khalt(void)
{
	printk("Shutting down...\n");
	while(1);
}

void
printk(const char *fmt, ...)
{

}

void
chal_tls_update(vaddr_t vaddr)
{

}

void
chal_timer_set(cycles_t cycles)
{  }

void
chal_timer_disable(void)
{  }

int
chal_cyc_usec(void)
{ return 1000; }
