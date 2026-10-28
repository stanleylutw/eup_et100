# Run from the SDK example's GCC directory; SDK files remain unchanged.
ET100_FW := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
override DIR_OBJS = $(filter-out %/ota.o %/service_ota.o,$(DIR_OBJS_S))
include Makefile
LD := $(ET100_FW)/ld/evb_app.ld
LDFLAGS += -L$(ET100_FW)/installer
INC_PATH := -I"$(ET100_FW)/include" $(INC_PATH)
INC_PATH += -I"$(ET100_FW)/third_party/tinycrypt/include"
ET100_VERSION_FLAGS ?= -DEUP_FIRMWARE_VERSION=\"0.9.7-dev\"
# DEVELOPMENT builds open an unauthenticated BLE SMP service (06 §6.1); clear for release.
ET100_SMP_BLE_FLAGS ?= -DET100_SMP_BLE_OPEN=1
CFLAGS += $(ET100_VERSION_FLAGS) $(ET100_TEST_FLAGS) $(ET100_SMP_BLE_FLAGS)
INC_PATH += -I"$(ET100_FW)/common" -I"$(ET100_FW)/installer" -I"$(TOP_DIR)/components/modules/mbedtls/include"
$(BASE_ElF): $(OBJECT_DIR)/boot_health.o $(OBJECT_DIR)/boot_state.o $(OBJECT_DIR)/mcuboot_image.o $(OBJECT_DIR)/sha256.o $(OBJECT_DIR)/platform_util.o
$(OBJECT_DIR)/boot_health.o: $(ET100_FW)/src/boot_health.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
$(OBJECT_DIR)/boot_state.o: $(ET100_FW)/common/boot_state.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(BASE_ElF): $(OBJECT_DIR)/inst_status.o
$(OBJECT_DIR)/inst_status.o: $(ET100_FW)/common/inst_status.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
$(OBJECT_DIR)/mcuboot_image.o: $(ET100_FW)/installer/mcuboot_image.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(OBJECT_DIR)/sha256.o: $(TOP_DIR)/components/modules/mbedtls/library/sha256.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(OBJECT_DIR)/platform_util.o: $(TOP_DIR)/components/modules/mbedtls/library/platform_util.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(BASE_ElF): $(OBJECT_DIR)/app_fw_info.o
$(OBJECT_DIR)/app_fw_info.o: $(ET100_FW)/installer/app_fw_info.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<

$(TOP_DIR)/examples/application/ble_simple_periphreal/Src/main.o: $(ET100_FW)/src/evb_serial_main.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $(OBJECT_DIR)/main.o $<

$(TOP_DIR)/examples/application/ble_simple_periphreal/Src/app_at.o: $(ET100_FW)/src/serial_gchk.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $(OBJECT_DIR)/app_at.o $<

$(TOP_DIR)/examples/application/ble_simple_periphreal/Src/app_ble.o: $(ET100_FW)/src/evb_serial_ble.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $(OBJECT_DIR)/app_ble.o $<

$(BASE_ElF): $(OBJECT_DIR)/ble_reporter.o
$(OBJECT_DIR)/ble_reporter.o: $(ET100_FW)/src/ble_reporter.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<

GATT_OBJECTS = gatt_client eup_aiot_uuid_client eup_aiot_metadata eup_aiot_auth status_led app_manager app_gatt_server
$(BASE_ElF): $(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(GATT_OBJECTS))) $(OBJECT_DIR)/tinycrypt_aes.o $(OBJECT_DIR)/tinycrypt_utils.o
$(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(GATT_OBJECTS))): $(OBJECT_DIR)/%.o: $(ET100_FW)/src/%.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(OBJECT_DIR)/tinycrypt_aes.o: $(ET100_FW)/third_party/tinycrypt/source/aes_encrypt.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
$(OBJECT_DIR)/tinycrypt_utils.o: $(ET100_FW)/third_party/tinycrypt/source/utils.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<

# v0.8.5: OTA manager + SMP over serial (UART3).
OTA_OBJECTS = system_status cbor_min smp_serial smp_server ota_manager ota_journal nor_qspi1 ota_worker smp_ble eup_meta_protocol eup_meta_service sensor_package sensor_store \
              sensor_smp_client sensor_ota_compat sensor_ota_job can_task
$(BASE_ElF): $(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(OTA_OBJECTS))) $(OBJECT_DIR)/candidate.o
$(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(OTA_OBJECTS))): $(OBJECT_DIR)/%.o: $(ET100_FW)/src/%.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
$(OBJECT_DIR)/candidate.o: $(ET100_FW)/installer/candidate.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<

INC_PATH += -I"$(ET100_FW)/src/can"
CAN_OBJECTS = j1939 app_can_protocol app_can_bitrate app_gateway
$(BASE_ElF): $(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(CAN_OBJECTS)))
$(addprefix $(OBJECT_DIR)/,$(addsuffix .o,$(CAN_OBJECTS))): $(OBJECT_DIR)/%.o: $(ET100_FW)/src/can/%.c
	$(CC) $(CFLAGS) $(INC_PATH) -Wall -Wextra -c -o $@ $<
$(BASE_ElF): $(OBJECT_DIR)/driver_can.o
$(OBJECT_DIR)/driver_can.o: $(TOP_DIR)/components/drivers/peripheral/Src/driver_can.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
