#ifndef _LCD_H
#define _LCD_H
#include "sys.h"

#define LCD_LED(n)              (n?HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_SET):HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_RESET))

typedef struct  
{							 
	u32 pwidth;
	u32 pheight;
	u16 hsw;
	u16 vsw;
	u16 hbp;
	u16 vbp;
	u16 hfp;
	u16 vfp;
	u8 activelayer;
	u8 dir;
	u16 width;
	u16 height;
	u32 pixsize;
}_ltdc_dev; 

extern _ltdc_dev lcdltdc;
extern LTDC_HandleTypeDef LTDC_Handler;
extern DMA2D_HandleTypeDef DMA2D_Handler;

#define LCD_PIXEL_FORMAT_ARGB8888       0X00    
#define LCD_PIXEL_FORMAT_RGB888         0X01    
#define LCD_PIXEL_FORMAT_RGB565         0X02       
#define LCD_PIXEL_FORMAT_ARGB1555       0X03      
#define LCD_PIXEL_FORMAT_ARGB4444       0X04     
#define LCD_PIXEL_FORMAT_L8             0X05     
#define LCD_PIXEL_FORMAT_AL44           0X06     
#define LCD_PIXEL_FORMAT_AL88           0X07      

#define LCD_PIXFORMAT				LCD_PIXEL_FORMAT_RGB565
#define LTDC_BACKLAYERCOLOR			0X00000000
#define LCD_FRAME_BUF_ADDR			0XC0000000  

void LTDC_Switch(u8 sw);
void LTDC_Layer_Switch(u8 layerx,u8 sw);
void LTDC_Select_Layer(u8 layerx);
void LTDC_Display_Dir(u8 dir);
void LTDC_Draw_Point(u16 x,u16 y,u32 color);
u32 LTDC_Read_Point(u16 x,u16 y);
void LTDC_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u32 color);
void LTDC_Color_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color);
void LTDC_Clear(u32 color);
u8 LTDC_Clk_Set(u32 pllsain,u32 pllsair,u32 pllsaidivr);
void LTDC_Layer_Window_Config(u8 layerx,u16 sx,u16 sy,u16 width,u16 height);
void LTDC_Layer_Parameter_Config(u8 layerx,u32 bufaddr,u8 pixformat,u8 alpha,u8 alpha0,u8 bfac1,u8 bfac2,u32 bkcolor);
u16 LTDC_PanelID_Read(void);
void LTDC_Init(void);
#endif 
