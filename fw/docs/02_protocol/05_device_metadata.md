# 05 — 裝置資訊查詢：GET_FW_INFO／GET_DEVICE_ID（v0.8.7）

日期：2026-10-22；分支 `v0.8.7_device_metadata`；實作：Claude Code。
對象：自家 App（BLE 相容性檢查）與 `tools/rs232_tester.html`（RS232）。

ET-100 以「被更新的裝置」身分回答相容性查詢，行為與 eup_gateway（nRF52840）**相同**：規格 `eup_gateway/docs/protocol_cache/02_eup_aiot_uuid_communication_protocol.md` §3、§5–§11；參考實作 `eup_gateway/src/service/eup_ota_metadata_service.c`。

## 1. BLE：EUP AIoT Communication Service

| 項目 | 值 |
|---|---|
| Service | `E9E00000-4F4C-4F47-4943-455550000001`（**所有 build 都有**，不受 `ET100_SMP_BLE_OPEN` 控制） |
| RX | `E9E00001-…`：Write／Write Without Response |
| TX | `E9E00002-…`：Notify（先開 CCCD） |
| 驗證 | **不需要**（4 個指令皆唯讀；寫入用的 SMP 另有安全 gate，見 06 §6.1） |

Request：`E9 55 01 cmd seq len payload sum8`。Response：`E9 55 01 cmd|0x80 seq status part total len payload sum8`，`sum8` 為前面所有 byte 加總 & 0xFF。回應超過一個 notify 時分 part（每 part payload ≤ MTU−13，最多 240 B），App 依 `part`／`total` 串回再解析 TLV。

| 指令 | 回應 TLV |
|---|---|
| `0x01 GET_FW_INFO` | 0x01 project `EUP_ET100`、0x02 hardware `ET100_EVB_DEV1.4`、0x03 firmware、0x04 build_time、0x05 build_id、0x06 git_commit、0x07 git_branch、0x08 ble_name_normal、0x09 ble_name_ota、0x0A release_note、0x0B metadata_schema `1`、**0x0C soc `fr3068e_c`**、0x0D protocol_version `1` |
| `0x05 GET_DEVICE_ID` | 0x01 mac_string（BLE identity，例 `C0:32:32:32:59:03`，與 GCHK identity 同源） |
| `0x07 GET_CAPABILITIES` | 0x01 flags（FW_INFO＋OTA_READY＋DEVICE_ID = `0x00000023`）、0x02 max_rx 244、0x03 max_tx 240、0x04 preferred_mtu（目前 MTU） |
| `0x7F PING` | 無 payload |
| 其他 | status `0x01 UNKNOWN_COMMAND`；格式錯 `0x02`；版本錯 `0x05` |

範例（PING）：送 `E9 55 01 7F 01 00 BF` → 收 `E9 55 01 FF 01 00 00 01 00 40`。

字串取自 App image 內的 EUPFWV1 block，與 `dfu_application.zip` 內 image 的 EUPFWV1 是同一份格式，App 可直接比對「裝置」與「檔案」的 project／hardware_version。

## 2. RS232：`BLEGW$GFWINFO`

9600 下即可查詢（不必進 SMP）：

```text
Host  → BLEGW$GFWINFO;<token4>\r\n
ET100 → AT$PD=BLEGW$GFWINFO;1;<project>;<hardware_version>;<firmware_version>;<soc>;<token4>\r\n
例      AT$PD=BLEGW$GFWINFO;1;EUP_ET100;ET100_EVB_DEV1.4;0.8.7-dev;fr3068e_c;M087
```

HTML 工具更新前應以此比對：裝置 `project`／`hardware_version`／`soc` 與檔案（manifest board／soc、EUPFWV1）一致才允許更新。

## 3. App 端相容性流程（06 §6.1）

```text
連線 → GET_FW_INFO／GET_DEVICE_ID（本服務）→ 依裝置 soc 判斷平台（fr3068e_c = FR3068）
→ 檢查 zip manifest（board／soc）與 image EUPFWV1（project／hardware）三方一致、CRC 非 0 且正確
→ SMP（Nordic MCUmgr service，見 04）上傳 → Test → Reset → Confirm
```
