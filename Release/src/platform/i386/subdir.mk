################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/platform/i386/boot_comp.c \
../src/platform/i386/chal.c \
../src/platform/i386/console.c \
../src/platform/i386/exception.c \
../src/platform/i386/gdt.c \
../src/platform/i386/hpet.c \
../src/platform/i386/idt.c \
../src/platform/i386/kernel.c \
../src/platform/i386/lapic.c \
../src/platform/i386/miniacpi.c \
../src/platform/i386/printk.c \
../src/platform/i386/serial.c \
../src/platform/i386/string.c \
../src/platform/i386/tss.c \
../src/platform/i386/user.c \
../src/platform/i386/vga.c \
../src/platform/i386/vm.c \
../src/platform/i386/vtxprintf.c 

S_UPPER_SRCS += \
../src/platform/i386/entry.S \
../src/platform/i386/loader.S 

OBJS += \
./src/platform/i386/boot_comp.o \
./src/platform/i386/chal.o \
./src/platform/i386/console.o \
./src/platform/i386/entry.o \
./src/platform/i386/exception.o \
./src/platform/i386/gdt.o \
./src/platform/i386/hpet.o \
./src/platform/i386/idt.o \
./src/platform/i386/kernel.o \
./src/platform/i386/lapic.o \
./src/platform/i386/loader.o \
./src/platform/i386/miniacpi.o \
./src/platform/i386/printk.o \
./src/platform/i386/serial.o \
./src/platform/i386/string.o \
./src/platform/i386/tss.o \
./src/platform/i386/user.o \
./src/platform/i386/vga.o \
./src/platform/i386/vm.o \
./src/platform/i386/vtxprintf.o 

S_UPPER_DEPS += \
./src/platform/i386/entry.d \
./src/platform/i386/loader.d 

C_DEPS += \
./src/platform/i386/boot_comp.d \
./src/platform/i386/chal.d \
./src/platform/i386/console.d \
./src/platform/i386/exception.d \
./src/platform/i386/gdt.d \
./src/platform/i386/hpet.d \
./src/platform/i386/idt.d \
./src/platform/i386/kernel.d \
./src/platform/i386/lapic.d \
./src/platform/i386/miniacpi.d \
./src/platform/i386/printk.d \
./src/platform/i386/serial.d \
./src/platform/i386/string.d \
./src/platform/i386/tss.d \
./src/platform/i386/user.d \
./src/platform/i386/vga.d \
./src/platform/i386/vm.d \
./src/platform/i386/vtxprintf.d 


# Each subdirectory must supply rules for building sources it contributes
src/platform/i386/%.o: ../src/platform/i386/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -Wall -Wextra  -g -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/platform/i386/%.o: ../src/platform/i386/%.S
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM GNU Assembler'
	arm-none-eabi-gcc -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -Wall -Wextra  -g -x assembler-with-cpp -DDEBUG -DUSE_FULL_ASSERT -DTRACE -DSTM32F767xx -DUSE_HAL_DRIVER -DHSE_VALUE=8000000 -I"/home/pry/cos_arm/test/src/kernel/include" -I"/home/pry/cos_arm/test/src/kernel/include/shared" -I"/home/pry/cos_arm/test/src/components/include" -I"/home/pry/cos_arm/test/src/platform/cmx" -I"/home/pry/cos_arm/test/src/platform/cmx/include/cmsis" -I"/home/pry/cos_arm/test/src/platform/cmx/include/stm32f7-hal" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


