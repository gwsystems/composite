################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/linux/net/util/cnet_user.c \
../src/platform/linux/net/util/http_cli.c \
../src/platform/linux/net/util/tcp_client.c \
../src/platform/linux/net/util/tcp_server.c \
../src/platform/linux/net/util/udp_client.c \
../src/platform/linux/net/util/udp_server.c 

OBJS += \
./src/platform/linux/net/util/cnet_user.o \
./src/platform/linux/net/util/http_cli.o \
./src/platform/linux/net/util/tcp_client.o \
./src/platform/linux/net/util/tcp_server.o \
./src/platform/linux/net/util/udp_client.o \
./src/platform/linux/net/util/udp_server.o 

C_DEPS += \
./src/platform/linux/net/util/cnet_user.d \
./src/platform/linux/net/util/http_cli.d \
./src/platform/linux/net/util/tcp_client.d \
./src/platform/linux/net/util/tcp_server.d \
./src/platform/linux/net/util/udp_client.d \
./src/platform/linux/net/util/udp_server.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/linux/net/util/%.o: ../src/platform/linux/net/util/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -Wall -Wextra  -g -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


