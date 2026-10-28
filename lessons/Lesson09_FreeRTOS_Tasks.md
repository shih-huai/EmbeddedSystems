# ESP32 韌體與驅動面試速成：第 9 課

## 第 9 課：FreeRTOS（一）task、排程、優先權、堆疊

一句話：RTOS 讓你把程式拆成多個 task，每個 task 有自己的堆疊和優先權；排程器永遠讓「準備好的、優先權最高的」task 執行，task 用 delay 或等待資源主動讓出 CPU。

### 概念

**Task 的四種狀態**：

| 狀態 | 意思 |
| --- | --- |
| Running | 正在這顆核心上執行 |
| Ready | 可以執行，在等 CPU |
| Blocked | 在等時間（`vTaskDelay`）或等事件（queue、semaphore），不佔 CPU |
| Suspended | 被 `vTaskSuspend` 暫停，不會被排程 |

**搶占式排程（preemptive）**：只要有更高優先權的 task 變成 Ready（例如它的 delay 時間到了、或 ISR 送了訊號給它），排程器立刻切換過去，不用等目前的 task 讓出。同優先權的 task 會在每個 tick（ESP32 預設 1 ms）輪流（time slicing）。合作式排程（cooperative）則要 task 自己讓出 CPU 才會切換。

**優先權怎麼設**：數字越大越優先。原則是「越需要準時、做得越短的越高」，例如通訊協定處理高、UI 和 log 低。高優先權 task 一定要會 block，不然會讓同核心所有低優先權的 task 餓死（starvation），包括 idle task，然後 watchdog 會叫。

**`vTaskDelay` vs `vTaskDelayUntil`**：`vTaskDelay(100)` 是「從現在起睡 100 ms」，工作本身花的時間會累加上去，週期越跑越長。`vTaskDelayUntil(&last, 100)` 是「睡到上次預定時間 + 100 ms」，週期固定，適合取樣、控制迴圈。

**堆疊**：每個 task 有自己固定大小的堆疊，放區域變數、函式呼叫、中斷時的暫存器。太小會溢位，蓋到別人的記憶體，症狀常常是莫名其妙的當機。用 `uxTaskGetStackHighWaterMark()` 看「剩餘堆疊曾經最低到多少」，留 20–30% 餘裕。注意 ESP-IDF 的 `xTaskCreate` 堆疊單位是 **byte**，原版 FreeRTOS 是 word。

**ESP32 的雙核心**：ESP-IDF 的 FreeRTOS 是 SMP 版本，兩顆核心各有自己的排程。`xTaskCreatePinnedToCore` 可以把 task 固定在某顆核心；Wi-Fi/BT 協定堆疊通常在核心 0，Arduino 的 `loop()` 在核心 1。

### Wokwi 練習 9：搶占、漂移、餓死、溢位

程式專案是 `Day09_FreeRTOS_Tasks`，一顆 LED 接 GPIO18 由 `blink_task` 閃爍。週期統計放在 `lib/period_stats`，附 5 個單元測試。

| EXERCISE | 內容 |
| --- | --- |
| 0 | 優先權 5 的 task 每 100 ms 醒來，同核心有一個優先權 1 的 task 一直在計算 |
| 1 | 每次多做 3 ms 工作，比較 `vTaskDelay` 和 `vTaskDelayUntil` 的漂移 |
| 2 | 一個優先權 5、不讓出 CPU 的 task 霸佔核心 1 三秒 |
| 3 | 量測堆疊用量；2 KB 堆疊遞迴 40 層讓它溢位 |

四個練習我都在 ESP32 模擬器（QEMU，用指令計數模式讓時間穩定）上跑過：

| 練習 | 結果 |
| --- | --- |
| 0 | 高優先權 task 週期 100,000 µs，最短最長都是 100,000，同核心的計算 task 照樣在跑 |
| 1 | `vTaskDelay`：平均 103,009 µs，50 個週期累積漂移 +150 ms；`vTaskDelayUntil`：平均 99,988 µs，漂移不到 1 ms |
| 2 | hog 霸佔期間，`loop()` 每秒 0 圈；hog 結束後恢復到每秒約 930 圈 |
| 3 | 4 KB 堆疊遞迴 5 層剩 2,896 bytes、20 層只剩 256 bytes；2 KB 遞迴 40 層當機，而且錯誤訊息是 LoadProhibited，不是「stack overflow」 |

練習 3 的結果是重點：堆疊溢位常常先蓋壞別的資料，在別處才當機，錯誤訊息看起來和堆疊毫無關係。

```c
static void periodic(void *arg) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    do_work();                                      // 每次約 3 ms
    vTaskDelayUntil(&last, pdMS_TO_TICKS(100));     // 以預定時間為基準，不會漂移
  }
}
xTaskCreatePinnedToCore(periodic, "periodic", 4096, nullptr, 2, nullptr, 1);
//                      函式       名稱        堆疊(bytes) 參數  優先權 handle 核心
```

### 面試題與參考答案

**Q1. RTOS 和 super loop（一個大 while 迴圈）各適合什麼？**
super loop 簡單、可預測、沒有切換開銷，適合功能少的小系統。功能一多，一個慢的動作就拖累全部，時序很難保證。RTOS 讓每個功能是獨立的 task，用優先權保證重要的事準時，等待時不佔 CPU；代價是記憶體（每個 task 一個堆疊）、切換開銷，以及共享資源要同步。

**Q2. 什麼是搶占？什麼情況下會發生 context switch？**
更高優先權的 task 變成 Ready 時，排程器立刻中斷目前的 task 切換過去。context switch 發生在：tick 中斷時（時間片輪轉或 delay 到期）、task 呼叫會 block 的 API、ISR 喚醒更高優先權的 task 並呼叫 `portYIELD_FROM_ISR`、task 主動 `taskYIELD`。

**Q3. `vTaskDelay` 和 `vTaskDelayUntil` 差在哪？**
`vTaskDelay` 是相對時間，從呼叫當下開始算，所以工作時間會累加，週期會漂移。`vTaskDelayUntil` 是絕對時間，以上次預定的喚醒時間為基準，週期固定。週期性取樣、控制迴圈要用後者。

**Q4. 怎麼決定 task 的堆疊大小？**
先給寬鬆的值，實際跑過最壞情況（最深的呼叫路徑、最大的區域陣列、printf 這類吃堆疊的函式），用 `uxTaskGetStackHighWaterMark` 看最低剩餘量，再留 20–30% 餘裕。開發時開啟堆疊溢位檢查。避免在堆疊上放大陣列和遞迴。

**Q5. 什麼是餓死（starvation）？怎麼避免？**
高優先權 task 一直不 block，同核心較低優先權的 task 永遠拿不到 CPU。避免方法：高優先權 task 必須在等事件或 delay；耗時的計算放低優先權；用 watchdog 偵測 idle task 沒機會執行的情況。

**Q6. 為什麼不要在 task 裡用忙等（busy wait）？**
忙等期間 task 一直是 Running，同優先權和較低優先權的 task 都得不到 CPU，也浪費電。該用 `vTaskDelay`、等 queue 或 semaphore，讓 task 進入 Blocked，CPU 去做別的事或進入省電。

**Q7. 一個 task 跑一段時間後隨機當機，錯誤訊息每次都不一樣，你會先懷疑什麼？**
堆疊溢位和記憶體被蓋寫（陣列越界、野指標、use-after-free）。先檢查各 task 的 high water mark，加大堆疊看問題是否消失，開啟 FreeRTOS 的溢位檢查和 heap 毒化檢查，再用 core dump 的 backtrace 找源頭。
