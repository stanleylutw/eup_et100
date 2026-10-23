# REVIEW_REPORT

## v0.8.10 執行回報（未經獨立 review）

- 四個回報設定指令、102-byte FlashDB 記錄與回報過濾完成；CID／MAC OR、八欄、MAXRPT／GTIME 交互限制、寫入失敗、reload／corrupt codec、原子設定都有 host 覆蓋。
- HTML：分離 picker 記憶、ET-100 build root 下拉、中文分區／資訊段說明、A／B 等高及只縮放圖表。測試確認共用 Log 高度不變。
- 完整 host suite、ARM／DFU、SMP／UI mock 測試及 diff check PASS；只有既有 RWX linker warning。build log：`/tmp/et100_v0.8.10_build.log`；host log：`/tmp/et100_v0.8.10_host.log`。
- Stanley 的對話紀錄已證明 0.8.10 App OTA confirmed；GLIST／SLIST／MAXRPT 的 EVB 測試仍待 Stanley。Codex 未執行硬體操作。
- Claude 獨立 review 尚未完成；依 Stanley 明確指示先 commit／merge，再接 v0.8.11。不得將自測等同獨立 review。

---

先前紀錄：

v0.8.9 完整 review 保留於 commit `a387be9`：

```sh
git show a387be9:fw/REVIEW_REPORT.md
```

Host／ARM build／DFU 與 HTML mock 測試通過；實機 M-R1／M-R2 尚未驗收。後續結果記錄於 [17 測試報告](docs/04_testing/17_evb_sensor_ota_relay.md)，不得以合併到 main 取代硬體驗收。
