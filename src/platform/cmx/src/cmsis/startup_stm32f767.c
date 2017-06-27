/**
  ******************************************************************************
  * @file    startup_stm32f767.c
  * @author  pry,MCD Application Team
  * @version V1.0.0
  * @date    22-April-2016
  * @brief   CMSIS Cortex-M7 Device STM32F767 Startup File, derived from ST code.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

#include <stdint.h>
#include <sys/types.h>
#include "cmsis_device.h"
#include "stm32f7xx_hal_conf.h"

extern unsigned int __vectors_start;
extern unsigned int _estack;
extern unsigned int _sidata;
extern unsigned int _sdata;
extern unsigned int _edata;
extern unsigned int __bss_start__;
extern unsigned int __bss_end__;

extern unsigned int _c1_sidata;
extern unsigned int _c1_sdata;
extern unsigned int _c1_edata;
extern unsigned int __c1_bss_start__;
extern unsigned int __c1_bss_end__;

/* newlib; main function defined as int main(void) */
extern void __initialize_args (void);
extern int main (void);

/* Initialize the clock for STM32. code derived from Alientek */
void Stm32_Clock_Init(unsigned int plln,unsigned int pllm,unsigned int pllp,unsigned int pllq)
{
    HAL_StatusTypeDef ret = HAL_OK;
    RCC_OscInitTypeDef RCC_OscInitStructure;
    RCC_ClkInitTypeDef RCC_ClkInitStructure;

    __HAL_RCC_PWR_CLK_ENABLE(); //Ê¹ÄÜPWRÊ±ÖÓ

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);//ÉèÖÃµ÷Ñ¹Æ÷Êä³öµçÑ¹¼¶±ð£¬ÒÔ±ãÔÚÆ÷¼þÎ´ÒÔ×î´óÆµÂÊ¹¤×÷

    RCC_OscInitStructure.OscillatorType=RCC_OSCILLATORTYPE_HSE;    //Ê±ÖÓÔ´ÎªHSE
    RCC_OscInitStructure.HSEState=RCC_HSE_ON;                      //´ò¿ªHSE
    RCC_OscInitStructure.PLL.PLLState=RCC_PLL_ON;				   //´ò¿ªPLL
    RCC_OscInitStructure.PLL.PLLSource=RCC_PLLSOURCE_HSE;          //PLLÊ±ÖÓÔ´Ñ¡ÔñHSE
    RCC_OscInitStructure.PLL.PLLM=pllm;	//Ö÷PLLºÍÒôÆµPLL·ÖÆµÏµÊý(PLLÖ®Ç°µÄ·ÖÆµ)
    RCC_OscInitStructure.PLL.PLLN=plln; //Ö÷PLL±¶ÆµÏµÊý(PLL±¶Æµ)
    RCC_OscInitStructure.PLL.PLLP=pllp; //ÏµÍ³Ê±ÖÓµÄÖ÷PLL·ÖÆµÏµÊý(PLLÖ®ºóµÄ·ÖÆµ)
    RCC_OscInitStructure.PLL.PLLQ=pllq; //USB/SDIO/Ëæ»úÊý²úÉúÆ÷µÈµÄÖ÷PLL·ÖÆµÏµÊý(PLLÖ®ºóµÄ·ÖÆµ)
    ret=HAL_RCC_OscConfig(&RCC_OscInitStructure);//³õÊ¼»¯
    if(ret!=HAL_OK) while(1);

    ret=HAL_PWREx_EnableOverDrive(); //¿ªÆôOver-Driver¹¦ÄÜ
    if(ret!=HAL_OK) while(1);

    //Ñ¡ÖÐPLL×÷ÎªÏµÍ³Ê±ÖÓÔ´²¢ÇÒÅäÖÃHCLK,PCLK1ºÍPCLK2
    RCC_ClkInitStructure.ClockType=(RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStructure.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK;//ÉèÖÃÏµÍ³Ê±ÖÓÊ±ÖÓÔ´ÎªPLL
    RCC_ClkInitStructure.AHBCLKDivider=RCC_SYSCLK_DIV1;//AHB·ÖÆµÏµÊýÎª1
    RCC_ClkInitStructure.APB1CLKDivider=RCC_HCLK_DIV4;//APB1·ÖÆµÏµÊýÎª4
    RCC_ClkInitStructure.APB2CLKDivider=RCC_HCLK_DIV2;//APB2·ÖÆµÏµÊýÎª2

    ret=HAL_RCC_ClockConfig(&RCC_ClkInitStructure,FLASH_LATENCY_7);//Í¬Ê±ÉèÖÃFLASHÑÓÊ±ÖÜÆÚÎª7WS£¬Ò²¾ÍÊÇ8¸öCPUÖÜÆÚ¡£
    if(ret!=HAL_OK) while(1);
}

