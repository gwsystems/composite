################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/cmx/boot_comp.c \
../src/platform/cmx/chal.c \
../src/platform/cmx/kernel.c \
../src/platform/cmx/lcd.c \
../src/platform/cmx/ltdc.c \
../src/platform/cmx/sdram.c \
../src/platform/cmx/utmem.c \
../src/platform/cmx/vm.c 

S_UPPER_SRCS += \
../src/platform/cmx/entry.S 

OBJS += \
./src/platform/cmx/boot_comp.o \
./src/platform/cmx/chal.o \
./src/platform/cmx/entry.o \
./src/platform/cmx/kernel.o \
./src/platform/cmx/lcd.o \
./src/platform/cmx/ltdc.o \
./src/platform/cmx/sdram.o \
./src/platform/cmx/utmem.o \
./src/platform/cmx/vm.o 

S_UPPER_DEPS += \
./src/platform/cmx/entry.d 

C_DEPS += \
./src/platform/cmx/boot_comp.d \
./src/platform/cmx/chal.d \
./src/platform/cmx/kernel.d \
./src/platform/cmx/lcd.d \
./src/platform/cmx/ltdc.d \
./src/platform/cmx/sdram.d \
./src/platform/cmx/utmem.d \
./src/platform/cmx/vm.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/cmx/boot_comp.o: ../src/platform/cmx/boot_comp.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -fno-move-loop-invariants  -g3 -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -O0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"src/platform/cmx/boot_comp.d" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/platform/cmx/%.o: ../src/platform/cmx/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -fno-move-loop-invariants  -g3 -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/platform/cmx/%.o: ../src/platform/cmx/%.S
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU Assembler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -fno-move-loop-invariants  -g3 -x assembler-with-cpp -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


