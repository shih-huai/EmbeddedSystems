# Day 10：FreeRTOS（二）queue、semaphore、mutex、ISR 交接（VS Code + Wokwi）

ISR 怎麼把資料和事件交給 task、binary 和 counting semaphore 的差別、為什麼共享資料要 mutex、優先權反轉和優先權繼承。按鈕去彈跳放在 `lib/debounce`，附單元測試。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day10_FreeRTOS_Sync` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路
| 元件 | 腳位 |
| --- | --- |
| 綠 LED（串 220Ω） | GPIO18 |
| 按鈕（另一腳接 GND，內部上拉） | GPIO4 |

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 計時器 ISR 每 10 ms 把一筆取樣（序號、時間）丟進長度 8 的 queue，task 取出並檢查延遲和缺口。3 秒後消費者變慢（每筆 15 ms），queue 滿了、ISR 開始丟資料。按鈕 ISR 用 task notification 叫醒 task，task 去彈跳後切換 LED |
| 1 | ISR 每 100 ms 連續發生 5 個事件，每個事件要處理 3 ms。binary semaphore 會把重疊的事件合併成一個，counting semaphore 不會 |
| 2 | 兩個同優先權的 task 各把共享變數加 1000 次，讀和寫之間被切走。沒保護只剩約 1000，有 mutex 是 2000 |
| 3 | 優先權反轉：low 拿鎖、high 等鎖、medium 霸佔 CPU。用 binary semaphore 當鎖，high 要等到 medium 做完；用 mutex，low 暫時繼承 high 的優先權，high 很快拿到鎖 |

## 模擬器上量到的結果（QEMU）
| 練習 | 結果 |
| --- | --- |
| 0 | 正常時最大延遲 12 µs、0 遺失；消費者變慢後 queue 一直是滿的 8 筆，延遲變成約 120 ms，每 100 筆丟掉約 50 筆 |
| 1 | 150 個事件：binary 處理 90 個（遺失 60），counting 處理 150 個 |
| 2 | 沒有保護 1000，有 mutex 2000 |
| 3 | binary semaphore：high 等 476 ms；mutex：high 等 35 ms |

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或：`gcc -I<unity>/src -Ilib/debounce <unity>/src/unity.c lib/debounce/debounce.c test/test_debounce/test_main.c -o test_debounce`

## 小提醒
- ISR 裡只能呼叫 `...FromISR` 結尾的 API，而且不能等待（不能傳 timeout）。
- `portYIELD_FROM_ISR()`：ISR 叫醒了更高優先權的 task 時呼叫，離開 ISR 就直接切到那個 task，不用等下一個 tick。
- mutex 不能在 ISR 裡用，也只能由拿鎖的 task 自己放。
