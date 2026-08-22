set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Robust ARM GNU toolchain discovery for macOS/Linux and VS Code launched from GUI.
# Prefer PATH, then common Homebrew locations.
set(_ARM_TOOLCHAIN_HINTS
    /opt/homebrew/bin
    /usr/local/bin
    $ENV{HOME}/.local/bin
    $ENV{HOME}/arm-gnu-toolchain/bin
)

find_program(ARM_NONE_EABI_GCC NAMES arm-none-eabi-gcc HINTS ${_ARM_TOOLCHAIN_HINTS} REQUIRED)
find_program(ARM_NONE_EABI_GXX NAMES arm-none-eabi-g++ HINTS ${_ARM_TOOLCHAIN_HINTS} REQUIRED)
find_program(ARM_NONE_EABI_OBJCOPY NAMES arm-none-eabi-objcopy HINTS ${_ARM_TOOLCHAIN_HINTS} REQUIRED)
find_program(ARM_NONE_EABI_SIZE NAMES arm-none-eabi-size HINTS ${_ARM_TOOLCHAIN_HINTS} REQUIRED)

set(CMAKE_C_COMPILER   "${ARM_NONE_EABI_GCC}" CACHE FILEPATH "ARM GCC compiler" FORCE)
set(CMAKE_ASM_COMPILER "${ARM_NONE_EABI_GCC}" CACHE FILEPATH "ARM assembler" FORCE)
set(CMAKE_CXX_COMPILER "${ARM_NONE_EABI_GXX}" CACHE FILEPATH "ARM G++ compiler" FORCE)
set(CMAKE_OBJCOPY      "${ARM_NONE_EABI_OBJCOPY}" CACHE FILEPATH "ARM objcopy" FORCE)
set(CMAKE_SIZE         "${ARM_NONE_EABI_SIZE}" CACHE FILEPATH "ARM size" FORCE)

set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C   ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TARGET_FLAGS "-mcpu=cortex-m0plus ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fdata-sections -ffunction-sections")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(TOOLCHAIN_LINK_LIBRARIES "m")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "--specs=nosys.specs -Wl,--gc-sections -Wl,--no-warn-rwx-segment")