void __attribute__ ((section(".after_vectors"),noreturn))
_start (void)
{
	unsigned int* src;
	unsigned int* ptr;

	/* system init after rst */
	SystemInit();
	/* set vector to linker script positions */
	SCB->VTOR = (uint32_t)(&__vectors_start);

	/* no C++ inits done by this func so cannot use cpp */
	/* data section initialization */
	for(ptr=&_sdata,src=&_sidata;ptr<&_edata;ptr++,src++)
		*ptr=*src;
	/* bss section initialization */
	for(ptr=&__bss_start__;ptr<&__bss_end__;ptr++)
		*ptr=0;

	/* This is for component 1 */
	/* data section initialization */
	for(ptr=&_c1_sdata,src=&_c1_sidata;ptr<&_c1_edata;ptr++,src++)
		*ptr=*src;
	/* bss section initialization */
	for(ptr=&__c1_bss_start__;ptr<&__c1_bss_end__;ptr++)
		*ptr=0;

	/* fpu enabled? */
	/* SCB->CPACR |= (0xF << 20); */
	Stm32_Clock_Init(432,25,2,9);
	SystemCoreClockUpdate();

	/* Enable cache */
	SCB_EnableICache();
	SCB_EnableDCache();
	/* Force write through or we may encounter errors */
	SCB->CACR|=1<<2;

	/* Enable all fault handlers */
	SCB->SHCSR|=0x00030000;/*enable Usage Fault, Bus Fault, and MMU Fault*/

	/* call into main and never return */
  	main();
  	/* reset the machine if main quit */
  	NVIC_SystemReset();
  	/* never return, should not get here */
  	while(1);
}

void
_exit(int code)
{
	/* always a system reset */
	NVIC_SystemReset();
}

int
_kill(int pid, int sig)
{
	return -1;
}

int
_getpid(void)
{
	return -1;
}

char*
_sbrk(int incr)
{
	/* defined by the linker */
	extern char _Heap_Limit;
	return (char*)(&_Heap_Limit);
}

/* interrupt handlers */
void __attribute__ ((section(".after_vectors"),noreturn))
Reset_Handler (void)
{
  _start ();
}

void __attribute__ ((section(".after_vectors"),weak))
NMI_Handler (void)
{
	while(1);
}

extern char str[100];
void __attribute__ ((section(".after_vectors"),weak))
HardFault_Handler (void)
{
	LED_reset();
	sprintf(str,"CCR value : 0x%8x", SCB->CCR);
	LCD_ShowString(20,20,260,32,12,str);
	sprintf(str,"HFSR value : 0x%8x", SCB->HFSR);
	LCD_ShowString(20,40,260,32,12,str);
	sprintf(str,"CFSR value : 0x%8x", SCB->CFSR);
	LCD_ShowString(20,60,260,32,12,str);
	while(1);
}

void __attribute__ ((section(".after_vectors"),weak))
MemManage_Handler (void)
{
	while(1);
}

void __attribute__ ((section(".after_vectors"),weak))
BusFault_Handler (void)
{
	while(1);
}

