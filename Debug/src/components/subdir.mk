################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
MODULES += \
./src/components/mb_comp.o

# Each subdirectory must supply rules for building sources it contributes
src/components/mb_comp.o: $(COMP_OBJS)
	@echo $(COMP_OBJS)
	@echo 'Building component: $<'
	@echo 'Invoking: Cross ARM C++ Linker'
	arm-none-eabi-g++ -mcpu=cortex-m7 -mthumb -mfloat-abi=soft -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -ffreestanding -fno-move-loop-invariants -nostartfiles -nostdlib -g3 -o "./src/components/mb_comp.o" -r $(COMP_OBJS)
	@echo 'Finished building: $<'
	@echo ' '


