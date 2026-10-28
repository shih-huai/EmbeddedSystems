# ESP32 韌體與驅動面試速成：第 10 課

## 第 10 課：FreeRTOS（二）queue、semaphore、mutex、ISR 交接

一句話：task 之間、ISR 和 task 之間不要直接共用變數，用 RTOS 提供的物件交接。資料用 queue，事件用 semaphore 或 task notification，共享資源用 mutex。

### 概念

**該用哪一種**：

| 物件 | 用途 | 重點 |
| --- | --- | --- |
| Queue | 傳資料 | 以「複製」放入，滿了可以等或丟棄；ISR 用 `xQueueSendFromISR` |
| Binary semaphore | 通知「發生了」 | 只有 0 和 1，事件重疊時會合併 |
| Counting semaphore | 通知「發生了幾次」、管理 N 個資源 | 不會合併事件 |
| Mutex | 保護共享資源 | 有擁有者、有優先權繼承；只能由拿鎖的 task 放，不能在 ISR 用 |
| Task notification | 叫醒某一個特定 task | 最快最省記憶體，但只能有一個接收者 |
| Event group | 等「多個條件」中的任一或全部 | 例如 Wi-Fi 連上且時間同步完成 |

**ISR 交接（deferred interrupt processing）**：ISR 只做最少的事，例如讀資料、清旗標、丟進 queue 或 give semaphore，真正的處理交給 task。ISR 裡只能用 `...FromISR` 版本的 API，不能等待。它們多一個 `pxHigherPriorityTaskWoken` 參數：如果這次叫醒的 task 比被中斷的 task 優先權高，就在 ISR 結尾呼叫 `portYIELD_FROM_ISR()`，離開 ISR 就直接切過去，不用等下一個 tick。

**優先權反轉（priority inversion）**：低優先權 task 拿著鎖，高優先權 task 在等這把鎖，這時中優先權 task 跑起來（它不需要鎖），把低優先權 task 擋住，結果高優先權 task 要等中優先權 task 做完。解法是**優先權繼承**：FreeRTOS 的 mutex 會讓拿鎖的 task 暫時升到等待者的優先權。binary semaphore 沒有這個機制，所以「當鎖用」一定要用 mutex。

**Deadlock**：兩個 task 各拿一把鎖又等對方的鎖。避免方法：所有地方用固定順序拿鎖、拿鎖時設 timeout、盡量一次只拿一把。

**Critical section vs mutex**：`portENTER_CRITICAL` 關中斷（ESP32 還加 spinlock 擋另一顆核心），只能包幾行，裡面不能呼叫會 block 的 API。mutex 可以包較長的動作（例如一整次 I2C 交易），等鎖時 task 進入 Blocked，不浪費 CPU。

### Wokwi 練習 10：ISR 到 task、事件遺失、競爭、優先權反轉

程式專案是 `Day10_FreeRTOS_Sync`。LED 接 GPIO18，按鈕接 GPIO4 到 GND。按鈕去彈跳放在 `lib/debounce`，附 4 個單元測試。

| EXERCISE | 內容 |
| --- | --- |
| 0 | 計時器 ISR 每 10 ms 把一筆取樣丟進長度 8 的 queue，task 取出並檢查延遲和缺口；3 秒後消費者變慢。按鈕 ISR 用 task notification 叫醒 task，在 task 裡去彈跳再切換 LED |
| 1 | ISR 每 100 ms 連續發生 5 個事件，每個要處理 3 ms，比較 binary 和 counting semaphore |
| 2 | 兩個同優先權的 task 各把共享變數加 1000 次，讀和寫之間被切走，比較有無 mutex |
| 3 | low 拿鎖、high 等鎖、medium 霸佔 CPU，比較用 binary semaphore 和 mutex 當鎖 |

我在 ESP32 模擬器（QEMU）上跑過四個練習，按鈕部分要在 Wokwi 或實機上按：

