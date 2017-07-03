/* Hardware related part of Cortex-M */
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

#include "cos_component.h"
#include "mem_layout.h"

#include "lcd.h"

volatile unsigned long long rdtsc_sim=1;

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
MPU_Init(void)
{
	/* Enable MPU access to all the code section, and the peripheral section */
	MPU_Region_InitTypeDef MPU_Initure;
	HAL_MPU_Disable();

	/* The code section */
	MPU_Initure.Enable=MPU_REGION_ENABLE;			   //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER4;			   //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)0x08000000;	           //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_2MB;		           //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                         //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_FULL_ACCESS; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_CACHEABLE;              //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	/* The SDRAM section - RW */
	MPU_Initure.Enable=MPU_REGION_ENABLE;			   //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER5;			   //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)0x60000000;	           //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_1GB;		           //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                         //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_FULL_ACCESS; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_CACHEABLE;              //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	/* The system peripheral section */
	MPU_Initure.Enable=MPU_REGION_ENABLE;			   //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER6;			   //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)0xE0000000;	           //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_512MB;			   //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                         //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_PRIV_RW_URO; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_NOT_CACHEABLE;          //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);			   //Enable the MPU without 2820 with 2733
}

void hw_timer_set(cycles_t cycles)
{
	TIM3_Init(cycles/1024/2);
}

void hw_timer_disable(void)
{
	/* Disable timer 3 clock */
	HAL_TIM_Base_Stop_IT(&TIM3_Handler);
	__HAL_RCC_TIM3_CLK_DISABLE();
	/* Clear interrupt pending bit, because we used EGR to update the registers */
	__HAL_TIM_CLEAR_IT(&TIM3_Handler, TIM_IT_UPDATE);
}

extern void SDRAM_Init(void);

void hw_init(void)
{
	timer_init();

	SDRAM_Init();
	LCD_Init();
	GPIO_Init();

	POINT_COLOR=RED;
	LCD_Clear(GREEN);
	MPU_Init();
}
