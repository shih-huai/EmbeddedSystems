# Day 09：FreeRTOS（一）task、排程、優先權、堆疊（VS Code + Wokwi）

建立 task、看高優先權怎麼搶占低優先權、比較 `vTaskDelay` 和 `vTaskDelayUntil`、製造餓死和堆疊溢位。週期統計放在 `lib/period_stats`，附單元測試。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day09_FreeRTOS_Tasks` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路
| 元件 | 腳位 |
| --- | --- |
| 綠 LED（串 220Ω），由 `blink_task` 閃爍 | GPIO18 |

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 優先權 5 的 task 每 100 ms 醒來，同核心有一個優先權 1 的 task 一直在計算。看高優先權 task 的週期有多準 |
| 1 | 每次多做 3 ms 工作：`vTaskDelay(100)` 的週期變 103 ms、越漂越遠；`vTaskDelayUntil` 保持 100 ms（參考解） |
| 2 | 一個優先權 5、不讓出 CPU 的 task 釘在核心 1，`loop()` 在那 3 秒每秒 0 圈（餓死） |
| 3 | 用 `uxTaskGetStackHighWaterMark` 量堆疊用量；2 KB 堆疊遞迴 40 層會溢位。溢位不一定印出「stack overflow」，常常是蓋壞別的資料後才在別處當機（例如 LoadProhibited），這正是它難查的原因 |

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或：`gcc -I<unity>/src -Ilib/period_stats <unity>/src/unity.c lib/period_stats/period_stats.c test/test_period_stats/test_main.c -o test_period_stats`

## 小提醒
- ESP-IDF 版的 FreeRTOS，`xTaskCreate` 的堆疊大小單位是 **byte**；原版 FreeRTOS 是 word（4 bytes）。
- `uxTaskGetStackHighWaterMark` 回傳的是「曾經剩下最少多少」，越接近 0 越危險。
