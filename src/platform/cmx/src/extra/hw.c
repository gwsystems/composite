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

#define PRY_DEBUG_NO_TIMERS

extern volatile unsigned long long rdtsc_val;

TIM_HandleTypeDef TIM3_Handler;
TIM_HandleTypeDef TIM4_Handler;

void
serial_init(u32 baud)
{
//	RCC_OscInitTypeDef RCC_OscInitStructure;
//	RCC_ClkInitTypeDef RCC_ClkInitStructure;
//
//	/* Set Interrupt Group Priority */ \
//	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2); \
//
////	/* Set the clock tree in the system */
////	/* Enble power regulator clock, and configure voltage scaling function */
////	__HAL_RCC_PWR_CLK_ENABLE();
////	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
////	/* Initialize the oscillator */
////	RCC_OscInitStructure.OscillatorType=RCC_OSCILLATORTYPE_HSE;
////	RCC_OscInitStructure.HSEState=RCC_HSE_ON;
////	RCC_OscInitStructure.PLL.PLLState=RCC_PLL_ON;
////	RCC_OscInitStructure.PLL.PLLSource=RCC_PLLSOURCE_HSE;
////	/* Fpll=Fin/PLLM*PLLN, Fsys=Fpll/PLLP, Fperiph=Fpll/PLLQ */
////	RCC_OscInitStructure.PLL.PLLM=25;
////	RCC_OscInitStructure.PLL.PLLN=432;
////	RCC_OscInitStructure.PLL.PLLP=2;
////	RCC_OscInitStructure.PLL.PLLQ=9;
////	assert(HAL_RCC_OscConfig(&RCC_OscInitStructure)==HAL_OK);
////	/* Overdrive to 216MHz */
////	assert(HAL_PWREx_EnableOverDrive()==HAL_OK);
//
//	/* HCLK,PCLK1 & PCLK2 configuration */
//	RCC_ClkInitStructure.ClockType=(RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2);
//	RCC_ClkInitStructure.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK;
//	RCC_ClkInitStructure.AHBCLKDivider=RCC_SYSCLK_DIV1;
//	RCC_ClkInitStructure.APB1CLKDivider=RCC_HCLK_DIV4;
//	RCC_ClkInitStructure.APB2CLKDivider=RCC_HCLK_DIV2;
//	/* Flash latency = 7us, 8 CPU cycles */
//	assert(HAL_RCC_ClockConfig(&RCC_ClkInitStructure,FLASH_LATENCY_7)==HAL_OK);
//	HAL_SuspendTick();
//
//	/* Finally, cache operations */
//	/* Cache/Flash ART enabling */
//	SCB_EnableICache();
//	SCB_EnableDCache(); SCB->CACR|=1<<2;
//	__HAL_FLASH_ART_ENABLE();
//
	GPIO_InitTypeDef GPIO_Init;
	UART_HandleTypeDef UART1_Handle;
	/* Clock enabling */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_USART1_CLK_ENABLE();
	/* UART IO initialization */
	GPIO_Init.Pin=GPIO_PIN_9;
	GPIO_Init.Mode=GPIO_MODE_AF_PP;
	GPIO_Init.Pull=GPIO_PULLUP;
	GPIO_Init.Speed=GPIO_SPEED_FAST;
	GPIO_Init.Alternate=GPIO_AF7_USART1;
	HAL_GPIO_Init(GPIOA,&GPIO_Init);
	GPIO_Init.Pin=GPIO_PIN_10;
	HAL_GPIO_Init(GPIOA,&GPIO_Init);
//	/* UART initialization */
//	UART1_Handle.Instance=USART1;
//	UART1_Handle.Init.BaudRate=baud;
//	UART1_Handle.Init.WordLength=UART_WORDLENGTH_8B;
//	UART1_Handle.Init.StopBits=UART_STOPBITS_1;
//	UART1_Handle.Init.Parity=UART_PARITY_NONE;
//	UART1_Handle.Init.HwFlowCtl=UART_HWCONTROL_NONE;
//	UART1_Handle.Init.Mode=UART_MODE_TX;
//	HAL_UART_Init(&UART1_Handle);

	u32	temp;
	u32 pclk2=108;
	u32 bound=115200;
	temp=(pclk2*1000000+bound/2)/bound;	//µÃµœUSARTDIV@OVER8=0,²ÉÓÃËÄÉáÎåÈëŒÆËã
	RCC->AHB1ENR|=1<<0;   	//Ê¹ÄÜPORTA¿ÚÊ±ÖÓ
	RCC->APB2ENR|=1<<4;  	//Ê¹ÄÜŽ®¿Ú1Ê±ÖÓ
//	GPIO_Set(GPIOA,PIN9|PIN10,GPIO_MODE_AF,GPIO_OTYPE_PP,GPIO_SPEED_50M,GPIO_PUPD_PU);//PA9,PA10,žŽÓÃ¹ŠÄÜ,ÉÏÀ­Êä³ö
// 	GPIO_AF_Set(GPIOA,9,7);	//PA9,AF7
//	GPIO_AF_Set(GPIOA,10,7);//PA10,AF7
	//²šÌØÂÊÉèÖÃ
 	USART1->BRR=temp; 		//²šÌØÂÊÉèÖÃ@OVER8=0
	USART1->CR1=0;		 	//ÇåÁãCR1ŒÄŽæÆ÷
	USART1->CR1|=0<<28;	 	//ÉèÖÃM1=0
	USART1->CR1|=0<<12;	 	//ÉèÖÃM0=0&M1=0,Ñ¡Ôñ8Î»×Ö³€
	USART1->CR1|=0<<15; 	//ÉèÖÃOVER8=0,16±¶¹ý²ÉÑù
	USART1->CR1|=1<<3;  	//Ž®¿Ú·¢ËÍÊ¹ÄÜ
#if EN_USART1_RX		  	//Èç¹ûÊ¹ÄÜÁËœÓÊÕ
	//Ê¹ÄÜœÓÊÕÖÐ¶Ï
	USART1->CR1|=1<<2;  	//Ž®¿ÚœÓÊÕÊ¹ÄÜ
	USART1->CR1|=1<<5;    	//œÓÊÕ»º³åÇø·Ç¿ÕÖÐ¶ÏÊ¹ÄÜ
	MY_NVIC_Init(3,3,USART1_IRQn,2);//×é2£¬×îµÍÓÅÏÈŒ¶
#endif
	USART1->CR1|=1<<0;  	//Ž®¿ÚÊ¹ÄÜ
}

