# Run from the SDK example's GCC directory; SDK files remain unchanged.
ET100_FW := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
include Makefile

$(TOP_DIR)/examples/application/ble_simple_periphreal/Src/main.o: $(ET100_FW)/src/evb_serial_main.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $(OBJECT_DIR)/main.o $<

$(TOP_DIR)/examples/application/ble_simple_periphreal/Src/app_at.o: $(ET100_FW)/src/serial_gchk.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $(OBJECT_DIR)/app_at.o $<

$(TOP_DIR)/examples/application/ble_simple_periphreal/Src/app_ble.o: $(ET100_FW)/src/evb_serial_ble.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $(OBJECT_DIR)/app_ble.o $<

$(BASE_ElF): $(OBJECT_DIR)/ble_reporter.o
$(OBJECT_DIR)/ble_reporter.o: $(ET100_FW)/src/ble_reporter.c
	$(CC) $(CFLAGS) $(INC_PATH) -c -o $@ $<