void __attribute__ ((section(".after_vectors"),weak))
UsageFault_Handler (void)
{
	while(1);
}

void __attribute__ ((section(".after_vectors"),naked))
SVC_Handler (void)
{
	/* Here, the stack of the Cortex-M3 should be xplained. this will require modification of this
	 * code to make the thd switch actually work.
	 * dditionally, this will require modifications on the parameter passing to make sure it works.
	 * regs are saved on the user stack, so r0-r3 still cannot be used as retvals. modification of the r15_pc is impossible too. */
        __asm__ __volatile__(//"push {lr} \n"
                             "push {r0-r12} \n"

    			     "mrs r0,psp \n"
    			     "push {r0} \n"
			     "push {lr} \n"
    			     "mov r0,pc \n"
			     "push {r0} \n"
			     "mrs r0,xpsr \n"
			     "push {r0} \n"
			     "mov  r0,sp \n"

			     "bl composite_syscall_handler \n"

			     "pop {r0} \n"   /* xpsr */
    		             /* "msr xpsr,r0 \n" */
			     "pop {r0} \n"   /* pc */
    			     "pop {r0} \n"   /* lr */
    			     "pop {r0} \n"   /* psp */
			     "msr psp,r0 \n"

			     "pop {r0-r12} \n"
			     //"pop {lr} \n"
        		 "ldr lr,=0xFFFFFFFD \n"
			     "bx lr \n"
			     :::);
}

void __attribute__ ((section(".after_vectors"),weak))
DebugMon_Handler (void)
{
	while(1);
}

void __attribute__ ((section(".after_vectors"),weak))
PendSV_Handler (void)
{
	while(1);
}

void __attribute__ ((section(".after_vectors"),weak))
SysTick_Handler (void)
{
	return;
}

void __attribute__ ((section(".after_vectors"),weak))
Default_Handler(void)
{
	while(1);
}

