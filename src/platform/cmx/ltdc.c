#include "ltdc.h"
#include "lcd.h"
LTDC_HandleTypeDef  LTDC_Handler;
DMA2D_HandleTypeDef DMA2D_Handler;

#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888
	u32 ltdc_lcd_framebuf[1280][800] __attribute__((section(".erw0data")));
#else
	u16 ltdc_lcd_framebuf[1280][800] __attribute__((section(".erw0data")));
#endif

u32 *ltdc_framebuf[2];
_ltdc_dev lcdltdc;

void LTDC_Switch(u8 sw)
{
	if(sw==1) __HAL_LTDC_ENABLE(&LTDC_Handler);
	else if(sw==0)__HAL_LTDC_DISABLE(&LTDC_Handler);
}

void LTDC_Layer_Switch(u8 layerx,u8 sw)
{
	if(sw==1) __HAL_LTDC_LAYER_ENABLE(&LTDC_Handler,layerx);
	else if(sw==0) __HAL_LTDC_LAYER_DISABLE(&LTDC_Handler,layerx);
	__HAL_LTDC_RELOAD_CONFIG(&LTDC_Handler);
}

void LTDC_Select_Layer(u8 layerx)
{
	lcdltdc.activelayer=layerx;
}

void LTDC_Display_Dir(u8 dir)
{
    lcdltdc.dir=dir;
	if(dir==0)
	{
		lcdltdc.width=lcdltdc.pheight;
		lcdltdc.height=lcdltdc.pwidth;	
	}else if(dir==1)
	{
		lcdltdc.width=lcdltdc.pwidth;
		lcdltdc.height=lcdltdc.pheight;
	}
}

void LTDC_Draw_Point(u16 x,u16 y,u32 color)
{ 
#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888
	if(lcdltdc.dir)
	{
        *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x))=color;
	}else
	{
        *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y))=color; 
	}
#else
	if(lcdltdc.dir)
	{
        *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x))=color;
	}else
	{
        *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y))=color; 
	}
#endif
}

u32 LTDC_Read_Point(u16 x,u16 y)
{ 
#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888
	if(lcdltdc.dir)
	{
		return *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x));
	}else
	{
		return *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y)); 
	}
#else
	if(lcdltdc.dir)
	{
		return *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x));
	}else
	{
		return *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y)); 
	}
#endif 
}

void LTDC_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u32 color)
{ 
	u32 psx,psy,pex,pey;
	u32 timeout=0; 
	u16 offline;
	u32 addr; 

	if(lcdltdc.dir)
	{
		psx=sx;psy=sy;
		pex=ex;pey=ey;
	}else
	{
		psx=sy;psy=lcdltdc.pheight-ex-1;
		pex=ey;pey=lcdltdc.pheight-sx-1;
	} 
	offline=lcdltdc.pwidth-(pex-psx+1);
	addr=((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*psy+psx));
	RCC->AHB1ENR|=1<<23;
	DMA2D->CR=3<<16;
	DMA2D->OPFCCR=LCD_PIXFORMAT;
	DMA2D->OOR=offline;
	DMA2D->CR&=~(1<<0);
	DMA2D->OMAR=addr;
	DMA2D->NLR=(pey-psy+1)|((pex-psx+1)<<16);
	DMA2D->OCOLR=color;
	DMA2D->CR|=1<<0;
	while((DMA2D->ISR&(1<<1))==0)
	{
		timeout++;
		if(timeout>0X1FFFFF)break;
	}  
	DMA2D->IFCR|=1<<1;
}

void LTDC_Color_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color)
{
	u32 psx,psy,pex,pey;
	u32 timeout=0; 
	u16 offline;
	u32 addr; 

	if(lcdltdc.dir)
	{
		psx=sx;psy=sy;
		pex=ex;pey=ey;
	}else
	{
		psx=sy;psy=lcdltdc.pheight-ex-1;
		pex=ey;pey=lcdltdc.pheight-sx-1;
	}
	offline=lcdltdc.pwidth-(pex-psx+1);
	addr=((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*psy+psx));
	RCC->AHB1ENR|=1<<23;
	DMA2D->CR=0<<16;
	DMA2D->FGPFCCR=LCD_PIXFORMAT;
	DMA2D->FGOR=0;
	DMA2D->OOR=offline;
	DMA2D->CR&=~(1<<0);
	DMA2D->FGMAR=(u32)color;
	DMA2D->OMAR=addr;
	DMA2D->NLR=(pey-psy+1)|((pex-psx+1)<<16);
	DMA2D->CR|=1<<0;
	while((DMA2D->ISR&(1<<1))==0)
	{
		timeout++;
		if(timeout>0X1FFFFF)break;
	} 
	DMA2D->IFCR|=1<<1;
}

