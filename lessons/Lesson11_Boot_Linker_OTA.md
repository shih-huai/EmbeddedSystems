# ESP32 韌體與驅動面試速成：第 11 課

## 第 11 課：開機流程、linker、分割表、OTA

一句話：從按下 reset 到 `setup()` 之間，ROM、bootloader 和啟動程式碼把映像檔的各個 segment 搬到正確的記憶體、準備好 C 執行環境；OTA 就是把新映像檔寫到另一個分割區，再告訴 bootloader 下次從那裡開機。

### 概念

**ESP32 開機流程**：

| 階段 | 做什麼 |
| --- | --- |
| 1. ROM bootloader | 晶片內建、不能改。看 strapping 腳（GPIO0）決定進下載模式還是從 flash 開機，從 flash `0x1000` 載入第二階段 bootloader |
| 2. 第二階段 bootloader | 讀 `0x8000` 的分割表和 otadata，決定要開哪個 app 分割區，驗證映像檔，把 IRAM／DRAM segment 複製到內部 RAM，設定 MMU 把 flash 中的程式和常數對應到 `0x400D…`、`0x3F4…`，跳到進入點 |
| 3. 啟動程式碼（`call_start_cpu0`） | 清 `.bss`、初始化 heap、時脈、啟動另一顆核心、執行 constructor 和 C++ 全域物件建構子 |
| 4. FreeRTOS 和 `app_main` | 啟動排程器、建立 main task；Arduino 再建立 `loopTask` 執行 `setup()` 和 `loop()` |

**Linker 把東西放在哪**：

| Section | 內容 | ESP32 位置 |
| --- | --- | --- |
| `.text` | 程式 | flash（IROM，經快取執行） |
| `.iram1` | `IRAM_ATTR` 函式、ISR、flash 驅動本身 | 內部 IRAM |
| `.rodata` | 常數、字串 | flash（DROM） |
| `.data` | 有初始值的全域／static 變數 | DRAM；初始值存在映像檔裡 |
| `.bss` | 沒有初始值的全域／static 變數 | DRAM；開機時清成 0 |
| `.rtc.data`／`.rtc_noinit` | `RTC_DATA_ATTR`／`RTC_NOINIT_ATTR` | RTC 記憶體，深度睡眠時不斷電 |

一般 MCU（例如 STM32）的映像檔直接燒在 flash，啟動程式碼要自己把 `.data` 從 flash 複製到 RAM。ESP32 的映像檔是一個個 segment，`.data` 的初始值是由 bootloader 直接載入 DRAM 的。linker script 也定義了 `_data_start`、`_bss_start` 這類符號，讓程式知道各區段的邊界。

**分割表**：放在 flash `0x8000`，描述 flash 怎麼切。Arduino 預設有 `nvs`（設定值）、`otadata`（下次從哪個 app 開機）、`app0`／`app1`（兩個 OTA 分割區）、`spiffs`（檔案系統）、`coredump`。

**OTA 流程**：`esp_ota_get_next_update_partition` 找到「不是正在執行的那個」分割區，`esp_ota_begin` 抹除，`esp_ota_write` 一段段寫入，`esp_ota_end` 驗證整個映像檔（checksum 和 SHA-256），通過後 `esp_ota_set_boot_partition` 寫 otadata，重開機。開啟 rollback 時，新韌體開機後要自我檢查並呼叫 `esp_ota_mark_app_valid_cancel_rollback()`，否則下次開機 bootloader 會退回舊版。正在執行的分割區永遠不會被覆寫，所以寫到一半斷電也不會變磚。

### Wokwi 練習 11：記憶體配置、重開機、自己做 OTA

程式專案是 `Day11_Boot_Linker_OTA`，不需要外接元件。映像檔解析放在 `lib/app_image`，附 7 個單元測試，裝置上用同一份程式解析正在執行的韌體。

| EXERCISE | 內容 |
| --- | --- |
| 0 | 印出各種變數和函式的位址，再解析正在執行的映像檔，列出每個 segment 載入到哪裡 |
| 1 | 印分割表和韌體資訊，軟體重開機一次、panic 一次，比較三種變數的計數 |
| 2 | 把自己的韌體複製到另一個 OTA 分割區：先寫壞一個 bit 看驗證擋下，再寫正確版本、切換開機分割區、重開機 |
| 3 | constructor、C++ 全域物件在 `setup()` 之前執行；`setup()` 跑在哪個 task |

我在 ESP32 模擬器（QEMU）上跑過四個練習：

