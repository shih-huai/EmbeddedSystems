# ESP32 韌體與驅動面試速成：第 5 課

## 第 5 課：UART 與 ring buffer

一句話：UART 是沒有時脈線、靠雙方約好鮑率的非同步串列；驅動的核心工作，是用中斷把硬體 FIFO 的資料搬進 ring buffer，讓主程式慢慢處理。

### 概念

**一個 UART 框格**：閒置時線是高電位。一個低電位的 start bit，接著 5–9 個資料位元（**低位元先送**），可選的同位位元，最後 1 或 2 個高電位的 stop bit。最常見的 8N1 是每個位元組 10 個位元，所以 115200 baud 一秒最多約 11,520 個位元組。

**鮑率誤差**：接收端看到 start bit 的下降緣就開始計時，在每個位元的中間取樣（通常用 16 倍過取樣）。到第 10 個位元時累積誤差不能超過半個位元，所以兩端加起來的誤差大約要在 ±2–3% 內。鮑率不對時，接收端在該是 stop bit 的地方讀到 0，就是 **framing error**。

**硬體流量控制**：RTS/CTS 兩條線，接收端快滿時告訴對方先停。沒有流量控制又來不及讀，就會 **overrun**（硬體 FIFO 滿了，新資料被丟掉）。

**ESP32 的 UART 驅動**：3 組 UART，每組有 128 位元組的硬體 FIFO。驅動設定「FIFO 快滿」和「一段時間沒新資料（RX timeout）」兩種中斷，在中斷裡把 FIFO 搬進軟體 ring buffer。因為不是每個位元組中斷一次，CPU 負擔很小。進一步可以用 DMA（第 8 課）。

**SPSC ring buffer（單一生產者、單一消費者）**：

- 容量用 2 的次方，取餘數變成 `& mask`，索引是一直往上加的 `uint32_t`，滿的條件是 `head - tail == size`，每一格都用得到。
- `head` 只有生產者（ISR）寫，`tail` 只有消費者（主程式）寫，所以不用上鎖也不用關中斷。
- 順序很重要：生產者先寫資料再更新 `head`。單核心上 volatile 加上編譯器屏障就夠；雙核心要用 release/acquire（本課用 GCC 的 `__atomic` 內建函式）。

### Wokwi 練習 5：自己跟自己通訊，再做一個指令列

程式專案是 `Day05_UART_RingBuffer`。電路上 GPIO27（UART1 TX）接到 GPIO16（UART2 RX），LED 接 GPIO18。ring buffer 和指令列放在 `lib/ringbuf`、`lib/cli`，共 12 個單元測試。

| EXERCISE | 內容 |
| --- | --- |
| 0 | UART1 每秒送一行字，UART2 收到後印出來 |
| 1 | 送 115200、收 9600，看收到的位元組和 framing error 次數 |
| 2 | 在 Serial Monitor 打 `help`、`led on`、`blink 200`、`stats` 控制 LED |
| 3 | ring buffer 只有 64 bytes、消費者很慢，看資料被丟掉 |

練習 2 的資料流：UART 驅動的回呼是生產者，`loop()` 是消費者。我在 ESP32 模擬器上送進 `help`、`led on`、`blink 200`、`foo`、`stats`，回應都正確。ring buffer 另外用兩個執行緒在電腦上對傳了 5,000 萬個位元組，沒有任何錯誤。

```c
bool rb_put(ringbuf_t *rb, uint8_t byte) {          // 只有生產者呼叫
  uint32_t head = __atomic_load_n(&rb->head, __ATOMIC_RELAXED);
  uint32_t tail = __atomic_load_n(&rb->tail, __ATOMIC_ACQUIRE);
  if (head - tail > rb->mask) { rb->dropped++; return false; }   // 滿了
  rb->buf[head & rb->mask] = byte;
  __atomic_store_n(&rb->head, head + 1, __ATOMIC_RELEASE);        // 資料寫好才公布
  return true;
}
```

### 面試題與參考答案

**Q1. 畫出一個 8N1 的 UART 框格。115200 baud 一秒能傳多少位元組？**
start bit（0）、資料 bit0 到 bit7（低位元先送）、stop bit（1），共 10 個位元。115200 / 10 = 11,520 位元組每秒。

**Q2. 兩端鮑率差多少還能通？為什麼？**
接收端只在 start bit 對齊一次，之後靠自己的時脈在位元中間取樣。到最後一個位元累積誤差不能超過半個位元，10 個位元下理論上限約 5%，扣掉取樣誤差與邊緣斜率，實務上兩端合計要在 2–3% 內。這也是為什麼用內部 RC 振盪器的 MCU 做 UART 要小心。

**Q3. UART、I2C、SPI 怎麼選？**
UART：兩條線、點對點、不需時脈，適合除錯輸出、模組（GPS、藍牙、數據機）、較長距離（搭 RS-485）。I2C：兩條線掛很多裝置、有位址與 ACK，適合低速感測器。SPI：四條線以上、全雙工、幾十 MHz，適合螢幕、flash、ADC。

**Q4. 為什麼 ring buffer 的容量常用 2 的次方？**
取餘數可以用 `& (size-1)`，比除法快很多（很多 MCU 沒有硬體除法）。且 2^32 是 size 的倍數，索引可以讓它一直往上加、自然溢位，`head - tail` 永遠是正確的個數，不用為了區分滿和空而浪費一格。

**Q5. 這個 ring buffer 為什麼不用上鎖？什麼情況下就需要？**
因為每個索引只有一方寫，且 32 位元寫入是原子的。如果有兩個生產者（例如兩個 task 都想往同一個緩衝區寫 log），兩個人會同時寫 `head`，就需要 mutex、critical section 或 compare-and-swap。

**Q6. 接收資料會掉，你怎麼查？**
先分是哪一層掉的：硬體 FIFO overrun（中斷太慢，紀錄 overrun 旗標）、軟體緩衝區滿（主程式太慢，計數丟棄）、framing error（鮑率或電氣問題，用示波器量位元寬度）。解法依序是：調低中斷門檻、加大緩衝區、開硬體流量控制、改用 DMA。

**Q7. 指令列收到一個超長的行該怎麼處理？**
不要寫超過緩衝區（緩衝區溢位是經典的安全漏洞），也不要交出被截斷的指令，因為截斷後可能變成另一個合法指令。本課的做法是標記溢位，等到換行整行丟掉，單元測試有一個案例專門測這件事。
