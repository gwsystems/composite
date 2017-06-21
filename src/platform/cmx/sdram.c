#include "sdram.h"

SDRAM_HandleTypeDef SDRAM_Handler;
void SDRAM_Init(void)
{
    FMC_SDRAM_TimingTypeDef SDRAM_Timing;
                                                     
    SDRAM_Handler.Instance=FMC_SDRAM_DEVICE;
    SDRAM_Handler.Init.SDBank=FMC_SDRAM_BANK1;
    SDRAM_Handler.Init.ColumnBitsNumber=FMC_SDRAM_COLUMN_BITS_NUM_9;
    SDRAM_Handler.Init.RowBitsNumber=FMC_SDRAM_ROW_BITS_NUM_13;
    SDRAM_Handler.Init.MemoryDataWidth=FMC_SDRAM_MEM_BUS_WIDTH_16;
    SDRAM_Handler.Init.InternalBankNumber=FMC_SDRAM_INTERN_BANKS_NUM_4;
    SDRAM_Handler.Init.CASLatency=FMC_SDRAM_CAS_LATENCY_3;
    SDRAM_Handler.Init.WriteProtection=FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    SDRAM_Handler.Init.SDClockPeriod=FMC_SDRAM_CLOCK_PERIOD_2;
    SDRAM_Handler.Init.ReadBurst=FMC_SDRAM_RBURST_ENABLE;
    SDRAM_Handler.Init.ReadPipeDelay=FMC_SDRAM_RPIPE_DELAY_1;
    
    SDRAM_Timing.LoadToActiveDelay=2;
    SDRAM_Timing.ExitSelfRefreshDelay=8;
    SDRAM_Timing.SelfRefreshTime=6;
    SDRAM_Timing.RowCycleDelay=6;
    SDRAM_Timing.WriteRecoveryTime=2;
    SDRAM_Timing.RPDelay=2;
    SDRAM_Timing.RCDDelay=2;
    HAL_SDRAM_Init(&SDRAM_Handler,&SDRAM_Timing);
	SDRAM_Initialization_Sequence(&SDRAM_Handler);
}

void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram)
{
	u32 temp=0;
    SDRAM_Send_Cmd(0,FMC_SDRAM_CMD_CLK_ENABLE,1,0);
    delay_us(500);
    SDRAM_Send_Cmd(0,FMC_SDRAM_CMD_PALL,1,0);
    SDRAM_Send_Cmd(0,FMC_SDRAM_CMD_AUTOREFRESH_MODE,8,0);

	temp=(u32)SDRAM_MODEREG_BURST_LENGTH_1          |
              SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
              SDRAM_MODEREG_CAS_LATENCY_3           |
              SDRAM_MODEREG_OPERATING_MODE_STANDARD |
              SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
    SDRAM_Send_Cmd(0,FMC_SDRAM_CMD_LOAD_MODE,1,temp);
	HAL_SDRAM_ProgramRefreshRate(&SDRAM_Handler,823);	

}

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram)
{
    GPIO_InitTypeDef GPIO_Initure;
    
    __HAL_RCC_FMC_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    
    GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3;  
    GPIO_Initure.Mode=GPIO_MODE_AF_PP;
    GPIO_Initure.Pull=GPIO_PULLUP;
    GPIO_Initure.Speed=GPIO_SPEED_HIGH;
    GPIO_Initure.Alternate=GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOC,&GPIO_Initure);
    
    
    GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_14|GPIO_PIN_15;              
    HAL_GPIO_Init(GPIOD,&GPIO_Initure);
    
    GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;              
    HAL_GPIO_Init(GPIOE,&GPIO_Initure);
    
    GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;              
    HAL_GPIO_Init(GPIOF,&GPIO_Initure);
    
    GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_8|GPIO_PIN_15;              
    HAL_GPIO_Init(GPIOG,&GPIO_Initure);
}

u8 SDRAM_Send_Cmd(u8 bankx,u8 cmd,u8 refresh,u16 regval)
{
    u32 target_bank=0;
    FMC_SDRAM_CommandTypeDef Command;
    
    if(bankx==0) target_bank=FMC_SDRAM_CMD_TARGET_BANK1;       
    else if(bankx==1) target_bank=FMC_SDRAM_CMD_TARGET_BANK2;   
    Command.CommandMode=cmd;
    Command.CommandTarget=target_bank;
    Command.AutoRefreshNumber=refresh;
    Command.ModeRegisterDefinition=regval;
    if(HAL_SDRAM_SendCommand(&SDRAM_Handler,&Command,0X1000)==HAL_OK)
        return 0;
    else 
        return 1;    
}

void FMC_SDRAM_WriteBuffer(u8 *pBuffer,u32 WriteAddr,u32 n)
{
	for(;n!=0;n--)
	{
		*(vu8*)(Bank5_SDRAM_ADDR+WriteAddr)=*pBuffer;
		WriteAddr++;
		pBuffer++;
	}
}

void FMC_SDRAM_ReadBuffer(u8 *pBuffer,u32 ReadAddr,u32 n)
{
	for(;n!=0;n--)
	{
		*pBuffer++=*(vu8*)(Bank5_SDRAM_ADDR+ReadAddr);
		ReadAddr++;
	}
}
