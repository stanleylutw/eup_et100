#ifndef ET100_FDB_KEYS_H
#define ET100_FDB_KEYS_H
/* Project FlashEnv user keys; distinct from SDK random seed and BT keys.
 * One key per record: values already on flash must never change or be reused. */
#define ET100_FDB_GTIME_KEY       0x00030002U /* serial_gchk.c GTIME, 8 bytes */
#define ET100_FDB_CID_KEY         0x00030003U /* serial_gchk.c CID, 20 bytes */
#define ET100_FDB_AUTO_LIST_KEY   0x00030004U /* gatt_client.c auto list, 100 bytes */
#define ET100_FDB_SENSOR_PKG_KEY  0x00030005U /* ota_worker.c sensor package record */
#define ET100_FDB_BOOT_HEALTH_KEY 0x00030006U /* boot_health.c TRIAL probe, 4 bytes */
#define ET100_FDB_REPORT_KEY      0x00030007U /* serial_gchk.c whitelist/MAXRPT, 102 bytes */
#endif
