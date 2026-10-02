# vendor/ — 第三方 SDK

本目錄存放外部提供的 SDK / library，**不進 git**（.gitignore 已排除 `fr30xxc_sdk*/`）。

## 自動下載

```bash
./tools/fetch_sdk.sh
```

成功後會建立 `vendor/fr30xxc_sdk__202411/` 子目錄（約 165 MB，含 drivers / BLE stack / peripheral drivers / Keil + GCC build 範例）。

## 來源

Freqchip (富芮坤) FR3068E-C SDK，透過社群 bundled 版本取得：
- URL：https://gitee.com/qinyunti/fr3068-e-c-micropython
- SDK 版本：`fr30xxc_sdk__202411`（2024 年 11 月發布）
- 原廠 SDK 下載頁（空頁 / 需註冊）：https://www.freqchip.com/sjds

**TODO**：待 Quectel / Freqchip 回覆 Q39（正式 BSP 授權管道），改用正式發布版本。

## 目錄結構（SDK 內部）

```text
fr30xxc_sdk__202411/
├── components/
│   ├── btdm/              # Bluetooth 5.3 stack（binary archive：libbtdm_host.a 317 .o）
│   ├── drivers/
│   │   ├── bsp/           # Board support
│   │   ├── cmsis/         # ARM CMSIS v5.8（支援 GCC/armclang/IAR/ARMCC）
│   │   ├── device/fr30xx/
│   │   │   ├── gcc/       # ⭐ GCC startup/syscalls/sysmem
│   │   │   ├── armcc/     # Keil ARM Compiler 版本
│   │   │   └── iar/       # IAR 版本
│   │   └── peripheral/    # GPIO/I2C/SPI/UART/CAN/PWM/RTC/ADC/CODEC driver
│   ├── modules/           # FatFS / LittleFS / FlashDB / mbedTLS / FreeRTOS / lwIP / lvgl / coremark
│   └── tools/
│       ├── gcc/           # ⭐ ldscript_3068e.ld（GNU ld 標準）
│       └── keil/          # FLM 燒錄算法 + .sct scatter file
├── examples/
│   ├── application/btdm/GCC/Makefile              # ⭐ BT 範例 GCC 可 build
│   ├── application/ble_simple_periphreal/GCC/Makefile  # ⭐ BLE 範例 GCC 可 build
│   └── evb_demo/lvgl_demo/GCC/Makefile            # ⭐ LVGL 範例 GCC 可 build
└── 文檔 pdf / changelog
```

## GCC Support 證據（已驗證 2026-10-02）

| 項目 | 位置 | 狀態 |
|---|---|---|
| GCC linker script | `components/tools/gcc/ldscript_3068e.ld` | ✅ 178 行完整 GNU ld 語法 |
| GCC startup | `components/drivers/device/fr30xx/gcc/startup_fr30xx.S` | ✅ `.syntax unified / .cpu cortex-m33` |
| newlib stub | `syscalls.c` + `sysmem.c` | ✅ _sbrk / _write 等 |
| BLE 可 link | `libbtdm_host.a` = GNU ar archive（317 .o）| ✅ `-lbtdm_host` 可用 |
| GCC Makefile 範例 | `examples/*/GCC/Makefile` | ✅ 3 個 |
| CMSIS GCC header | `components/drivers/cmsis/cmsis_gcc.h` | ✅ |

## 已知缺的（Quectel/Freqchip 要補）

- **SVD 檔** — 無，debugger peripheral view 要靠 `fr30xx.h` 的 struct define 自己對（Q47）
- **CMakeLists.txt** — 無 top-level CMake，只有 Makefile；本專案自寫 CMake wrapper
- **Documentation 中英對照** — SDK 文件主要中文

## 更新 SDK

重跑 `tools/fetch_sdk.sh`，會比對 `.sdk_version` 檔案，若有新版會提示。