void TIM3_Init(u16 Time)
{
#ifndef PRY_DEBUG_NO_TIMERS
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
	/* We never repeat */
	TIM3_Handler.Init.RepetitionCounter=0;
	/* Disable the timer */
	HAL_TIM_Base_DeInit(&TIM3_Handler);
	chal_timer_disable();
	/* Low-level init and enable interrupts */
	HAL_TIM_Base_Init(&TIM3_Handler);//, TIM_OPMODE_SINGLE);
	/* Clear interrupt pending bit, because we used EGR to update the registers */
	__HAL_TIM_CLEAR_IT(&TIM3_Handler, TIM_IT_UPDATE);
	HAL_TIM_Base_Start_IT(&TIM3_Handler);
#endif
}

/* The low-level driver which will be called by HAL_TIM_Base_Init to set the interrupt priority */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
	if(htim->Instance==TIM3) {
		/* Set the interrupt priority */
		HAL_NVIC_SetPriority(TIM3_IRQn,1,3);
		/* Enable timer 3 interrupt */
		HAL_NVIC_EnableIRQ(TIM3_IRQn);
		/* Enable timer 3 clock */
		__HAL_RCC_TIM3_CLK_ENABLE();
	}
	/* This path will be run only once */
	else if(htim->Instance==TIM4) {
		/* Set the interrupt priority */
		HAL_NVIC_SetPriority(TIM4_IRQn,1,2);
		/* Enable timer 4 interrupt */
		HAL_NVIC_EnableIRQ(TIM4_IRQn);
		/* Enable timer 4 clock */
		__HAL_RCC_TIM4_CLK_ENABLE();
	}
}

/* The low-level driver which will be called by HAL_TIM_Base_Init to set the interrupt priority */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
	if(htim->Instance==TIM3) {
		/* Set the interrupt priority */
		//HAL_NVIC_SetPriority(TIM3_IRQn,1,3);
		/* Enable timer 3 interrupt */
		HAL_NVIC_DisableIRQ(TIM3_IRQn);
	}
	else if(htim->Instance==TIM4) {
		HAL_NVIC_DisableIRQ(TIM4_IRQn);
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
	if(htim==(&TIM3_Handler)) {
		/* We are using one-shot, so disable the timer now */
		HAL_TIM_Base_DeInit(&TIM3_Handler);
		chal_timer_disable();
		timer_process(timer_regs);
	}
}

