#ifndef _SYS_H
#define _SYS_H
#include "stm32f7xx.h"
#include "core_cm7.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_conf.h"

#define SYSTEM_SUPPORT_OS		0

typedef int32_t  s32;
typedef int16_t s16;
typedef int8_t  s8;

typedef const int32_t sc32;  
typedef const int16_t sc16;  
typedef const int8_t sc8;  

typedef __IO int32_t  vs32;
typedef __IO int16_t  vs16;
typedef __IO int8_t   vs8;

typedef __I int32_t vsc32;  
typedef __I int16_t vsc16; 
typedef __I int8_t vsc8;   

typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;

typedef const uint32_t uc32;  
typedef const uint16_t uc16;  
typedef const uint8_t uc8; 

typedef __IO uint32_t  vu32;
typedef __IO uint16_t vu16;
typedef __IO uint8_t  vu8;

typedef __I uint32_t vuc32;  
typedef __I uint16_t vuc16; 
typedef __I uint8_t vuc8;  

#define ON	1
#define OFF	0
#define Write_Through() (*(__IO uint32_t*)0XE000EF9C=1UL<<2)

void Cache_Enable(void);
void Stm32_Clock_Init(u32 plln,u32 pllm,u32 pllp,u32 pllq);
u8 Get_ICahceSta(void);
u8 Get_DCahceSta(void);

void WFI_SET(void);
void INTX_DISABLE(void);
void INTX_ENABLE(void);
void MSR_MSP(u32 addr);
#endif

