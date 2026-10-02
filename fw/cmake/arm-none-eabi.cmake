# CMake toolchain file for ARM GNU Toolchain + Cortex-M33
#
# 使用：
#   cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake ..

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 禁用主機測試（arm-none-eabi 無 libc runtime 給 host 測試）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 工具鏈前綴（可用 TOOLCHAIN_PREFIX env var 覆寫）
if(NOT DEFINED TOOLCHAIN_PREFIX)
    set(TOOLCHAIN_PREFIX arm-none-eabi-)
endif()

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_LINKER       ${TOOLCHAIN_PREFIX}ld)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}ranlib)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}objdump)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

# Cortex-M33 具 FPU 但 Freqchip startup 用 softvfp（見 SDK startup_fr30xx.S）
set(CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=soft")
# 若要用 hardfp（有 FPU 硬浮點加速），改成：
# set(CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")

# 共用編譯 flag
set(COMMON_FLAGS
    "${CPU_FLAGS} \
    -ffunction-sections \
    -fdata-sections \
    -fno-common \
    -Wall \
    -Wextra \
    -Wshadow \
    -Wno-unused-parameter \
    -fno-strict-aliasing"
)

set(CMAKE_C_FLAGS_INIT   "${COMMON_FLAGS} -std=gnu11")
set(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS} -std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

# Linker flag — 讓 ld 處理 unused section 剃除、--print-memory-usage 看 flash/ram 用量
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${CPU_FLAGS} \
    -specs=nano.specs \
    -specs=nosys.specs \
    -Wl,--gc-sections \
    -Wl,--print-memory-usage \
    -Wl,-Map=\${CMAKE_PROJECT_NAME}.map"
)

# 預設 Debug build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Debug: -Og 保留除錯符號但輕度最佳化（比 -O0 size 小且 debug 友善）
set(CMAKE_C_FLAGS_DEBUG   "-Og -g3 -gdwarf-2")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3 -gdwarf-2")

# Release: -Os size-optimised；若需效能改 -O2 或 -O3
set(CMAKE_C_FLAGS_RELEASE   "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")

# 不要跑 compiler test（嵌入式 target 無 stdio）
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
