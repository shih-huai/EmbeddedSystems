# Day 07：SPI 與 ILI9341 螢幕驅動（VS Code + Wokwi）

自己寫 ILI9341 240x320 TFT 螢幕的最小驅動：初始化、設定視窗、填矩形。驅動只依賴一組匯流排函式指標（`lcd_bus_t`），附 6 個用假匯流排的單元測試。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day07_SPI_ILI9341` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路（VSPI）
| ILI9341 | ESP32 |
| --- | --- |
| SCK | GPIO18 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| CS | GPIO17 |
| D/C | GPIO16 |
| RST | GPIO4 |
| VCC、LED | 3.3V |
| GND | GND |

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 輪流填滿紅綠藍，印出每次填滿要幾 ms |
| 1 | 比較 SPI 時脈 10、20、40 MHz 的填滿時間，和理論值對照（參考解） |
| 2 | 同一塊 100x100：逐點畫 vs 一次 `fill_rect`，看差幾倍（參考解） |
| 3 | 彈跳方塊動畫：只擦舊位置、畫新位置（參考解） |

模擬器的時間和實體板不同，練習 1、2 的數字請以實體板為準；比例關係才是重點。

## 程式庫
| 檔案 | 內容 |
| --- | --- |
| `lib/ili9341/lcd_bus.h` | 匯流排抽象：`cs`、`dc`、`write`、`delay_ms` |
| `lib/ili9341/ili9341.c` | 初始化序列、CASET/PASET 視窗、RAMWR 連續寫像素、裁切 |
| `test/test_ili9341` | 檢查初始化指令順序、視窗座標的位元組、像素數、裁切、RGB565 |

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或：`gcc -I<unity>/src -Ilib/ili9341 <unity>/src/unity.c lib/ili9341/ili9341.c test/test_ili9341/test_main.c -o test_ili9341`