void LTDC_Clear(u32 color)
{
	LTDC_Fill(0,0,lcdltdc.width-1,lcdltdc.height-1,color);
}

u8 LTDC_Clk_Set(u32 pllsain,u32 pllsair,u32 pllsaidivr)
{
	RCC_PeriphCLKInitTypeDef PeriphClkIniture;

    PeriphClkIniture.PeriphClockSelection=RCC_PERIPHCLK_LTDC;
	PeriphClkIniture.PLLSAI.PLLSAIN=pllsain;    
	PeriphClkIniture.PLLSAI.PLLSAIR=pllsair;  
	PeriphClkIniture.PLLSAIDivR=pllsaidivr;
	if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkIniture)==HAL_OK)
        return 0;
    else 
        return 1;
}

void LTDC_Layer_Window_Config(u8 layerx,u16 sx,u16 sy,u16 width,u16 height)
{
    HAL_LTDC_SetWindowPosition(&LTDC_Handler,sx,sy,layerx);
    HAL_LTDC_SetWindowSize(&LTDC_Handler,width,height,layerx);
}

void LTDC_Layer_Parameter_Config(u8 layerx,u32 bufaddr,u8 pixformat,u8 alpha,u8 alpha0,u8 bfac1,u8 bfac2,u32 bkcolor)
{
	LTDC_LayerCfgTypeDef pLayerCfg;
	
	pLayerCfg.WindowX0=0;
	pLayerCfg.WindowY0=0;
	pLayerCfg.WindowX1=lcdltdc.pwidth;
	pLayerCfg.WindowY1=lcdltdc.pheight;
	pLayerCfg.PixelFormat=pixformat;
	pLayerCfg.Alpha=alpha;
	pLayerCfg.Alpha0=alpha0;
	pLayerCfg.BlendingFactor1=(u32)bfac1<<8;
	pLayerCfg.BlendingFactor2=(u32)bfac2<<8;
	pLayerCfg.FBStartAdress=bufaddr;
	pLayerCfg.ImageWidth=lcdltdc.pwidth;
	pLayerCfg.ImageHeight=lcdltdc.pheight;
	pLayerCfg.Backcolor.Red=(u8)(bkcolor&0X00FF0000)>>16;
	pLayerCfg.Backcolor.Green=(u8)(bkcolor&0X0000FF00)>>8;
	pLayerCfg.Backcolor.Blue=(u8)bkcolor&0X000000FF;
	HAL_LTDC_ConfigLayer(&LTDC_Handler,&pLayerCfg,layerx);
}  

u16 LTDC_PanelID_Read(void)
{
	u8 idx=0;
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOI_CLK_ENABLE();
    
    GPIO_Initure.Pin=GPIO_PIN_6;
    GPIO_Initure.Mode=GPIO_MODE_INPUT;
    GPIO_Initure.Pull=GPIO_PULLUP;
    GPIO_Initure.Speed=GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOG,&GPIO_Initure);
    
    GPIO_Initure.Pin=GPIO_PIN_2|GPIO_PIN_7;
    HAL_GPIO_Init(GPIOI,&GPIO_Initure);
    
    idx=(u8)HAL_GPIO_ReadPin(GPIOG,GPIO_PIN_6);
    idx|=(u8)HAL_GPIO_ReadPin(GPIOI,GPIO_PIN_2)<<1;
    idx|=(u8)HAL_GPIO_ReadPin(GPIOI,GPIO_PIN_7)<<2;

	if(idx==0)
        return 0X4342;
	else if(idx==1)
        return 0X7084;
	else if(idx==2)
        return 0X7016;
	else if(idx==3)
        return 0X7018;
	else if(idx==4)
        return 0X8017;
	else 
        return 0;
}

