ET100_FW := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
TOP_DIR := ../../../..
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
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_pmu_iwdt.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_gpio.c \
 $(TOP_DIR)/components/drivers/peripheral/Src/driver_uart.c \
 $(TOP_DIR)/components/modules/common/src/co_util.c \
 $(TOP_DIR)/components/modules/crc/crc32.c
include Makefile
LD := $(ET100_FW)/installer/installer.ld
LIB :=
LDFLAGS += -L$(ET100_FW)/installer
INC_PATH += -I"$(ET100_FW)/common" -I"$(ET100_FW)/installer" -I"$(ET100_FW)/include" -I"$(ET100_FW)/src" -I"$(TOP_DIR)/components/modules/mbedtls/include"
STUB_OBJECTS := main mcuboot_image candidate loader_frame install device jump transaction
$(BASE_ElF): $(OBJECT_DIR)/boot_state.o
$(OBJECT_DIR)/boot_state.o: $(ET100_FW)/common/boot_state.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
$(BASE_ElF): $(addprefix $(OBJECT_DIR)/stub_,$(addsuffix .o,$(STUB_OBJECTS))) $(OBJECT_DIR)/sha256.o $(OBJECT_DIR)/platform_util.o
$(addprefix $(OBJECT_DIR)/stub_,$(addsuffix .o,$(STUB_OBJECTS))): $(OBJECT_DIR)/stub_%.o: $(ET100_FW)/installer/%.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
$(OBJECT_DIR)/sha256.o: $(TOP_DIR)/components/modules/mbedtls/library/sha256.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(OBJECT_DIR)/platform_util.o: $(TOP_DIR)/components/modules/mbedtls/library/platform_util.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(BASE_ElF): $(OBJECT_DIR)/nor_qspi1.o $(OBJECT_DIR)/ota_journal.o
$(OBJECT_DIR)/nor_qspi1.o $(OBJECT_DIR)/ota_journal.o: $(OBJECT_DIR)/%.o: $(ET100_FW)/src/%.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
