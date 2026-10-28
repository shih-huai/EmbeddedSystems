# Day 12：低功耗、HAL 重構、總複習（VS Code + Wokwi）

深度睡眠和淺度睡眠、各種喚醒來源，最後把前面學的東西組成一個小應用：應用邏輯只透過 HAL 碰硬體，在電腦上用假 HAL 測試，在 ESP32 上閒置就睡。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day12_LowPower_HAL_Capstone` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路
| 元件 | 腳位 |
| --- | --- |
| 綠 LED（串 220Ω） | GPIO18 |
| 按鈕（另一腳接 GND） | GPIO33（RTC GPIO，深度睡眠時能喚醒） |
| 可變電阻中間腳（兩端接 3V3、GND） | GPIO34（ADC1） |

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 深度睡眠 2 秒、計時器喚醒，重複 3 次。`RTC_DATA_ATTR` 的開機次數和睡前時間會保留，一般變數不會 |
| 1 | 深度睡眠，按按鈕（ext0）或 10 秒到期喚醒，印出喚醒原因；按鈕喚醒時 LED 亮 1 秒 |
| 2 | 淺度睡眠 500 ms × 5 次，按按鈕可以提早叫醒。程式從呼叫的下一行繼續，區域變數都還在 |
| 3 | 總複習：`lib/sensor_app` 是純 C 的應用邏輯，按鈕切換取樣（每 100 ms 讀可變電阻、每 10 筆回報），閒置 10 秒就深度睡眠，按按鈕叫醒 |

## 驗證狀況
- `lib/sensor_app` 有 5 個單元測試，在電腦上用假 HAL、模擬時間跑過，全部通過。
- 四個練習都編譯過、0 warning。
- ESP32 模擬器（QEMU）沒有模擬 RTC 和睡眠電路，呼叫 `esp_deep_sleep_start` 會在 `esp_sleep_start` 裡當機，所以睡眠部分沒辦法在 QEMU 驗證。請在 Wokwi 或實體開發板上跑。

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或：`gcc -I<unity>/src -Ilib/debounce -Ilib/sensor_app <unity>/src/unity.c lib/debounce/debounce.c lib/sensor_app/sensor_app.c test/test_sensor_app/test_main.c -o test_sensor_app`

## 小提醒
- 睡前呼叫 `Serial.flush()`，否則 UART 緩衝區最後幾個字會不見。
- 深度睡眠時一般 GPIO 的上拉會關掉，喚醒腳要用 `rtc_gpio_pullup_en`。
- 用按鈕喚醒後，要等按鈕放開再睡，否則會一直馬上被叫醒。
- 實機上量電流時要拔掉 USB 轉 UART 晶片的影響（開發板上的 LED、USB 晶片、穩壓器都會吃電），裸晶片深度睡眠約 10 µA 等級，開發板常常是 mA 等級。
