################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_cortex.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma2d.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma_ex.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_gpio.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_ltdc.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_nand.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_nor.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_pwr.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_pwr_ex.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_rcc.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_rcc_ex.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_sdram.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_sram.c \
../src/platform/cmx/src/stm32f7-hal/stm32f7xx_ll_fmc.c 

OBJS += \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_cortex.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma2d.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma_ex.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_gpio.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_ltdc.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_nand.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_nor.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_pwr.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_pwr_ex.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_rcc.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_rcc_ex.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_sdram.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_sram.o \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_ll_fmc.o 

C_DEPS += \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_cortex.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma2d.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_dma_ex.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_gpio.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_ltdc.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_nand.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_nor.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_pwr.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_pwr_ex.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_rcc.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_rcc_ex.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_sdram.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_hal_sram.d \
./src/platform/cmx/src/stm32f7-hal/stm32f7xx_ll_fmc.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/cmx/src/stm32f7-hal/%.o: ../src/platform/cmx/src/stm32f7-hal/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -O3 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -fno-move-loop-invariants  -g3 -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