| 練習 | 結果 |
| --- | --- |
| 0 | 常數在 `0x3F4…`、一般函式在 `0x400D…`、IRAM 函式在 `0x4008…`、全域變數在 `0x3FFB…`、RTC 變數在 `0x5000…`。映像檔 6 個 segment，兩個 DRAM segment 共 16,552 bytes，剛好等於 `.data` 大小 |
| 1 | reset 原因依序 POWERON、SW、PANIC；RTC_NOINIT 計數 1→2→3，RTC_DATA 和一般全域每次都是 1 |
| 2 | 壞 1 bit 的版本：`Checksum failed`、`ESP_ERR_OTA_VALIDATE_FAILED`，開機分割區不變；正確版本寫入後從 app1 開機，再切回 app0 |
| 3 | constructor 和全域物件都在 `setup()` 前執行；`setup()` 在 `loopTask`、核心 1、優先權 1 |

```c
const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
esp_ota_handle_t h;
esp_ota_begin(next, image_len, &h);          // 抹除需要的區塊
while (有資料) esp_ota_write(h, buf, n);     // 從網路或 UART 收一段寫一段
if (esp_ota_end(h) == ESP_OK) {              // 驗證 checksum 和 SHA-256
  esp_ota_set_boot_partition(next);          // 寫 otadata
  esp_restart();
}
```

### 面試題與參考答案

**Q1. 從上電到 `main`（或 `app_main`）之間發生了什麼？**
ROM bootloader 依 strapping 腳決定開機模式，從 flash 載入第二階段 bootloader。bootloader 讀分割表和 otadata，選定 app 分割區並驗證，把 IRAM、DRAM segment 複製到內部 RAM，設定 MMU 讓 flash 中的程式和常數可以經快取存取，跳到進入點。啟動程式碼清 `.bss`、初始化時脈和 heap、執行 constructor，然後啟動 RTOS 呼叫 `app_main`。一般 MCU 則是 reset vector 指向 `Reset_Handler`，由它複製 `.data`、清 `.bss`、呼叫 `SystemInit` 和 `main`。

**Q2. `.data`、`.bss`、`.rodata` 差在哪？為什麼要分開？**
`.data` 是有初始值的全域變數，需要在 flash 存初始值、在 RAM 佔空間；`.bss` 是初值為 0 的變數，只需在開機時清 0，不佔映像檔空間；`.rodata` 是常數，可以留在 flash 不佔 RAM。分開才能讓映像檔小、RAM 用量少。把大表格宣告成 `const` 就能從 RAM 移到 flash。

**Q3. 為什麼 ISR 要加 `IRAM_ATTR`？**
寫 flash 時快取會被關閉，這段期間在 flash 中的程式無法執行。如果此時中斷發生而 ISR 在 flash，就會當機（Cache disabled but cached memory region accessed）。放在 IRAM 的 ISR 和它用到的資料（放 DRAM）不受影響。

**Q4. 怎麼設計一個不會變磚的 OTA？**
雙分割區（A/B）：永遠寫到沒在執行的分割區；寫完驗證完整性（hash）和真實性（簽章，secure boot）；驗證通過才切換開機分割區；新韌體開機後自我檢查，成功才標記有效，否則 bootloader 自動退回舊版。另外要考慮版本防降級（anti-rollback）、斷電時的狀態，以及保留一個 factory 分割區當最後手段。

**Q5. linker script 是做什麼的？你改過什麼？**
它告訴 linker 有哪些記憶體區域（起點、大小、權限），每個 section 要放進哪個區域，以及定義 `_bss_start` 這類符號給啟動程式碼用。常見的修改：把某些函式放到 RAM 加速、劃出一塊不初始化的 RAM 保存重開機前的錯誤資訊、對齊 DMA 緩衝區、在固定位址放版本資訊。

**Q6. 怎麼保存「當機前發生了什麼」？**
用不初始化的 RAM 區段（ESP32 的 `RTC_NOINIT_ATTR`，或自訂 `.noinit` section）存錯誤碼和計數，加上 magic number 判斷是否有效；開機時讀 reset 原因（`esp_reset_reason`）；ESP32 還可以把 core dump 寫到 flash 的 coredump 分割區，事後用工具還原 backtrace。

**Q7. RTC_DATA_ATTR 和 RTC_NOINIT_ATTR 差在哪？**
兩者都在 RTC 記憶體，深度睡眠時都保留。`RTC_DATA_ATTR` 有初始值，除了深度睡眠喚醒以外的 reset，bootloader 會重新載入初值；`RTC_NOINIT_ATTR` 不會被初始化，連軟體重開機和 panic 都能保留，但上電後內容是亂的，要用 magic number 檢查。