void LTDC_Init(void)
{   
	u16 lcdid=0;
	
	lcdid=LTDC_PanelID_Read();
	if(lcdid==0X4342)
	{
		lcdltdc.pwidth=480;
		lcdltdc.pheight=272;
		lcdltdc.hsw=1;
		lcdltdc.vsw=1;
		lcdltdc.hbp=40;
		lcdltdc.vbp=8;
		lcdltdc.hfp=5;
		lcdltdc.vfp=8;
        LTDC_Clk_Set(288,4,RCC_PLLSAIDIVR_8);

	}else if(lcdid==0X7084)
	{
		lcdltdc.pwidth=800;
		lcdltdc.pheight=480;
		lcdltdc.hsw=1;
		lcdltdc.vsw=1;
		lcdltdc.hbp=46;
		lcdltdc.vbp=23;
		lcdltdc.hfp=210;
		lcdltdc.vfp=22;
        LTDC_Clk_Set(396,3,RCC_PLLSAIDIVR_4);
	}else if(lcdid==0X7016)		
	{
		lcdltdc.pwidth=1024;
		lcdltdc.pheight=600;
        lcdltdc.hsw=20;
		lcdltdc.vsw=3;
		lcdltdc.hbp=140;
		lcdltdc.vbp=20;
		lcdltdc.hfp=160;
		lcdltdc.vfp=12;
		LTDC_Clk_Set(360,2,RCC_PLLSAIDIVR_4);
	}else if(lcdid==0X7018)		
	{
		lcdltdc.pwidth=1280;
		lcdltdc.pheight=800;
	}else if(lcdid==0X8017)		
	{
		lcdltdc.pwidth=1024;
		lcdltdc.pheight=768;
    }

	lcddev.width=lcdltdc.pwidth;
	lcddev.height=lcdltdc.pheight;
    
#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888 
	ltdc_framebuf[0]=(u32*)&ltdc_lcd_framebuf;
	lcdltdc.pixsize=4;
#else 
    lcdltdc.pixsize=2;
	ltdc_framebuf[0]=(u32*)&ltdc_lcd_framebuf;
#endif 	
    
    LTDC_Handler.Instance=LTDC;
    LTDC_Handler.Init.HSPolarity=LTDC_HSPOLARITY_AL;
    LTDC_Handler.Init.VSPolarity=LTDC_VSPOLARITY_AL;
    LTDC_Handler.Init.DEPolarity=LTDC_DEPOLARITY_AL;
    LTDC_Handler.Init.PCPolarity=LTDC_PCPOLARITY_IPC;
    LTDC_Handler.Init.HorizontalSync=lcdltdc.hsw-1;
    LTDC_Handler.Init.VerticalSync=lcdltdc.vsw-1;
    LTDC_Handler.Init.AccumulatedHBP=lcdltdc.hsw+lcdltdc.hbp-1;
    LTDC_Handler.Init.AccumulatedVBP=lcdltdc.vsw+lcdltdc.vbp-1;
    LTDC_Handler.Init.AccumulatedActiveW=lcdltdc.hsw+lcdltdc.hbp+lcdltdc.pwidth-1;
    LTDC_Handler.Init.AccumulatedActiveH=lcdltdc.vsw+lcdltdc.vbp+lcdltdc.pheight-1;
    LTDC_Handler.Init.TotalWidth=lcdltdc.hsw+lcdltdc.hbp+lcdltdc.pwidth+lcdltdc.hfp-1;
    LTDC_Handler.Init.TotalHeigh=lcdltdc.vsw+lcdltdc.vbp+lcdltdc.pheight+lcdltdc.vfp-1;
    LTDC_Handler.Init.Backcolor.Red=0;
    LTDC_Handler.Init.Backcolor.Green=0;
    LTDC_Handler.Init.Backcolor.Blue=0;
    HAL_LTDC_Init(&LTDC_Handler);

	LTDC_Layer_Parameter_Config(0,(u32)ltdc_framebuf[0],LCD_PIXFORMAT,255,0,6,7,0X000000);
	LTDC_Layer_Window_Config(0,0,0,lcdltdc.pwidth,lcdltdc.pheight);
	 	
 	LTDC_Display_Dir(0);
	LTDC_Select_Layer(0);
    LCD_LED(1);
    LTDC_Clear(0XFFFFFFFF);
}

void HAL_LTDC_MspInit(LTDC_HandleTypeDef* hltdc)
{
    GPIO_InitTypeDef GPIO_Initure;
    
    __HAL_RCC_LTDC_CLK_ENABLE();
    __HAL_RCC_DMA2D_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    
    GPIO_Initure.Pin=GPIO_PIN_5;
    GPIO_Initure.Mode=GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull=GPIO_PULLUP;
    GPIO_Initure.Speed=GPIO_SPEED_HIGH;
    HAL_GPIO_Init(GPIOB,&GPIO_Initure);

    GPIO_Initure.Pin=GPIO_PIN_10; 
    GPIO_Initure.Mode=GPIO_MODE_AF_PP;
    GPIO_Initure.Pull=GPIO_NOPULL;              
    GPIO_Initure.Speed=GPIO_SPEED_HIGH;
    GPIO_Initure.Alternate=GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOF,&GPIO_Initure);
    
    GPIO_Initure.Pin=GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_11;
    HAL_GPIO_Init(GPIOG,&GPIO_Initure);
    
    GPIO_Initure.Pin=GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|\
                     GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
    HAL_GPIO_Init(GPIOH,&GPIO_Initure);
    
    GPIO_Initure.Pin=GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_4|GPIO_PIN_5|\
                     GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_9|GPIO_PIN_10;
    HAL_GPIO_Init(GPIOI,&GPIO_Initure); 
}

