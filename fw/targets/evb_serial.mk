# Run from the SDK example's GCC directory; SDK files remain unchanged.
ET100_FW := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
override DIR_OBJS = $(filter-out %/ota.o %/service_ota.o,$(DIR_OBJS_S))
include Makefile
LD := $(ET100_FW)/ld/evb_app.ld
LDFLAGS += -L$(ET100_FW)/installer
INC_PATH := -I"$(ET100_FW)/include" $(INC_PATH)
INC_PATH += -I"$(ET100_FW)/third_party/tinycrypt/include"
CFLAGS += $(ET100_VERSION_FLAGS)
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
