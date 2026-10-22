# Project-owned overlay; no BLE/RTOS/FlashDB initialization in this target.
ET100_FW := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
TOP_DIR := ../../../..
# Explicit list avoids accidentally linking application/PSRAM/sleep init.
override SRC_FILES := $(TOP_DIR)/components/drivers/device/fr30xx/system_fr30xx.c \
 $(TOP_DIR)/components/drivers/device/fr30xx/trim_fr30xx.c \
 $(TOP_DIR)/components/drivers/device/fr30xx/gcc/syscalls.c \
 $(TOP_DIR)/components/drivers/device/fr30xx/gcc/sysmem.c \
 $(TOP_DIR)/components/drivers/device/fr30xx/gcc/startup_fr30xx.S \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_flash.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_efuse.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_qspi.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_frspim.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_pmu.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_gpio.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_uart.c \
 $(TOP_DIR)/components/modules/common/src/co_util.c \
 $(TOP_DIR)/components/modules/crc/crc32.c
include Makefile
INC_PATH += -I"$(ET100_FW)/src" -I"$(TOP_DIR)/components/modules/mbedtls/include"
NOR_OBJECTS := nor_test_main nor_qspi1 ota_journal
$(BASE_ElF): $(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(NOR_OBJECTS))) \
 $(OBJECT_DIR)/sha256.o $(OBJECT_DIR)/platform_util.o
$(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(NOR_OBJECTS))): $(OBJECT_DIR)/%.o: $(ET100_FW)/src/%.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
$(OBJECT_DIR)/sha256.o: $(TOP_DIR)/components/modules/mbedtls/library/sha256.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(OBJECT_DIR)/platform_util.o: $(TOP_DIR)/components/modules/mbedtls/library/platform_util.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
