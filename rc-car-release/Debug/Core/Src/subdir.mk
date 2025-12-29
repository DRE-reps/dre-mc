################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/VL53L0X.c \
../Core/Src/converters.c \
../Core/Src/debug.c \
../Core/Src/esc.c \
../Core/Src/logger.c \
../Core/Src/main.c \
../Core/Src/measure_speed_FC33.c \
../Core/Src/mpu6050.c \
../Core/Src/steering_servo.c \
../Core/Src/stm32l1xx_hal_msp.c \
../Core/Src/stm32l1xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32l1xx.c \
../Core/Src/usart1_callbacks.c 

OBJS += \
./Core/Src/VL53L0X.o \
./Core/Src/converters.o \
./Core/Src/debug.o \
./Core/Src/esc.o \
./Core/Src/logger.o \
./Core/Src/main.o \
./Core/Src/measure_speed_FC33.o \
./Core/Src/mpu6050.o \
./Core/Src/steering_servo.o \
./Core/Src/stm32l1xx_hal_msp.o \
./Core/Src/stm32l1xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32l1xx.o \
./Core/Src/usart1_callbacks.o 

C_DEPS += \
./Core/Src/VL53L0X.d \
./Core/Src/converters.d \
./Core/Src/debug.d \
./Core/Src/esc.d \
./Core/Src/logger.d \
./Core/Src/main.d \
./Core/Src/measure_speed_FC33.d \
./Core/Src/mpu6050.d \
./Core/Src/steering_servo.d \
./Core/Src/stm32l1xx_hal_msp.d \
./Core/Src/stm32l1xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32l1xx.d \
./Core/Src/usart1_callbacks.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L151xC -c -I../Core/Inc -I../Drivers/STM32L1xx_HAL_Driver/Inc -I../Drivers/STM32L1xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L1xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/VL53L0X.cyclo ./Core/Src/VL53L0X.d ./Core/Src/VL53L0X.o ./Core/Src/VL53L0X.su ./Core/Src/converters.cyclo ./Core/Src/converters.d ./Core/Src/converters.o ./Core/Src/converters.su ./Core/Src/debug.cyclo ./Core/Src/debug.d ./Core/Src/debug.o ./Core/Src/debug.su ./Core/Src/esc.cyclo ./Core/Src/esc.d ./Core/Src/esc.o ./Core/Src/esc.su ./Core/Src/logger.cyclo ./Core/Src/logger.d ./Core/Src/logger.o ./Core/Src/logger.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/measure_speed_FC33.cyclo ./Core/Src/measure_speed_FC33.d ./Core/Src/measure_speed_FC33.o ./Core/Src/measure_speed_FC33.su ./Core/Src/mpu6050.cyclo ./Core/Src/mpu6050.d ./Core/Src/mpu6050.o ./Core/Src/mpu6050.su ./Core/Src/steering_servo.cyclo ./Core/Src/steering_servo.d ./Core/Src/steering_servo.o ./Core/Src/steering_servo.su ./Core/Src/stm32l1xx_hal_msp.cyclo ./Core/Src/stm32l1xx_hal_msp.d ./Core/Src/stm32l1xx_hal_msp.o ./Core/Src/stm32l1xx_hal_msp.su ./Core/Src/stm32l1xx_it.cyclo ./Core/Src/stm32l1xx_it.d ./Core/Src/stm32l1xx_it.o ./Core/Src/stm32l1xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32l1xx.cyclo ./Core/Src/system_stm32l1xx.d ./Core/Src/system_stm32l1xx.o ./Core/Src/system_stm32l1xx.su ./Core/Src/usart1_callbacks.cyclo ./Core/Src/usart1_callbacks.d ./Core/Src/usart1_callbacks.o ./Core/Src/usart1_callbacks.su

.PHONY: clean-Core-2f-Src

