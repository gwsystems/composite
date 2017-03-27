################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/linker/deserialize.c \
../src/platform/linker/gen_client_stub.c \
../src/platform/linker/genstubs.c \
../src/platform/linker/globals.c \
../src/platform/linker/loadall.c \
../src/platform/linker/main.c \
../src/platform/linker/output.c \
../src/platform/linker/prepsymbs.c \
../src/platform/linker/printobjs.c \
../src/platform/linker/vdc.c \
../src/platform/linker/vds.c 

OBJS += \
./src/platform/linker/deserialize.o \
./src/platform/linker/gen_client_stub.o \
./src/platform/linker/genstubs.o \
./src/platform/linker/globals.o \
./src/platform/linker/loadall.o \
./src/platform/linker/main.o \
./src/platform/linker/output.o \
./src/platform/linker/prepsymbs.o \
./src/platform/linker/printobjs.o \
./src/platform/linker/vdc.o \
./src/platform/linker/vds.o 

C_DEPS += \
./src/platform/linker/deserialize.d \
./src/platform/linker/gen_client_stub.d \
./src/platform/linker/genstubs.d \
./src/platform/linker/globals.d \
./src/platform/linker/loadall.d \
./src/platform/linker/main.d \
./src/platform/linker/output.d \
./src/platform/linker/prepsymbs.d \
./src/platform/linker/printobjs.d \
./src/platform/linker/vdc.d \
./src/platform/linker/vds.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/linker/%.o: ../src/platform/linker/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -Wall -Wextra  -g -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


