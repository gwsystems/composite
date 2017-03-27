################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/linux/translator/helpers/ltoc_meas/ltoc_meas.c 

OBJS += \
./src/platform/linux/translator/helpers/ltoc_meas/ltoc_meas.o 

C_DEPS += \
./src/platform/linux/translator/helpers/ltoc_meas/ltoc_meas.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/linux/translator/helpers/ltoc_meas/%.o: ../src/platform/linux/translator/helpers/ltoc_meas/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -Wall -Wextra  -g -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


