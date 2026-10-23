# IMPLEMENTATION_PLAN

## v0.8.10 — BLE 回報控制與 HTML

Branch：`v0.8.10_ble_report_controls`，base：`main`。Stanley 在聊天核准版本名、四個 BLE 指令與 HTML 修改；2026-10-23 核准補齊文件、commit、merge。

範圍：serial_gchk／ble_reporter、獨立 FlashDB key、evb_serial 版本、相應 host tests、HTML 與 UI tests、相關 serial／platform／status 文件。只移植 GLIST／SLIST、GMAXRPT／SMAXRPT 及既有 HTML 改進；不改 SDK、不燒錄。硬體分析七檔與 installer status DRAFT v2 不混入功能 commit。

驗收：完整 serial host suite、SMP／UI tests、ARM build／DFU、diff whitespace；實機由 Stanley 執行。成果見本任務 REVIEW_REPORT；獨立 review 待 Claude，不得宣稱通過。

---

先前紀錄：

v0.8.9 完整施工單保留於 commit `a387be9`：

```sh
git show a387be9:fw/IMPLEMENTATION_PLAN.md
```

硬體驗收 M-R1／M-R2 仍待 Stanley 執行，不因 merge 而視為通過。狀態與步驟見 [17 測試報告](docs/04_testing/17_evb_sensor_ota_relay.md)及 [relay 協定](docs/02_protocol/06_sensor_ota_relay.md)。
