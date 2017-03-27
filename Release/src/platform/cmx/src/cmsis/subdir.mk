################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/cmx/src/cmsis/startup_stm32f767.c \
../src/platform/cmx/src/cmsis/system_stm32f7xx.c 

OBJS += \
./src/platform/cmx/src/cmsis/startup_stm32f767.o \
./src/platform/cmx/src/cmsis/system_stm32f7xx.o 

C_DEPS += \
./src/platform/cmx/src/cmsis/startup_stm32f767.d \
./src/platform/cmx/src/cmsis/system_stm32f7xx.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/cmx/src/cmsis/%.o: ../src/platform/cmx/src/cmsis/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -O3 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding  -g -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


