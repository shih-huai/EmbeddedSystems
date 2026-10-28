# ESP32 韌體與驅動面試速成：第 3 課

## 第 3 課：中斷與 ISR

一句話：中斷讓硬體在事件發生的瞬間打斷 CPU，ISR 要做得極短，把重活丟給主程式或 task，而且和主程式共享的資料一定要保護。

### 概念

**一次中斷發生什麼事**：

1. 周邊設起中斷旗標（例如 GPIO 偵測到下降緣）。
2. 中斷控制器把它送到 CPU。ESP32 有一個 interrupt matrix，把約 70 個中斷來源對映到每顆核心的 32 個 CPU 中斷；ARM Cortex-M 對應的是 NVIC。
3. CPU 停下目前的指令、保存後走到中斷向量，執行 ISR。
4. ISR 清掉中斷旗標（沒清會一直重複進來），返回後 CPU 接著執行。

從事件發生到 ISR 第一行執行的時間叫**中斷延遲**（latency）。關中斷太久、優先權較高的 ISR 太長，都會讓它變大。

**ISR 的規矩**：

- 要短：記錄事件、搬資料、清旗標，就結束。重活丟給主迴圈或 task（上半部／下半部的分工）。
- 不能阻塞：不用 `delay()`、不等 mutex、不寫長字串到 Serial、不 `malloc`。要通知 FreeRTOS task，只能用結尾是 `FromISR` 的 API（第 10 課）。
- 放在 IRAM：ESP32 的程式通常從 flash 經由 cache 執行。寫 flash（例如存 NVS）時 cache 會被關掉，這時如果中斷觸發、ISR 又在 flash 裡，CPU 就讀不到指令而當機。`IRAM_ATTR` 把 ISR 放進內部 RAM。
- 共享變數：加 `volatile`，且讀–改–寫要放在 critical section 裡。

**競態條件（race condition）**：`shared++` 是讀、加、寫三個步驟。主程式讀完、還沒寫回時中斷發生，ISR 對它加 1，接著主程式寫回舊值，ISR 的那次更新就消失了。修法是在主程式這邊關中斷（critical section）。ESP32 是雙核心，關中斷只擋得住自己這顆核心，所以 `portENTER_CRITICAL()` 還搭配一把 spinlock（`portMUX_TYPE`）。

**邊緣觸發與水平觸發**：邊緣觸發在電位「變化」時觸發一次，適合按鈕、脈衝計數。水平觸發在電位「維持」時一直觸發，適合多個裝置共用的中斷線：只要還有裝置在拉低，就不會漏接。代價是沒清掉來源就會卡在 ISR 裡。

### Wokwi 練習 3：按鈕中斷與競態條件

程式專案是 `Day03_Interrupts_ISR`。電路是一顆 LED 接 GPIO18，按鈕接 GPIO4 到 GND。

| EXERCISE | 內容 |
| --- | --- |
| 0 | 按鈕下降緣中斷，ISR 只計數，`loop()` 負責印出。彈跳會讓一次按下觸發好幾次 |
| 1 | 在 ISR 裡用 50 ms 時間鎖定去彈跳，並用 W1TS／W1TC 切換 LED |
| 2 | 計時器 ISR 每 20 µs 對共享變數加 1，主程式也同時加 1，印出「遺失」的次數 |
| 3 | 用 critical section 修好練習 2，遺失應為 0 |

練習 2、3 的核心：

```c
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR on_timer() {
  portENTER_CRITICAL_ISR(&mux);   // ISR 裡用 _ISR 版本
  shared++;
  portEXIT_CRITICAL_ISR(&mux);
}

// loop() 裡
portENTER_CRITICAL(&mux);
shared++;                         // 這三個步驟現在不會被打斷
portEXIT_CRITICAL(&mux);
```

這個專案用的是 Arduino-ESP32 2.0.x 的計時器 API（`timerBegin(0, 80, true)`），`platformio.ini` 已固定版本。Arduino 3.x 的計時器 API 不同，網路範例要看清楚版本。

### 面試題與參考答案

**Q1. ISR 裡不能做哪些事？為什麼？** 不能阻塞（delay、等 mutex、等 queue），因為 ISR 不是 task，沒辦法被排程器換出去，會卡住整個系統。不要 printf、malloc，因為它們通常不可重入，又慢。浮點運算也要小心，有些平台的 ISR 不保存 FPU 狀態。原則是記錄事件、通知 task，然後馬上返回。

**Q2. volatile 夠不夠保護 ISR 和主程式的共享變數？** 不夠。volatile 只保證每次都真的去讀寫，不保證讀–改–寫不被打斷。只有一方寫、一方讀，且變數寬度不超過 CPU 一次能存取的大小時，volatile 才夠。雙方都寫、或是 64 位元變數、或是幾個欄位要一起更新時，要用 critical section 或 atomic。

**Q3. 什麼是中斷延遲？怎麼降低？** 從事件發生到 ISR 開始執行的時間。降低的方法：縮短關中斷的區段、讓高優先權 ISR 保持短、時間關鍵的中斷設高優先權、ISR 放 IRAM 避免 cache miss。

**Q4. 中斷可以巢狀嗎？** 可以。高優先權的中斷可以打斷低優先權的 ISR。Cortex-M 的 NVIC 由硬體自動處理；ESP32 的 Xtensa 有 1–7 級中斷。代價是堆疊用量要估算最深的巢狀情況。

**Q5. 邊緣觸發和水平觸發各適合什麼？** 邊緣觸發適合計數和單次事件，但 CPU 在處理時如果又來一個邊緣，可能漏掉。水平觸發適合共用的中斷線，只要來源還在就會一直觸發，不會漏，但 ISR 一定要清掉來源，否則會無限重入。

**Q6. 為什麼 ESP32 的 ISR 要加 IRAM\_ATTR？** 程式碼通常放在外部 flash，透過 cache 執行。寫入或擦除 flash 時 cache 會被關掉，這時 flash 裡的程式不能執行。如果 ISR 在 flash 裡而中斷正好發生，就會出現 "Cache disabled but cached memory region accessed" 當機。放在 IRAM 也讓 ISR 不受 cache miss 影響，延遲更穩定。ISR 用到的常數資料也要放 DRAM（`DRAM_ATTR`）。

**Q7. 關中斷在雙核心上夠不夠？** 不夠。關中斷只影響目前這顆核心，另一顆核心仍然可以存取同一份資料。ESP-IDF 的 `portENTER_CRITICAL` 同時關中斷並拿一把 spinlock，兩個核心才都被擋住。

**Q8. 實際專案裡怎麼把 ISR 的工作交給主程式？** 沒有 RTOS 時：ISR 設旗標或把資料放進 ring buffer，主迴圈輪詢處理。有 RTOS 時：ISR 用 `xQueueSendFromISR` 或 `vTaskNotifyGiveFromISR` 喊醒 task，必要時用 `portYIELD_FROM_ISR` 讓高優先權 task 立刻執行。這就是上半部／下半部，Linux 的 softirq、workqueue 也是同樣的想法。

