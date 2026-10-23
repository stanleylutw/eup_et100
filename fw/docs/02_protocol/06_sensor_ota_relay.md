# 06 — Sensor OTA relay（v0.8.9）

日期：2026-10-23；分支 `v0.8.9_sensor_ota_relay`。目前 host／ARM build 通過，M-R1／M-R2 待 Stanley 實測，不宣稱量產可用。

## 範圍

ET-100 從 NOR 的 VALID Sensor package 讀取映像，以既有 central owner 連接指定 MAC，執行 AES、metadata 相容性檢查及 BLE MCUmgr SMP 更新。RS232 ASCII 在 9600 8N1 使用；套件預先以 [SMP image 1](04_smp_serial_ota.md) 儲存。

wire format、state／step／error 名稱及數值沿用 `eup_gateway/docs/02_protocol/03_rs232_protocol.md` §9.9；不提供 `SOTAPROBE` 或 `SOTASTAGE`，不從 App BLE 入口啟動 relay。

## 指令

每行加 CRLF；`<tok>` 為 4 碼 token，MAC 為 12 碼 HEX，不含冒號。

| 指令 | 成功回覆（皆以 `AT$PD=BLEGW$` 開頭） |
|---|---|
| `SENSOROTA;<mac>;<tok>` | `SENSOROTA;1;<mac>;<generation>;<image_length>;<tok>`，只代表已接受，非更新完成 |
| `GSENSOROTA;<tok>` | `GSENSOROTA;1;<state>;<mac>;<generation>;<bytes>;<image_length>;<elapsed_ms>;<error>;<error_name>;<tok>` |
| `GSENSOROTAFLOW;<tok>` | `GSENSOROTAFLOW;1;<state>;<step>;<result>;<failed_step>;<match_mask_8hex>;<step_results_12hex>;<tok>`（**已知問題**：0.8.9–0.8.11 韌體因 newlib-nano 不支援 `%llX`，`step_results` 欄位輸出錯誤且 token 遺失；0.8.12 起修正，格式不變） |
| `GSENSOROTAMETA;<tok>` | `GSENSOROTAMETA;1;<mac>;<project>;<hardware>;<firmware>;<build_id>;<soc>;<board>;<device_id>;<capabilities_8hex>;<role_2hex>;<mismatch>;<tok>`；尚未取得時 `0;NOT_READY;<tok>` |
| `CSENSOROTA;<tok>` | `CSENSOROTA;1;OK;<tok>`，等待狀態變成 CANCELLED 才算完成；無 job 時 `0;NOT_ACTIVE;<tok>` |

開始失敗回 `SENSOROTA;0;<error>;<error_name>;<tok>`。查詢／取消不帶其他參數。`GSENSOROTAMETA` 只讀 job 快照，不會自行連線或啟動唯讀探測。

## 流程與互斥

1. VALID 套件與 exact MAC → 連線 → AES → metadata → project／hardware／SoC／board／device ID／capabilities／schema／protocol／role 檢查。
2. SMP discovery → image state。只有 firmware 與 active MCUboot hash 都相同才回 `ALREADY_UP_TO_DATE`／SUCCESS，且不 erase／upload／reset。
3. 否則 erase slot 1 → upload（192 B chunk）→ 比對 inactive hash → test（confirm=false）→ reset → 重連與 AES／metadata → 比對 firmware 與 active hash → SUCCESS。
4. 最多 3 個 upload session；斷線後同一 job 可依 Sensor offset 續傳。ET-100 重開機即回 IDLE，不續傳 RAM job。
5. relay 期間 image 0／1 上傳、package 刪除、手動 GATT session 與 auto-connect 受互斥保護。查詢不受限；只有 OTA worker 讀 NOR。

`GSENSORINFO` 屬手動 GATT 介面，即使只讀快取，在 relay reservation（含兩個 session 之間）仍回 `AT$PD=BLEGW$GSENSORINFO;0;BUSY;<tok>`；relay 自己讀取 metadata 不受影響。`GSOTAPKG`／`GSENSOROTA*` 查詢照常。reservation 釋放後恢復一般快取查詢。

完整 AES／metadata GATT 連線要求 MTU 至少 35，目前請求 MTU 247。Host 的 MTU 23 案例只測 SMP 分段與重組，不代表整套認證流程可在 MTU 23 運作。

Type 8 格式 `AT$PD=G1;8;OP;<mac>;SOTA;…*<xor>`，包含 STEP、SCAN_PAUSED、ERASE_DONE、LINK_MTU、ALREADY_UP_TO_DATE 及 terminal state。UART queue 滿時事件可能延後或丟棄，查詢快照為準。

## Stanley 手動驗收

HTML 接線已完成：使用 `tools/rs232_tester.html`，連線後以 GCHK 判別 ET-100；`0.8.9` 以上啟用 **OTA → Device → GATT OTA**，舊版保持停用。這只是 UI 版本 gate，還要 GSOTAPKG 回 VALID、GSENSOROTA 確認介面可用，才顯示可開始的 job。

1. **OTA → ET-100**：載入本分支的 `dfu_application.zip` 更新主機，等待自動確認。
2. **OTA → Device → RS232 OTA**：選擇 Sensor DFU ZIP，儲存到 ET-100，讀 NOR 確認 VALID 與欄位比對。
3. **GATT OTA**：自動重新查詢 NOR 套件與 job 狀態，輸入 exact MAC，再按開始。沿用 Gateway 15 步畫面及開始／取消／完整狀態查詢。
4. 主圖來源為 **ET-100 NOR 16 MB（EVB）**，Sensor 套件位於 0x420000／256 KB；右側為 Sensor Flash，不能當成內部 App 更新。
5. 不呼叫 ET-100 未實作的 SOTAPROBE／SOTASTAGE／ZVERO；切回 RS232 OTA 保留已選 Sensor 檔案。

以上 UI 僅以模擬 Serial 驗證，真實 Sensor 結果仍由 Stanley 驗收。

部署前保存 `GAUTOLIST`，使用本分支 `dfu_application.zip` 更新 ET-100 並等待確認，再檢查 `GCHK` 為 0.8.9-dev。不要用 App 的 Project.bin 直接燒錄，也不要寫 `0x08001000`。

```text
BLEGW$GSOTAPKG;P001
BLEGW$SENSOROTA;<P1_MAC>;R001
BLEGW$GSENSOROTA;R002
BLEGW$GSENSOROTAFLOW;R003
BLEGW$GSENSOROTAMETA;R004
BLEGW$CSENSOROTA;R005
BLEGW$GAUTOLIST;R006
```

M-R1 要先放入與該 Sensor **實際執行版本及 active hash 均一致**的已知 package，預期 ALREADY_UP_TO_DATE。僅名稱／版本一樣不足以保證唯讀；若 hash 不同，`SENSOROTA` 會進入擦除／更新。沒有可靠相同映像時，不要把這個指令當成唯讀探測工具。

M-R2 改放另一版本 package，檢查 SUCCESS、版本／active hash 改變，再測重跑、途中斷線續傳與取消。比較 auto 清單及 B-info 前後 SHA-256；Codex 本輪未替使用者執行這些硬體步驟。
