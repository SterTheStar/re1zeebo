# zeebo-arm.cmake - CMake toolchain for Qualcomm BREW 4.0.2 / Zeebo (ARM11 / ARMv6K)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(ZEEBO_CC  arm-none-eabi-gcc)
find_program(ZEEBO_CXX arm-none-eabi-g++)
find_program(ZEEBO_OBJCOPY arm-none-eabi-objcopy)

if(NOT ZEEBO_CC OR NOT ZEEBO_CXX)
    message(FATAL_ERROR
        "arm-none-eabi-gcc / arm-none-eabi-g++ not found in PATH.\n"
        "Install the Arm GNU Toolchain (arm-none-eabi) to build for Zeebo.")
endif()

set(CMAKE_C_COMPILER   "${ZEEBO_CC}")
set(CMAKE_CXX_COMPILER "${ZEEBO_CXX}")
set(CMAKE_ASM_COMPILER "${ZEEBO_CC}")

set(ZEEBO_ARCH_FLAGS "-march=armv6k -marm -mfpu=vfp -mfloat-abi=softfp")
set(CMAKE_C_FLAGS_INIT   "${ZEEBO_ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ZEEBO_ARCH_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${ZEEBO_ARCH_FLAGS}")
