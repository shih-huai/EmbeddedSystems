# Day 04：硬體計時器、PWM、Watchdog（VS Code + Wokwi）

用硬體計時器做精準 tick、用 LEDC 做 PWM 呼吸燈、用 Task Watchdog 從卡死中自動恢復。計算計時器與 PWM 參數的程式放在 `lib/timing_calc`，附單元測試。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day04_Timers_PWM_Watchdog` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路
| 元件 | 腳位 |
| --- | --- |
| 紅 LED（串 220Ω），PWM 呼吸燈 | GPIO18 |
| 綠 LED（串 220Ω），心跳燈 | GPIO19 |
| 按鈕（另一端接 GND） | GPIO4，內部上拉 |

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 硬體計時器 1 kHz tick，綠燈每 500 tick 切換。比較 `ticks` 和 `millis()` 是否同步 |
| 1 | LEDC PWM 呼吸燈，5 kHz、13 位元，加上簡單的 gamma 校正（參考解） |
| 2 | Task Watchdog：按住按鈕模擬卡死，3 秒後自動重開，開機時印出重開原因（參考解） |
| 3 | 用 `tc_pwm_max_bits()` 算出各頻率的最大解析度，交給 `ledcSetup()` 驗證；多 1 位元就會失敗（參考解） |

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc，例如 MSYS2 的 MinGW-w64）
- 或直接用 gcc：
  ```
  gcc -I<unity>/src -Ilib/timing_calc <unity>/src/unity.c lib/timing_calc/timing_calc.c test/test_timing_calc/test_main.c -o test_timing_calc
  ./test_timing_calc
  ```

## 版本注意
計時器與 LEDC 用的是 Arduino-ESP32 2.0.x 的 API（`timerBegin(0, 80, true)`、`ledcSetup` + `ledcAttachPin`）。`platformio.ini` 已固定 `espressif32@^6.9.0`。Arduino 3.x 改成 `timerBegin(頻率)`、`ledcAttach(pin, freq, bits)`。