void TIM4_Init(u16 Time)
{
#ifndef PRY_DEBUG_NO_TIMERS
	/* General purpose timer 4 */
	TIM4_Handler.Instance=TIM4;
	/* Prescaler value 0 - not prescaled */
	TIM4_Handler.Init.Prescaler=0;
	/* Counting up */
	TIM4_Handler.Init.CounterMode=TIM_COUNTERMODE_UP;
	/* Autoreload value */
	TIM4_Handler.Init.Period=Time;
	/* Clock division factor - 1. This TIM4 is mounted on HCLK/2 clock tree, thus works at SYSCLK/2 speed */
	TIM4_Handler.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	/* We never repeat */
	TIM4_Handler.Init.RepetitionCounter=0;
	/* Low-level init and enable interrupts */
	HAL_TIM_Base_Init(&TIM4_Handler);
	/* Clear interrupt pending bit, because we used EGR to update the registers */
	__HAL_TIM_CLEAR_IT(&TIM4_Handler, TIM_IT_UPDATE);
	HAL_TIM_Base_Start_IT(&TIM4_Handler);
#endif
}


/* TIM4 is used to simulate the rdtsc. do not touch */
extern volatile unsigned long long rdtsc_val;
extern volatile unsigned long long* rdtsc_sim;
void
timer_init(void)
{
	rdtsc_val=0;
	rdtsc_sim=&rdtsc_val;
	TIM4_Init(0xFFFF);
	/* Do not use this - we use tim4 instead, so we can read the value safely */

//	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
//	SysTick->CTRL|=SysTick_CTRL_TICKINT_Msk;
//	SysTick->LOAD=0xFFFFFF;/*1000;*/
//	SysTick->VAL=0x00;
//	NVIC_SetPriority(SysTick_IRQn,0x00);
//	SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk;
}

//void SysTick_Handler(void)
//{
//
//}

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
	//HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);
}

void
LED_reset(void)
{
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1,GPIO_PIN_SET);
	//HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_RESET);
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
	MPU_Initure.IsCacheable=MPU_ACCESS_CACHEABLE;              //Cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	/* The shared read-only memory section - directly exposed */
	MPU_Initure.Enable=MPU_REGION_ENABLE;			   //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER5;			   //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)0x2002C000;	           //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_16KB;			   //Set the protection area size
	MPU_Initure.SubRegionDisable=0X00;                         //We disable the subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_PRIV_RW_URO; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_NOT_CACHEABLE;          //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_NOT_BUFFERABLE;            //Bufferable
	HAL_MPU_ConfigRegion(&MPU_Initure);                        //Initialize the MPU

	/* The STM32 peripheral/SDRAM section - directly expose to peripherals */
	MPU_Initure.Enable=MPU_REGION_ENABLE;			   //Enable this area
	MPU_Initure.Number=MPU_REGION_NUMBER6;			   //Set the protection area
	MPU_Initure.BaseAddress=(u32_t)0x00000000;	           //Set the base address
	MPU_Initure.Size=MPU_REGION_SIZE_4GB;			   //Set the protection area size
	MPU_Initure.SubRegionDisable=0X03;                         //We disable the first 2 subregions now
	MPU_Initure.TypeExtField=MPU_TEX_LEVEL0;                   //Type expansion area:level0
	MPU_Initure.AccessPermission=(u8_t)MPU_REGION_FULL_ACCESS; //Set the access permissions
	MPU_Initure.DisableExec=MPU_INSTRUCTION_ACCESS_ENABLE;	   //Executable
	MPU_Initure.IsShareable=MPU_ACCESS_NOT_SHAREABLE;          //Not sharable
	MPU_Initure.IsCacheable=MPU_ACCESS_NOT_CACHEABLE;          //Not cacheable
	MPU_Initure.IsBufferable=MPU_ACCESS_NOT_BUFFERABLE;        //Not bufferable
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
	/* initialize the serial port */
	serial_init(115200);

	timer_init();

	SDRAM_Init();
	LCD_Init();
	GPIO_Init();

	POINT_COLOR=RED;
	LCD_Clear(GREEN);
	MPU_Init();

}
