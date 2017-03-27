################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/linux/net/util/udponeway/tunltoc_meas.c \
../src/platform/linux/net/util/udponeway/tunrecv.c \
../src/platform/linux/net/util/udponeway/udp_sender1w.c \
../src/platform/linux/net/util/udponeway/udp_server1w.c 

OBJS += \
./src/platform/linux/net/util/udponeway/tunltoc_meas.o \
./src/platform/linux/net/util/udponeway/tunrecv.o \
./src/platform/linux/net/util/udponeway/udp_sender1w.o \
./src/platform/linux/net/util/udponeway/udp_server1w.o 

C_DEPS += \
./src/platform/linux/net/util/udponeway/tunltoc_meas.d \
./src/platform/linux/net/util/udponeway/tunrecv.d \
./src/platform/linux/net/util/udponeway/udp_sender1w.d \
./src/platform/linux/net/util/udponeway/udp_server1w.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/linux/net/util/udponeway/%.o: ../src/platform/linux/net/util/udponeway/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -Wall -Wextra  -g -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


