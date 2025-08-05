################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/sdl3-basic-computer-linux-build.cpp 

CPP_DEPS += \
./src/sdl3-basic-computer-linux-build.d 

OBJS += \
./src/sdl3-basic-computer-linux-build.o 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++26 -O0 -g3 -Wall -c -fmessage-length=0 -static -v -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-src

clean-src:
	-$(RM) ./src/sdl3-basic-computer-linux-build.d ./src/sdl3-basic-computer-linux-build.o

.PHONY: clean-src

