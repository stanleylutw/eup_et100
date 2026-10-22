# FR3068 EVB Tools

這些工具供 FR3068EP-C DEV1.4 EVB bring-up 使用。預設 SDK、FLM 與 image 路徑都由 script 所在位置反推 repo root，不依賴 `/Users/<name>/...` 絕對路徑。

## 需求

```bash
python3 -m pip install --user pyserial pyocd
```

從 repo root 執行 `python3 fw/tools/evb/<script>.py --help` 可查看完整參數。板卡 jumper、image header 與救援流程見 [`docs/03_hardware/FR3068EP-C Dev1.4/README.md`](../../../docs/03_hardware/FR3068EP-C%20Dev1.4/README.md)。

## 工具

| Script | 寫入硬體 | 用途 |
|---|:-:|---|
| `uart_listen.py` | 否 | 限時擷取 DAPLink VCOM UART |
| `swd_probe.py` | 否 | attach-only，讀 DP/AP、CPUID 與少量 memory |
| `swd_retry.py` | 否 | 最多 N 次 attach-only，判斷 SWD 是否持續 WAIT/失聯 |
| `flash_size_probe.py` | 否 | attach-only，比對 base 與 +1/2/4/8 MiB 內容，以 address alias 判斷內建 Flash 容量 |
| `flash_sector_test.py` | **是** | 上半 1 MiB 單一空白 4 KiB sector：program → readback → 抹回 0xFF → reset；非空白即中止 |
| `fr3068_flash.py` | **是** | 正常 sector program、readback verify、清 DEMCR、reset |
| `rescue_and_flash.py` | **是** | 手動 RST release 後抓 halt，同一 process 內救援燒錄 |

## 安全規則

1. 未取得 Stanley 明確同意，不執行 `fr3068_flash.py`、`rescue_and_flash.py` 或 `flash_sector_test.py`。
2. 燒錄工具只接受已驗證的 FR509x header layout：word 7/11 為 `0x51525251`、word 8 為 `0`。
3. 預設 image 是 SDK example 的 `Project_burn.bin`，預設寫入 `0x08000000`。
4. 不把 `Project.bin` raw image 當成 `Project_burn.bin` 使用。
5. 救援腳本要求只有一個 debug probe，且執行期間不得由另一個 pyOCD process 同時連線。
6. `flash_sector_test.py` 預設 0x081F7000 在[新 NOR installer 候選 map](../../docs/01_firmware/06_ota_storage_design.md) 中屬 Settings 區；不是永久 scratch。新 map 部署後不可直接沿用預設，須另核准測試區；工具只檢查空白／地址範圍，不能代替分區所有權確認。

內建 Flash 容量的既有證據見 [08 實測報告](../../docs/04_testing/08_evb_internal_flash_size.md)。本輪只恢復工具／報告及更新文件，未執行 probe／sector test，不構成新硬體測試授權。

## 範例

```bash
# UART ROM banner
python3 fw/tools/evb/uart_listen.py /dev/cu.usbmodem2024_2425_62851 115200 20

# 唯讀 SWD
python3 fw/tools/evb/swd_probe.py
python3 fw/tools/evb/swd_retry.py --attempts 5 --delay 1.5

# 內建 Flash 容量（唯讀）
python3 fw/tools/evb/flash_size_probe.py

# 上半 1 MiB 寫入測試（需先取得 Stanley 同意；預設 0x081F7000）
python3 fw/tools/evb/flash_sector_test.py --address 0x081F7000

# 正常燒錄（需先取得 Stanley 同意）
python3 fw/tools/evb/fr3068_flash.py

# 壞 image 救援（需先取得 Stanley 同意）
python3 fw/tools/evb/rescue_and_flash.py --timeout 30
```

覆寫預設 image/FLM：

```bash
python3 fw/tools/evb/fr3068_flash.py \
  --image /path/to/Project_burn.bin \
  --flm /path/to/FR30xx.FLM
```

`--image` 與 `--flm` 接受相對目前 shell working directory 或絕對路徑；工具內部會先 resolve、檢查檔案存在，再載入 pyOCD。