#define EINT_HANDLER __attribute__ ((weak, alias ("Default_Handler")))
void EINT_HANDLER WWDG_IRQHandler(void);                   /* Window WatchDog */
void EINT_HANDLER PVD_IRQHandler(void);                    /* PVD through EXTI Line detection */
void EINT_HANDLER TAMP_STAMP_IRQHandler(void);             /* Tamper and TimeStamps through the EXTI line */
void EINT_HANDLER RTC_WKUP_IRQHandler(void);               /* RTC Wakeup through the EXTI line */
void EINT_HANDLER FLASH_IRQHandler(void);                  /* FLASH */
void EINT_HANDLER RCC_IRQHandler(void);                    /* RCC */
void EINT_HANDLER EXTI0_IRQHandler(void);                  /* EXTI Line0 */
void EINT_HANDLER EXTI1_IRQHandler(void);                  /* EXTI Line1 */
void EINT_HANDLER EXTI2_IRQHandler(void);                  /* EXTI Line2 */
void EINT_HANDLER EXTI3_IRQHandler(void);                  /* EXTI Line3 */
void EINT_HANDLER EXTI4_IRQHandler(void);                  /* EXTI Line4 */
void EINT_HANDLER DMA1_Stream0_IRQHandler(void);           /* DMA1 Stream 0 */
void EINT_HANDLER DMA1_Stream1_IRQHandler(void);           /* DMA1 Stream 1 */
void EINT_HANDLER DMA1_Stream2_IRQHandler(void);           /* DMA1 Stream 2 */
void EINT_HANDLER DMA1_Stream3_IRQHandler(void);           /* DMA1 Stream 3 */
void EINT_HANDLER DMA1_Stream4_IRQHandler(void);           /* DMA1 Stream 4 */
void EINT_HANDLER DMA1_Stream5_IRQHandler(void);           /* DMA1 Stream 5 */
void EINT_HANDLER DMA1_Stream6_IRQHandler(void);           /* DMA1 Stream 6 */
void EINT_HANDLER ADC_IRQHandler(void);                    /* ADC1(void); ADC2 and ADC3s */
void EINT_HANDLER CAN1_TX_IRQHandler(void);                /* CAN1 TX */
void EINT_HANDLER CAN1_RX0_IRQHandler(void);               /* CAN1 RX0 */
void EINT_HANDLER CAN1_RX1_IRQHandler(void);               /* CAN1 RX1 */
void EINT_HANDLER CAN1_SCE_IRQHandler(void);               /* CAN1 SCE */
void EINT_HANDLER EXTI9_5_IRQHandler(void);                /* External Line[9:5]s */
void EINT_HANDLER TIM1_BRK_TIM9_IRQHandler(void);          /* TIM1 Break and TIM9 */
void EINT_HANDLER TIM1_UP_TIM10_IRQHandler(void);          /* TIM1 Update and TIM10 */
void EINT_HANDLER TIM1_TRG_COM_TIM11_IRQHandler(void);     /* TIM1 Trigger and Commutation and TIM11 */
void EINT_HANDLER TIM1_CC_IRQHandler(void);                /* TIM1 Capture Compare */
void EINT_HANDLER TIM2_IRQHandler(void);                   /* TIM2 */
//void EINT_HANDLER TIM3_IRQHandler(void);                   /* TIM3 */
void EINT_HANDLER TIM4_IRQHandler(void);                   /* TIM4 */
void EINT_HANDLER I2C1_EV_IRQHandler(void);                /* I2C1 Event */
void EINT_HANDLER I2C1_ER_IRQHandler(void);                /* I2C1 Error */
void EINT_HANDLER I2C2_EV_IRQHandler(void);                /* I2C2 Event */
void EINT_HANDLER I2C2_ER_IRQHandler(void);                /* I2C2 Error */
void EINT_HANDLER SPI1_IRQHandler(void);                   /* SPI1 */
void EINT_HANDLER SPI2_IRQHandler(void);                   /* SPI2 */
void EINT_HANDLER USART1_IRQHandler(void);                 /* USART1 */
void EINT_HANDLER USART2_IRQHandler(void);                 /* USART2 */
void EINT_HANDLER USART3_IRQHandler(void);                 /* USART3 */
void EINT_HANDLER EXTI15_10_IRQHandler(void);              /* External Line[15:10]s */
void EINT_HANDLER RTC_Alarm_IRQHandler(void);              /* RTC Alarm (A and B) through EXTI Line */
void EINT_HANDLER OTG_FS_WKUP_IRQHandler(void);            /* USB OTG FS Wakeup through EXTI line */
void EINT_HANDLER TIM8_BRK_TIM12_IRQHandler(void);         /* TIM8 Break and TIM12 */
void EINT_HANDLER TIM8_UP_TIM13_IRQHandler(void);          /* TIM8 Update and TIM13 */
void EINT_HANDLER TIM8_TRG_COM_TIM14_IRQHandler(void);     /* TIM8 Trigger and Commutation and TIM14 */
void EINT_HANDLER TIM8_CC_IRQHandler(void);                /* TIM8 Capture Compare */
void EINT_HANDLER DMA1_Stream7_IRQHandler(void);           /* DMA1 Stream7 */
void EINT_HANDLER FMC_IRQHandler(void);                    /* FMC */
void EINT_HANDLER SDMMC1_IRQHandler(void);                 /* SDMMC1 */
void EINT_HANDLER TIM5_IRQHandler(void);                   /* TIM5 */
void EINT_HANDLER SPI3_IRQHandler(void);                   /* SPI3 */
void EINT_HANDLER UART4_IRQHandler(void);                  /* UART4 */
void EINT_HANDLER UART5_IRQHandler(void);                  /* UART5 */
void EINT_HANDLER TIM6_DAC_IRQHandler(void);               /* TIM6 and DAC1&2 underrun errors */
void EINT_HANDLER TIM7_IRQHandler(void);                   /* TIM7 */
void EINT_HANDLER DMA2_Stream0_IRQHandler(void);           /* DMA2 Stream 0 */
void EINT_HANDLER DMA2_Stream1_IRQHandler(void);           /* DMA2 Stream 1 */
void EINT_HANDLER DMA2_Stream2_IRQHandler(void);           /* DMA2 Stream 2 */
void EINT_HANDLER DMA2_Stream3_IRQHandler(void);           /* DMA2 Stream 3 */
void EINT_HANDLER DMA2_Stream4_IRQHandler(void);           /* DMA2 Stream 4 */
void EINT_HANDLER ETH_IRQHandler(void);                    /* Ethernet */
void EINT_HANDLER ETH_WKUP_IRQHandler(void);               /* Ethernet Wakeup through EXTI line */
void EINT_HANDLER CAN2_TX_IRQHandler(void);                /* CAN2 TX */
void EINT_HANDLER CAN2_RX0_IRQHandler(void);               /* CAN2 RX0 */
void EINT_HANDLER CAN2_RX1_IRQHandler(void);               /* CAN2 RX1 */
void EINT_HANDLER CAN2_SCE_IRQHandler(void);               /* CAN2 SCE */
void EINT_HANDLER OTG_FS_IRQHandler(void);                 /* USB OTG FS */
void EINT_HANDLER DMA2_Stream5_IRQHandler(void);           /* DMA2 Stream 5 */
void EINT_HANDLER DMA2_Stream6_IRQHandler(void);           /* DMA2 Stream 6 */
void EINT_HANDLER DMA2_Stream7_IRQHandler(void);           /* DMA2 Stream 7 */
void EINT_HANDLER USART6_IRQHandler(void);                 /* USART6 */
void EINT_HANDLER I2C3_EV_IRQHandler(void);                /* I2C3 event */
void EINT_HANDLER I2C3_ER_IRQHandler(void);                /* I2C3 error */
void EINT_HANDLER OTG_HS_EP1_OUT_IRQHandler(void);         /* USB OTG HS End Point 1 Out */
void EINT_HANDLER OTG_HS_EP1_IN_IRQHandler(void);          /* USB OTG HS End Point 1 In */
void EINT_HANDLER OTG_HS_WKUP_IRQHandler(void);            /* USB OTG HS Wakeup through EXTI */
void EINT_HANDLER OTG_HS_IRQHandler(void);                 /* USB OTG HS */
void EINT_HANDLER DCMI_IRQHandler(void);                   /* DCMI */
void EINT_HANDLER RNG_IRQHandler(void);                    /* Rng */
void EINT_HANDLER FPU_IRQHandler(void);                    /* FPU */
void EINT_HANDLER UART7_IRQHandler(void);                  /* UART7 */
void EINT_HANDLER UART8_IRQHandler(void);                  /* UART8 */
void EINT_HANDLER SPI4_IRQHandler(void);                   /* SPI4 */
void EINT_HANDLER SPI5_IRQHandler(void);                   /* SPI5 */
void EINT_HANDLER SPI6_IRQHandler(void);                   /* SPI6 */
void EINT_HANDLER SAI1_IRQHandler(void);                   /* SAI1 */
void EINT_HANDLER LTDC_IRQHandler(void);                   /* LTDC */
void EINT_HANDLER LTDC_ER_IRQHandler(void);                /* LTDC error */
void EINT_HANDLER DMA2D_IRQHandler(void);                  /* DMA2D */
void EINT_HANDLER SAI2_IRQHandler(void);                   /* SAI2 */
void EINT_HANDLER QUADSPI_IRQHandler(void);                /* QUADSPI */
void EINT_HANDLER LPTIM1_IRQHandler(void);                 /* LPTIM1 */
void EINT_HANDLER CEC_IRQHandler(void);                    /* HDMI_CEC */
void EINT_HANDLER I2C4_EV_IRQHandler(void);                /* I2C4 Event */
void EINT_HANDLER I2C4_ER_IRQHandler(void);                /* I2C4 Error */
void EINT_HANDLER SPDIF_RX_IRQHandler(void);               /* SPDIF_RX */
void EINT_HANDLER DFSDM1_FLT0_IRQHandler(void);            /* DFSDM1 Filter 0 global Interrupt */
void EINT_HANDLER DFSDM1_FLT1_IRQHandler(void);            /* DFSDM1 Filter 1 global Interrupt */
void EINT_HANDLER DFSDM1_FLT2_IRQHandler(void);            /* DFSDM1 Filter 2 global Interrupt */
void EINT_HANDLER DFSDM1_FLT3_IRQHandler(void);            /* DFSDM1 Filter 3 global Interrupt */
void EINT_HANDLER SDMMC2_IRQHandler(void);                 /* SDMMC2 */
void EINT_HANDLER CAN3_TX_IRQHandler(void);                /* CAN3 TX */
void EINT_HANDLER CAN3_RX0_IRQHandler(void);               /* CAN3 RX0 */
void EINT_HANDLER CAN3_RX1_IRQHandler(void);               /* CAN3 RX1 */
void EINT_HANDLER CAN3_SCE_IRQHandler(void);               /* CAN3 SCE */
void EINT_HANDLER JPEG_IRQHandler(void);                   /* JPEG */
void EINT_HANDLER MDIOS_IRQHandler(void);                  /* MDIOS */

