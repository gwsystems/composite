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

#include "user/cos_component.h"
#include "mem_layout.h"

#include "lcd.h"

struct mem_layout glb_memlayout;
volatile unsigned long long rdtsc_sim=1;

/* We have 64 pages in the system now. Thus, the 512kB will be cut into 8kB pages. All the access should be aligned to 8kB */
extern u32_t Stack_Mem[ALL_STACK_SZ];
/* These unused memory should be assigned to the last of the space */
extern unsigned long Memory_Used[65536];

extern void cos_init(void);
extern void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3);

extern unsigned int __utmem_end__;
void
kern_memory_setup(void)
{
	/* 0x10000:kernel|0x10000:mod|0x20000:heap|utmem */
	/* Some memory initialization with fixed parameters */
	/* This is the whole RAM range of the MCU */
	glb_memlayout.kern_end=0x20000000;/*COS_MEM_KERN_PA+COS_MEM_KERN_PA_SZ*//*COS_MEM_COMP_START_VA-0x1000*/;
	/* Currently we dont have any modules, mod_end unused */
	glb_memlayout.mod_start=((unsigned long)Memory_Used)+0x00000000;
	glb_memlayout.mod_end=((unsigned long)Memory_Used)+0x00010000;
	glb_memlayout.bootc_entry=glb_memlayout.bootc_vaddr=cos_upcall_fn;//cos_init;
	glb_memlayout.kern_boot_heap=((unsigned long)Memory_Used)+0x00020000;
	glb_memlayout.kmem_end=&__utmem_end__;//0x2007FFFF;
	glb_memlayout.allocs_avail=1;

	/* Validate the memory layout. */
	assert(mem_boot_start()  >= mem_kmem_start());
	assert(mem_kmem_start()  == mem_bootc_start());
	assert(mem_kmem_end()    >= mem_boot_end());
	assert(mem_utmem_start() >= mem_kmem_start());
	assert(mem_utmem_start() >= mem_boot_end());
	assert(mem_utmem_end()   <= mem_kmem_end());
	assert(mem_bootc_entry() - mem_bootc_vaddr() <= mem_bootc_end() - mem_bootc_start());
}

TIM_HandleTypeDef TIM3_Handler;
void TIM3_Init(u16 Time)
{
	/* General purpose timer 3 */
	TIM3_Handler.Instance=TIM3;
	/* Prescaler value 1024 */
	TIM3_Handler.Init.Prescaler=1024-1;
	/* Counting up */
	TIM3_Handler.Init.CounterMode=TIM_COUNTERMODE_UP;
	/* Autoreload value */
	TIM3_Handler.Init.Period=Time;
	/* Clock division factor - 1. This TIM2 is mounted on HCLK/2 clock tree, thus works at SYSCLK/2 speed */
	TIM3_Handler.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	/* Disable the timer */
	HAL_TIM_Base_DeInit(&TIM3_Handler);
	chal_timer_disable();
	/* Low-level init and enable interrupts */
	HAL_TIM_Base_Init(&TIM3_Handler);//, TIM_OPMODE_SINGLE);
	/* Clear interrupt pending bit, because we used EGR to update the registers */
	__HAL_TIM_CLEAR_IT(&TIM3_Handler, TIM_IT_UPDATE);
	//HAL_TIM_Base_Init();
	HAL_TIM_Base_Start_IT(&TIM3_Handler);
}

/* The low-level driver which will be called by HAL_TIM_Base_Init to set the interrupt priority */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
	if(htim->Instance==TIM3)
	{
		/* Set the interrupt priority */
		HAL_NVIC_SetPriority(TIM3_IRQn,1,3);
		/* Enable timer 3 interrupt */
		HAL_NVIC_EnableIRQ(TIM3_IRQn);
		/* Enable timer 3 clock */
		__HAL_RCC_TIM3_CLK_ENABLE();
	}
}

/* The low-level driver which will be called by HAL_TIM_Base_Init to set the interrupt priority */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    if(htim->Instance==TIM3)
	{
		/* Set the interrupt priority */
		//HAL_NVIC_SetPriority(TIM3_IRQn,1,3);
		/* Enable timer 3 interrupt */
		HAL_NVIC_DisableIRQ(TIM3_IRQn);
	}
}

/* The actual tim3 handler */
struct pt_regs* timer_regs;
void tim3irqhandler(struct pt_regs* regs)
{
	timer_regs=regs;
	HAL_TIM_IRQHandler(&TIM3_Handler);
}

extern int timer_process(struct pt_regs *regs);

/* The call-back function */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim==(&TIM3_Handler))
	{
		/* We are using one-shot, so disable the timer now */
		HAL_TIM_Base_DeInit(&TIM3_Handler);
		chal_timer_disable();
		timer_process(timer_regs);
	}
}


/* Systick is used to simulate the rdtsc. do not touch */
void
timer_init(void)
{
	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;
	SysTick->LOAD=0xFFFFFF;/*1000;*/
	SysTick->VAL=0x00;
	NVIC_SetPriority(SysTick_IRQn,0x00);
	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void)
{
	rdtsc_sim++;
}

void __attribute__((optimize("O0"))) delay_us(u32 nus)
{
	u32 ticks;
	u32 told,tnow,tcnt=0;
	u32 reload=SysTick->LOAD;
	ticks=nus*216;
	told=SysTick->VAL;
	while(1)
	{
		tnow=SysTick->VAL;
		if(tnow!=told)
		{
			if(tnow<told)tcnt+=told-tnow;
			else tcnt+=reload-tnow+told;
			told=tnow;
			if(tcnt>=ticks)break;
		}
	};
}

void
GPIO_Init(void)
{
	/* We initialize the GPIOB 0,1, push-pull */
	GPIO_InitTypeDef GPIO_Initure;
	__HAL_RCC_GPIOB_CLK_ENABLE();
	GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_1;
	GPIO_Initure.Mode=GPIO_MODE_OUTPUT_PP;
	GPIO_Initure.Pull=GPIO_PULLUP;
	GPIO_Initure.Speed=GPIO_SPEED_HIGH;
	HAL_GPIO_Init(GPIOB,&GPIO_Initure);
	/* Turn off the LED */
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1,GPIO_PIN_SET);
}

void
LED_set(void)
{
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1,GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);
}

void
LED_reset(void)
{
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1,GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_RESET);
}

void
main(void)
{
	/* The kernel image area */
	timer_init();

	SDRAM_Init();
	LCD_Init();
	GPIO_Init();

	POINT_COLOR=RED;
	LCD_Clear(GREEN);

	kern_memory_setup();

	chal_init();
	cap_init();
	ltbl_init();
	retype_tbl_init();
	comp_init();
	thd_init();

	/* Retype anything between mod_start and mod_end to user mem */
	for(unsigned long i=0;i<0x4000;i+=1024)
	{
		assert(retypetbl_retype2user(&Memory_Used[i])==0);
	}

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

void assert_failed(uint8_t* file, uint32_t line)
{

}

void
chal_tls_update(vaddr_t vaddr)
{

}

void
chal_timer_set(cycles_t cycles)
{
	TIM3_Init(cycles/1024/2);
}

void
chal_timer_disable(void)
{
	/* Disable timer 3 clock */
	HAL_TIM_Base_Stop_IT(&TIM3_Handler);
	__HAL_RCC_TIM3_CLK_DISABLE();
	/* Clear interrupt pending bit, because we used EGR to update the registers */
	__HAL_TIM_CLEAR_IT(&TIM3_Handler, TIM_IT_UPDATE);
}

int
chal_cyc_usec(void)
{ return 216; }
