# Day 03：中斷與 ISR（VS Code + Wokwi）

用按鈕中斷和硬體計時器中斷，練習 ISR 的規矩：要短、要放在 IRAM、和主程式共享的變數要保護。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day03_Interrupts_ISR` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

修改程式後要**重新編譯**，Wokwi 才會載入新的韌體。

## 電路
| 元件 | 腳位 |
| --- | --- |
| 紅 LED（串 220Ω） | GPIO18 |
| 按鈕（另一端接 GND） | GPIO4，內部上拉 |

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 按鈕下降緣觸發中斷，ISR 只計數，`loop()` 負責印出。按一次看中斷被觸發幾次（彈跳也會觸發） |
| 1 | 在 ISR 裡用 50 ms 時間鎖定去彈跳，每按一次 LED 切換一次（參考解） |
| 2 | 計時器 ISR 每 20 µs 對共享變數加 1，`loop()` 也同時加 1，觀察「遺失=」大於 0 |
| 3 | 用 `portENTER_CRITICAL` 修好練習 2，「遺失=」應該變成 0（參考解） |

### 練習 2 會看到什麼
`shared++` 是讀、加、寫三個步驟。如果計時器中斷剛好發生在「讀」和「寫」之間，ISR 加上去的那一次會被主程式寫回的舊值蓋掉。每輪遺失的數字會不一樣，這正是競態條件難抓的原因。

### 練習 3 要注意
- 主程式用 `portENTER_CRITICAL(&mux)`，ISR 裡用 `portENTER_CRITICAL_ISR(&mux)`。
- ESP32 是雙核心，關中斷只擋得住自己這顆核心，所以 critical section 還要搭配一把 spinlock（`portMUX_TYPE`）。
- critical section 裡的程式要越短越好，期間這顆核心的中斷都被延後。

## 版本注意
這裡用的是 Arduino-ESP32 2.0.x 的計時器 API（`timerBegin(0, 80, true)`、`timerAlarmWrite`）。`platformio.ini` 已固定 `espressif32@^6.9.0`，所以會拿到 2.0.x。Arduino 3.x 改成 `timerBegin(頻率)` 與 `timerAlarm()`，網路範例要看清楚版本。
