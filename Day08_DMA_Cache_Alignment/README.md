# Day 08：DMA、快取、對齊（VS Code + Wokwi）

用 ESP-IDF 的 `spi_master` 驅動和 DMA 畫螢幕，比較「CPU 等傳輸」和「DMA 雙緩衝」的差別；再用小實驗看懂記憶體對齊、struct padding，以及哪些記憶體可以給 DMA 用。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day08_DMA_Cache_Alignment` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路
和 Day 07 相同：ILI9341 的 SCK=GPIO18、MOSI=GPIO23、MISO=GPIO19、CS=GPIO17、D/C=GPIO16、RST=GPIO4，VCC 與 LED 接 3.3V。

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | struct 的對齊與 padding：同樣的成員，排法不同 sizeof 就不同；packed struct |
| 1 | 用 memcpy 安全讀未對齊的資料；指標強制轉型讀 DRAM 未對齊位址（ESP32 剛好能動，換晶片就不行）；用位元組讀 IRAM，CPU 丟出 LoadStoreError 例外並重開機 |
| 2 | 畫漸層畫面：「算一塊、等它送完」vs「DMA 雙緩衝邊算邊送」，印出每張畫面的時間（參考解） |
| 3 | 檢查堆疊、全域、malloc、`heap_caps_malloc(MALLOC_CAP_DMA)`、flash 常數，哪些能給 DMA 用（參考解） |

練習 1 的 C 部分要在實體板上才看得到例外：Wokwi 和 QEMU 等模擬器不一定模擬 IRAM 只能 32 位元存取的限制。練習 2 的時間也請以實體板為準。

## 程式庫
| 資料夾 | 內容 |
| --- | --- |
| `lib/dma_plan` | 把一次大傳輸切成多段 DMA descriptor，每段不超過上限、長度對齊 |
| `lib/ili9341` | Day 07 的螢幕驅動，多了 `ili9341_begin_ramwr()`，讓呼叫端自己用 DMA 送像素 |

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或：`gcc -I<unity>/src -Ilib/dma_plan <unity>/src/unity.c lib/dma_plan/dma_plan.c test/test_dma_plan/test_main.c -o test_dma_plan`

## 版本注意
`soc/soc_memory_types.h` 是 ESP-IDF 4.4（Arduino-ESP32 2.0.x）的標頭檔；ESP-IDF 5.x 改成 `esp_memory_utils.h`。
