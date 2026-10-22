# TinyCrypt AES 子集來源

2026-10-21 從本機 NCS v2.9.3 的 `modules/crypto/tinycrypt/lib/` 複製：

- `source/aes_encrypt.c`、`source/utils.c`
- `include/tinycrypt/aes.h`、`utils.h`、`constants.h`

僅使用 AES-128 encrypt／key schedule；未自行撰寫 AES。各檔原始 copyright 與授權條款完整保留在檔頭。未修改 NCS 或 Freqchip SDK。

GATT auth helper、UUID codec 與 metadata parser 來自 sibling Gateway 的 `src/service/eup_aiot_*`。Auth helper 保持逐 byte 相同；不得將認證材料或 production-key 測試向量寫入 chat／log。測試只使用公開 NIST AES 向量或輸出 PASS／FAIL。實際 Sensor AES 認證須由 Stanley 透過 Type 8 AP／AF 驗證。
