# DEV1.4 Gateway LED

日期：2026-10-21；Branch：v0.6.0_evb_gatt_client；韌體維持 0.6.0-dev／UART3 9600。

## 授權與範圍

Stanley 指定 D4 作 Gateway LED、D3 不使用。依提供的官方 EVB 接線，D4=PA7、D3=PA6，均 Active HIGH。新增 `src/status_led.c`／`.h` 與 `tests/test_status_led.c`；修改專案 `evb_serial_main.c` 初始化、`gatt_client.c`／`.h` 提供 link getter、`targets/evb_serial.mk` 加入 object。不修改 SDK、UART／GATT payload、FlashDB、時脈或按鍵，不改 PCB 原始文件。

## 行為

| 狀況 | D4（PA7） | D3（PA6） |
|---|---|---|
| App 啟動、待機／掃描 | 常亮 | 低電位熄滅 |
| 收到通過現行 filter 的 ADV | 每 200 ms 切換，最後一次資料後約 3 秒恢復常亮 | 熄滅 |
| Sensor central GATT link 存在 | 每 100 ms 切換，優先於資料指示 | 熄滅 |
| Link 已確認斷開 | 恢復常亮，新符合資料可再觸發閃爍 | 熄滅 |

沿用 Gateway `src/main.c` 的 100／200／3000 ms 視覺規則，移植到 EVB 的 sensor central link。不是 AP／AES 認證指示，也不代表 TCP／LTE 狀態；原 peripheral profile 的手機連線不在這個 getter 範圍。密集符合廣播會反覆重設活動起點，因此可能看似常亮，與 Gateway 每包重觸發的特性相同。

初始化先 preload PA6／PA7 low，再設 GPIO push-pull／no-pull，放在 main 的全 port pull 設定之後，避免被覆寫。獨立 LED task 每 20 ms 讀 accepted counter 與實際 link flag，stack 384 words（1.5 KiB，另有 TCB），使用既有 RTOS heap，不占 UART 輸出、不在 callback 延遲。實際閃爍有 scheduler jitter，不是精密計時保證。D3 保證限於 app 完成 GPIO 初始化後；ROM／reset 前期狀態不由 app 控制。sleep 仍為 EVB disabled 設定，未宣稱有 low-power restore 支援。

## 驗證

新增 LED ASan／UBSan tests 通過：idle、200 ms／3 秒邊界、活動重觸發、link 優先／斷線、32-bit 時間／counter wrap。既有 GATT／scan lifecycle／serial／reporter 四組 tests 通過，log `/tmp/et100_gateway_led_host_tests.log`。第一次測試將 2999 ms 邊界誤寫成低電位，依 200 ms 方波計算修正期望值後全數通過；未為配合錯誤期望改 production logic。

```bash
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DET100_LED_HOST_TEST -Ifw/src fw/src/status_led.c \
  fw/tests/test_status_led.c -o /tmp/et100_status_led_test
/tmp/et100_status_led_test
```

ARM build PASS；既有 RWX segment warning 保留。獨立 FR509x header／CRC／payload／FlashEnv 範圍檢查 PASS；SDK 新 patch 0。

| 產物 | 數據 |
|---|---|
| ELF text/data/bss | 239,912／35,288／66,304 bytes |
| Raw / burn | 275,208／283,400 bytes |
| CRC32 | 0xE5DBB566 |
| Burn SHA-256 | `bd48d786813baaea10538c223e3ff9fbc570fbd8f79dc8287eea81e5bc36556e` |
| Build log | `/tmp/et100_gateway_led_build.log` |
| Flash log | `/tmp/et100_gateway_led_flash.log` |

實際 D3/D4 電位、亮滅／閃爍、GATT 與 UART regression、RTOS heap／stack high-water 仍須 Stanley 實測。Host 模型不能證明實體 GPIO 線路或 LED 狀態。

EVB 燒錄已完成：70 sectors，38.1 秒；逐 byte readback PASS，SYSRESETREQ 後 resumed，PC=0x08005F70 位於 app flash。Codex 未操作 UART／HTML；未 commit／push。

## Stanley 驗收

1. App 啟動後 D3 應熄滅；D4 待機常亮。ROM 初期亮燈不作 app 驗收依據。
2. 讓符合 CID 的 sensor 廣播，觀察 D4 活動閃爍；停止資料約三秒後常亮。
3. 用既有 HTML 連 sensor，D4 應快閃；認證仍依 AS／AP／AF 判斷。
4. HTML Disconnect 收到 DC 後，D4 停止 GATT 快閃，Type 1 回報恢復。若 auto list 仍啟用，須排除自動重連。
5. 確認 GCHK、GTIME／CID、GATT 讀取與 UART 9600 未退化；本次不驗 K1-K3，未改 PMU pull。
