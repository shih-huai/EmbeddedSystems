# Day 11：開機流程、linker、分割表、OTA（VS Code + Wokwi）

看 linker 把程式和變數放到哪裡、解析自己的韌體映像檔、看哪些變數撐得過重開機、自己做一次 OTA。映像檔解析放在 `lib/app_image`，附單元測試。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day11_Boot_Linker_OTA` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路
不需要外接元件。

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 印出常數、函式、IRAM 函式、`.data`、`.bss`、RTC 變數、heap、堆疊各在哪個位址；再解析正在執行的映像檔，列出每個 segment 載入到哪裡 |
| 1 | 印分割表、執行中和開機設定的分割區、韌體資訊，然後軟體重開機一次、panic 一次，比較 `RTC_NOINIT_ATTR`、`RTC_DATA_ATTR`、一般全域變數的計數 |
| 2 | 自己做 OTA：把目前的韌體複製到另一個 OTA 分割區。先寫一個壞掉 1 bit 的版本，`esp_ota_end` 驗證失敗、開機分割區不變；再寫正確版本、切換開機分割區、重開機，確認從新分割區執行，最後切回來 |
| 3 | `__attribute__((constructor))` 和 C++ 全域物件在 `setup()` 之前就執行了；`setup()` 跑在哪個 task |

## 模擬器上量到的結果（QEMU）
- 練習 0：常數在 `0x3F4…`（DROM）、一般函式在 `0x400D…`（IROM）、IRAM 函式在 `0x4008…`、全域變數在 `0x3FFB…`／`0x3FFC…`、RTC 變數在 `0x5000…`。映像檔有 6 個 segment，兩個 DRAM segment 加起來 16,552 bytes，剛好等於 `.data` 的大小。
- 練習 1：RTC_NOINIT 計數 1→2→3，RTC_DATA 和一般全域每次都是 1；reset 原因依序是 POWERON、SW、PANIC。
- 練習 2：壞掉 1 bit 的版本被擋下（`Checksum failed`、`ESP_ERR_OTA_VALIDATE_FAILED`），正確版本寫入後從 app1 開機，再切回 app0。
- 練習 3：constructor 和全域物件都在 `setup()` 之前執行；`setup()` 在 `loopTask`、核心 1、優先權 1。（模擬器的開機時間比實機慢很多，毫秒數不準。）

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或：`gcc -I<unity>/src -Ilib/app_image <unity>/src/unity.c lib/app_image/app_image.c test/test_app_image/test_main.c -o test_app_image`

## 小提醒
- Wokwi 對 flash 寫入和多次重開機的支援有限，練習 1、2 如果在 Wokwi 表現怪異，請用實體開發板。
- 練習 2 會改寫 otadata 和 app1。在實機上跑完會切回 app0，但 app1 會留著這份韌體，不影響使用。
- 預設分割表（`default.csv`）就有兩個 OTA 分割區；要改分割表，在 `platformio.ini` 加 `board_build.partitions = 你的檔案.csv`。
