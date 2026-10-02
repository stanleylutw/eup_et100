# ET-100 Firmware top-level CMake
#
# 依賴：
#   - CMAKE_TOOLCHAIN_FILE = cmake/arm-none-eabi.cmake
#   - vendor/fr30xxc_sdk__202411/ 已 fetch（tools/fetch_sdk.sh）
#
# 支援的 target：
#   - app              主韌體（P1 EVT board, FR3068E-C）
#   - app.hex          Intel HEX（給 FLM 燒錄）
#   - app.bin          raw binary（給 FreqChip_Download serial 工具）
#   - app.dfu          DFU image（給 OTA；bootloader 待實作）
#
# TODO Phase 0（本次只建骨架）：
#   實際 build 到可燒錄的 .elf 需要在拿到 EVB 之後，用 SDK 的 ble_simple_periphreal
#   範例做首次 build 驗證，然後再 port 本專案 main.c。目前 CMakeLists 僅確立
#   目錄結構與工具鏈，不保證可 build。

cmake_minimum_required(VERSION 3.20)

# 專案資訊
project(et100_fw
    VERSION 0.1.0
    LANGUAGES C CXX ASM
    DESCRIPTION "EUP ET-100 Vehicle Tracker Firmware (Eupfin custom)"
)

# ---------------------------------------------------------------
# SDK 路徑
# ---------------------------------------------------------------
set(SDK_ROOT "${CMAKE_SOURCE_DIR}/vendor/fr30xxc_sdk__202411" CACHE PATH "Freqchip SDK root")

if(NOT EXISTS "${SDK_ROOT}/components/drivers/device/fr30xx/fr30xx.h")
    message(FATAL_ERROR
        "Freqchip SDK not found at ${SDK_ROOT}\n"
        "Run tools/fetch_sdk.sh first."
    )
endif()

message(STATUS "SDK root: ${SDK_ROOT}")

# ---------------------------------------------------------------
# Linker script (選晶片：3068e)
# ---------------------------------------------------------------
set(LDSCRIPT "${SDK_ROOT}/components/tools/gcc/ldscript_3068e.ld")

# ---------------------------------------------------------------
# BLE / BT host library（binary archive）
# ---------------------------------------------------------------
set(BTDM_LIB "${SDK_ROOT}/components/btdm/libbtdm_host.a")

# ---------------------------------------------------------------
# SDK source（基本 driver 集合，之後按需擴充）
# ---------------------------------------------------------------
set(SDK_SOURCES
    # Device / startup
    ${SDK_ROOT}/components/drivers/device/fr30xx/system_fr30xx.c
    ${SDK_ROOT}/components/drivers/device/fr30xx/trim_fr30xx.c
    ${SDK_ROOT}/components/drivers/device/fr30xx/gcc/syscalls.c
    ${SDK_ROOT}/components/drivers/device/fr30xx/gcc/sysmem.c
    ${SDK_ROOT}/components/drivers/device/fr30xx/gcc/startup_fr30xx.S

    # Peripheral drivers（最小子集；按需加）
    ${SDK_ROOT}/components/drivers/peripheral/Src/driver_cali.c
    ${SDK_ROOT}/components/drivers/peripheral/Src/driver_efuse.c
    ${SDK_ROOT}/components/drivers/peripheral/Src/driver_flash.c
    ${SDK_ROOT}/components/drivers/peripheral/Src/driver_frspim.c
    ${SDK_ROOT}/components/drivers/peripheral/Src/driver_gpio.c
    ${SDK_ROOT}/components/drivers/peripheral/Src/driver_pmu.c
    ${SDK_ROOT}/components/drivers/peripheral/Src/driver_pmu_iwdt.c
    # 未來加入：uart、i2c、spi、can、rtc、timer、adc、pwm……
)

set(SDK_INCLUDES
    ${SDK_ROOT}/components/drivers/cmsis
    ${SDK_ROOT}/components/drivers/device/fr30xx
    ${SDK_ROOT}/components/drivers/bsp
    ${SDK_ROOT}/components/drivers/peripheral/Inc
    ${SDK_ROOT}/components/btdm/include
    ${SDK_ROOT}/components/modules/common
)

# ---------------------------------------------------------------
# Application source
# ---------------------------------------------------------------
set(APP_SOURCES
    src/main.c
)

set(APP_INCLUDES
    include
)

# ---------------------------------------------------------------
# Build target
# ---------------------------------------------------------------
add_executable(app
    ${APP_SOURCES}
    ${SDK_SOURCES}
)

target_include_directories(app PRIVATE
    ${APP_INCLUDES}
    ${SDK_INCLUDES}
)

target_compile_definitions(app PRIVATE
    CHIP_SEL_FR3068EC_V1=1   # 從 fr30xx.h 預設型號
)

# Link BLE binary library + linker script
target_link_options(app PRIVATE
    "-T${LDSCRIPT}"
)
target_link_libraries(app PRIVATE
    ${BTDM_LIB}
    -lc -lm -lnosys
)

# .elf 檔副檔名
set_target_properties(app PROPERTIES SUFFIX ".elf")

# ---------------------------------------------------------------
# Post-build：產出 .hex / .bin / size info
# ---------------------------------------------------------------
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:app> ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:app> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} --format=berkeley $<TARGET_FILE:app>
    COMMENT "Generating .hex / .bin and reporting size"
)