| 練習 | 結果 |
| --- | --- |
| 0 | 正常時 ISR 到 task 最大延遲 12 µs、0 遺失。消費者變慢後 queue 一直滿著 8 筆，延遲變成約 120 ms，每 100 筆丟掉約 50 筆 |
| 1 | 150 個事件：binary semaphore 處理 90 個、遺失 60 個；counting semaphore 150 個全部處理 |
| 2 | 沒有保護：結果 1000（應該 2000）；有 mutex：2000 |
| 3 | binary semaphore 當鎖：high 等了 476 ms（等 medium 做完）；mutex：high 只等 35 ms（low 繼承優先權後把剩下的工作做完） |

練習 0 說明了 queue 長度的意義：queue 只能吸收短暫的突發，消費者長期比生產者慢，再長的 queue 都會滿，而且滿的 queue 會讓每一筆資料的延遲都變長。

```c
static void IRAM_ATTR on_timer() {
  sample_t s = { isr_seq++, (uint32_t)micros() };
  BaseType_t woken = pdFALSE;
  if (xQueueSendFromISR(sample_q, &s, &woken) != pdTRUE)
    isr_drops++;                       // queue 滿了：ISR 不能等，只能丟並記下來
  if (woken) portYIELD_FROM_ISR();     // 叫醒了更高優先權的 task，離開 ISR 就切過去
}

static void sample_task(void *) {
  for (;;) {
    sample_t s;
    xQueueReceive(sample_q, &s, portMAX_DELAY);   // 沒資料時 Blocked，不佔 CPU
    process(&s);
  }
}
```

### 面試題與參考答案

**Q1. queue、semaphore、mutex 各用在什麼場合？**
要傳資料用 queue；只要通知「事件發生了」用 semaphore（要記次數用 counting）或 task notification；保護共享資源（例如 I2C 匯流排、一個全域結構）用 mutex。mutex 有擁有者和優先權繼承，semaphore 沒有。

**Q2. 為什麼不能拿 binary semaphore 當鎖？**
它沒有優先權繼承，會發生優先權反轉；它也沒有擁有者，任何 task 甚至 ISR 都可以 give，錯誤的 give 不會被發現。保護資源要用 mutex，需要同一個 task 重複拿鎖時用 recursive mutex。

**Q3. 解釋優先權反轉，以及怎麼解決。**
低優先權 task 拿著鎖，高優先權 task 等這把鎖，中優先權 task 把低優先權 task 擋住，高優先權 task 實際上被中優先權 task 卡住。解法：用有優先權繼承的 mutex；縮短持有鎖的時間；或用 priority ceiling（拿鎖時直接升到可能使用這把鎖的最高優先權）。經典案例是火星探路者號（Mars Pathfinder）的重開機問題。

**Q4. ISR 裡可以呼叫哪些 FreeRTOS API？`pxHigherPriorityTaskWoken` 是做什麼的？**
只能呼叫 `...FromISR` 版本，而且不能等待。`pxHigherPriorityTaskWoken` 由 API 填入：如果這次叫醒的 task 優先權比被中斷的 task 高，它會被設成 pdTRUE，ISR 結尾呼叫 `portYIELD_FROM_ISR()` 就能在離開 ISR 時直接切到那個 task，縮短反應時間。

**Q5. 中斷來得很快，資料怎麼交給 task 才不會掉？**
ISR 只把資料放進 queue 或環形緩衝區，task 批次處理。queue 長度要能吸收最壞情況的突發，消費者的平均處理速度一定要快過生產速度。量測 queue 的最高水位和丟棄次數，大量資料改用 DMA 加雙緩衝。

**Q6. 怎麼避免 deadlock？**
所有地方用相同順序拿鎖；拿鎖用 timeout，逾時就記錄錯誤並回退；持有鎖時不要呼叫可能拿另一把鎖的函式；能用一把鎖就不要用兩把，或把資源交給單一 task 管理，其他 task 透過 queue 發請求。

**Q7. task notification 和 semaphore 比有什麼優缺點？**
task notification 直接寫在 task 控制區塊裡，不用另外配置物件，速度快、省記憶體。缺點是只能通知一個特定 task，不能多個 task 等同一個事件，也不能在 ISR 裡等待。一對一的「ISR 叫醒 driver task」最適合用它。
