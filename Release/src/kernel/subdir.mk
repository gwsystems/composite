################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/kernel/capinv.c \
../src/kernel/captbl.c \
../src/kernel/liveness_tbl.c \
../src/kernel/pgtbl.c \
../src/kernel/retype_tbl.c \
../src/kernel/tcap.c 

OBJS += \
./src/kernel/capinv.o \
./src/kernel/captbl.o \
./src/kernel/liveness_tbl.o \
./src/kernel/pgtbl.o \
./src/kernel/retype_tbl.o \
./src/kernel/tcap.o 

C_DEPS += \
./src/kernel/capinv.d \
./src/kernel/captbl.d \
./src/kernel/liveness_tbl.d \
./src/kernel/pgtbl.d \
./src/kernel/retype_tbl.d \
./src/kernel/tcap.d 


# Each subdirectory must supply rules for building sources it contributes
src/kernel/%.o: ../src/kernel/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -O3 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding  -g -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


