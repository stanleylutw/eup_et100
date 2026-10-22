# 15 — EVB 裝置資訊查詢（GET_FW_INFO／GET_DEVICE_ID）

日期：2026-10-22；分支 `v0.8.7_device_metadata`，base `6817f6c`。實作：Claude Code；待 Codex review。測試依 Stanley 要求精簡。

## 結果摘要（M-META PASS）

| # | 驗收（施工單 §3） | 結果 |
|---|---|---|
| 1 | Host：回應能被 FR3068 既有 client parser 解析 | ✅ request 由 `eup_aiot_uuid_client` 產生，回應的每個 part 經 `eup_uuid_parse_response` 檢查、串回後由 `eup_aiot_metadata_parse` 解出 project／hardware／firmware／soc／schema／protocol、device_id `C03232325903`、capabilities `0x23`。GET_FW_INFO 171 B：MTU 247 為 1 part、MTU 23 為 18 parts |
| 1b | 錯誤處理（照 gateway） | ✅ 壞 checksum／短封包／錯 magic → `0x02`；錯版本 → `0x05`；未知指令 → `0x01`；EUPFWV1 欄位填滿無結尾時不越界 |
| 2 | EVB：`GFWINFO` | ✅ `AT$PD=BLEGW$GFWINFO;1;EUP_ET100;ET100_EVB_DEV1.4;0.8.7-dev;fr3068e_c;M087` |
| 3 | EVB：BLE service 已註冊 | ✅ SWD 讀三個 service id：App manager 80、metadata 82、SMP 83（皆非 0xff）；`et100_meta_dropped`=0。手機 PING 為選做，未執行 |
| 4 | SMP 更新流程、B 資訊段 | ✅ 0.8.7 以 SMP serial 部署並自動確認；B-info `ad7facb2…` 不變 |

## Host 測試

`fw/tests/test_eup_meta.c`（已加入 `fw/tools/test_evb_serial_host.sh`）、`test_serial_gchk.c` 新增 GFWINFO 案例、`test_app_gatt_server.c` 加上 metadata service hook stub；全部 PASS（ASan／UBSan）。ARM build 無 warning（SDK 既有 RWX 除外）。

## 限制

- 未以手機實際送 PING／GET_FW_INFO；格式相容性以 FR3068 既有、已與真實 nRF sensor 互通的 client parser 驗證。
- 與 gateway 相同，回應分 part 時以 byte 切分（不保證不拆 TLV）；App 需串回全部 part 再解析。