/* This is the real tim3 handler */
void __attribute__ ((section(".after_vectors"),naked))
TIM3_IRQHandler(void)
{
	/* The assembly is similar with the svc call */
	__asm__ __volatile__(//"push {lr} \n"
	                             "push {r0-r12} \n"

	    			     "mrs r0,psp \n"
	    			     "push {r0} \n"
				     "push {lr} \n"
	    			     "mov r0,pc \n"
				     "push {r0} \n"
				     "mrs r0,xpsr \n"
				     "push {r0} \n"
				     "mov  r0,sp \n"

				     "bl tim3irqhandler \n"

				     "pop {r0} \n"   /* xpsr */
	    		             /* "msr xpsr,r0 \n" */
				     "pop {r0} \n"   /* pc */
	    			     "pop {r0} \n"   /* lr */
	    			     "pop {r0} \n"   /* psp */
				     "msr psp,r0 \n"

				     "pop {r0-r12} \n"
				     //"pop {lr} \n"
			 	 	 "ldr lr,=0xFFFFFFFD \n"
				     "bx lr \n"
				     :::);
}

/* stm32f767 vector table */
__attribute__ ((section(".isr_vector"),used))
void (*const __isr_vectors[])(void)=
{
    /* Cortex-M Core Handlers */
    (void (*const)(void)) &_estack,    /* Top of Stack */
	Reset_Handler,                     /* Reset Handler */
	NMI_Handler,                       /* NMI Handler */
	HardFault_Handler,                 /* Hard Fault Handler */
	MemManage_Handler,                 /* MPU Fault Handler */
	BusFault_Handler,                  /* Bus Fault Handler */
	UsageFault_Handler,                /* Usage Fault Handler */
	0,                                 /* Reserved */
	0,                                 /* Reserved */
	0,                                 /* Reserved */
	0,                                 /* Reserved */
	SVC_Handler,                       /* SVCall Handler */
	DebugMon_Handler,                  /* Debug Monitor Handler */
	0,                                 /* Reserved */
	PendSV_Handler,                    /* PendSV Handler */
	SysTick_Handler,                   /* SysTick Handler */

	/* External interrupts */
	WWDG_IRQHandler,                   /* Window WatchDog */
	PVD_IRQHandler,                    /* PVD through EXTI Line detection */
	TAMP_STAMP_IRQHandler,             /* Tamper and TimeStamps through the EXTI line */
	RTC_WKUP_IRQHandler,               /* RTC Wakeup through the EXTI line */
	FLASH_IRQHandler,                  /* FLASH */
	RCC_IRQHandler,                    /* RCC */
	EXTI0_IRQHandler,                  /* EXTI Line0 */
	EXTI1_IRQHandler,                  /* EXTI Line1 */
	EXTI2_IRQHandler,                  /* EXTI Line2 */
	EXTI3_IRQHandler,                  /* EXTI Line3 */
	EXTI4_IRQHandler,                  /* EXTI Line4 */
	DMA1_Stream0_IRQHandler,           /* DMA1 Stream 0 */
	DMA1_Stream1_IRQHandler,           /* DMA1 Stream 1 */
	DMA1_Stream2_IRQHandler,           /* DMA1 Stream 2 */
	DMA1_Stream3_IRQHandler,           /* DMA1 Stream 3 */
	DMA1_Stream4_IRQHandler,           /* DMA1 Stream 4 */
	DMA1_Stream5_IRQHandler,           /* DMA1 Stream 5 */
	DMA1_Stream6_IRQHandler,           /* DMA1 Stream 6 */
	ADC_IRQHandler,                    /* ADC1, ADC2 and ADC3s */
	CAN1_TX_IRQHandler,                /* CAN1 TX */
	CAN1_RX0_IRQHandler,               /* CAN1 RX0 */
	CAN1_RX1_IRQHandler,               /* CAN1 RX1 */
	CAN1_SCE_IRQHandler,               /* CAN1 SCE */
	EXTI9_5_IRQHandler,                /* External Line[9:5]s */
	TIM1_BRK_TIM9_IRQHandler,          /* TIM1 Break and TIM9 */
	TIM1_UP_TIM10_IRQHandler,          /* TIM1 Update and TIM10 */
	TIM1_TRG_COM_TIM11_IRQHandler,     /* TIM1 Trigger and Commutation and TIM11 */
	TIM1_CC_IRQHandler,                /* TIM1 Capture Compare */
	TIM2_IRQHandler,                   /* TIM2 */
	TIM3_IRQHandler,                   /* TIM3 */
	TIM4_IRQHandler,                   /* TIM4 */
	I2C1_EV_IRQHandler,                /* I2C1 Event */
	I2C1_ER_IRQHandler,                /* I2C1 Error */
	I2C2_EV_IRQHandler,                /* I2C2 Event */
	I2C2_ER_IRQHandler,                /* I2C2 Error */
	SPI1_IRQHandler,                   /* SPI1 */
	SPI2_IRQHandler,                   /* SPI2 */
	USART1_IRQHandler,                 /* USART1 */
	USART2_IRQHandler,                 /* USART2 */
	USART3_IRQHandler,                 /* USART3 */
	EXTI15_10_IRQHandler,              /* External Line[15:10]s */
	RTC_Alarm_IRQHandler,              /* RTC Alarm (A and B) through EXTI Line */
	OTG_FS_WKUP_IRQHandler,            /* USB OTG FS Wakeup through EXTI line */
	TIM8_BRK_TIM12_IRQHandler,         /* TIM8 Break and TIM12 */
	TIM8_UP_TIM13_IRQHandler,          /* TIM8 Update and TIM13 */
	TIM8_TRG_COM_TIM14_IRQHandler,     /* TIM8 Trigger and Commutation and TIM14 */
	TIM8_CC_IRQHandler,                /* TIM8 Capture Compare */
	DMA1_Stream7_IRQHandler,           /* DMA1 Stream7 */
	FMC_IRQHandler,                    /* FMC */
	SDMMC1_IRQHandler,                 /* SDMMC1 */
	TIM5_IRQHandler,                   /* TIM5 */
	SPI3_IRQHandler,                   /* SPI3 */
	UART4_IRQHandler,                  /* UART4 */
	UART5_IRQHandler,                  /* UART5 */
	TIM6_DAC_IRQHandler,               /* TIM6 and DAC1&2 underrun errors */
	TIM7_IRQHandler,                   /* TIM7 */
	DMA2_Stream0_IRQHandler,           /* DMA2 Stream 0 */
	DMA2_Stream1_IRQHandler,           /* DMA2 Stream 1 */
	DMA2_Stream2_IRQHandler,           /* DMA2 Stream 2 */
	DMA2_Stream3_IRQHandler,           /* DMA2 Stream 3 */
	DMA2_Stream4_IRQHandler,           /* DMA2 Stream 4 */
	ETH_IRQHandler,                    /* Ethernet */
	ETH_WKUP_IRQHandler,               /* Ethernet Wakeup through EXTI line */
	CAN2_TX_IRQHandler,                /* CAN2 TX */
	CAN2_RX0_IRQHandler,               /* CAN2 RX0 */
	CAN2_RX1_IRQHandler,               /* CAN2 RX1 */
	CAN2_SCE_IRQHandler,               /* CAN2 SCE */
	OTG_FS_IRQHandler,                 /* USB OTG FS */
	DMA2_Stream5_IRQHandler,           /* DMA2 Stream 5 */
	DMA2_Stream6_IRQHandler,           /* DMA2 Stream 6 */
	DMA2_Stream7_IRQHandler,           /* DMA2 Stream 7 */
	USART6_IRQHandler,                 /* USART6 */
	I2C3_EV_IRQHandler,                /* I2C3 event */
	I2C3_ER_IRQHandler,                /* I2C3 error */
	OTG_HS_EP1_OUT_IRQHandler,         /* USB OTG HS End Point 1 Out */
	OTG_HS_EP1_IN_IRQHandler,          /* USB OTG HS End Point 1 In */
	OTG_HS_WKUP_IRQHandler,            /* USB OTG HS Wakeup through EXTI */
	OTG_HS_IRQHandler,                 /* USB OTG HS */
	DCMI_IRQHandler,                   /* DCMI */
	0,                                 /* Reserved */
	RNG_IRQHandler,                    /* Rng */
	FPU_IRQHandler,                    /* FPU */
	UART7_IRQHandler,                  /* UART7 */
	UART8_IRQHandler,                  /* UART8 */
	SPI4_IRQHandler,                   /* SPI4 */
	SPI5_IRQHandler,                   /* SPI5 */
	SPI6_IRQHandler,                   /* SPI6 */
	SAI1_IRQHandler,                   /* SAI1 */
	LTDC_IRQHandler,                   /* LTDC */
	LTDC_ER_IRQHandler,                /* LTDC error */
	DMA2D_IRQHandler,                  /* DMA2D */
	SAI2_IRQHandler,                   /* SAI2 */
	QUADSPI_IRQHandler,                /* QUADSPI */
	LPTIM1_IRQHandler,                 /* LPTIM1 */
	CEC_IRQHandler,                    /* HDMI_CEC */
	I2C4_EV_IRQHandler,                /* I2C4 Event */
	I2C4_ER_IRQHandler,                /* I2C4 Error */
	SPDIF_RX_IRQHandler,               /* SPDIF_RX */
	0,                                 /* Reserved */
	DFSDM1_FLT0_IRQHandler,            /* DFSDM1 Filter 0 global Interrupt */
	DFSDM1_FLT1_IRQHandler,            /* DFSDM1 Filter 1 global Interrupt */
	DFSDM1_FLT2_IRQHandler,            /* DFSDM1 Filter 2 global Interrupt */
	DFSDM1_FLT3_IRQHandler,            /* DFSDM1 Filter 3 global Interrupt */
	SDMMC2_IRQHandler,                 /* SDMMC2 */
	CAN3_TX_IRQHandler,                /* CAN3 TX */
	CAN3_RX0_IRQHandler,               /* CAN3 RX0 */
	CAN3_RX1_IRQHandler,               /* CAN3 RX1 */
	CAN3_SCE_IRQHandler,               /* CAN3 SCE */
	JPEG_IRQHandler,                   /* JPEG */
	MDIOS_IRQHandler                   /* MDIOS */
};
