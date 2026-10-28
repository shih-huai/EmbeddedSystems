# ESP32 韌體與驅動面試速成：第 8 課

## 第 8 課：DMA、快取、對齊

一句話：DMA 讓周邊自己搬資料、CPU 去做別的事；但 DMA 只能碰特定的記憶體，位址和長度要對齊，有快取的系統還要處理「CPU 看到的」和「DMA 看到的」不一致。

### 概念

**DMA 是什麼**：DMA 控制器照著 CPU 準備好的「描述子」（descriptor：來源位址、長度、下一個描述子），自己在記憶體和周邊之間搬資料，搬完發一個中斷。CPU 只負責設定和收尾。適合大量、規律的資料：螢幕像素、音訊、ADC 連續取樣、UART 高速接收。小量資料用 DMA 反而不划算，設定的開銷比搬資料還大。

**ESP32 的 DMA 限制**：

- 每個 descriptor 最多 4095 bytes，大傳輸要串成一串（驅動會自動處理，本課的 `lib/dma_plan` 示範怎麼切）。
- 緩衝區要在內部 SRAM，不能在 flash（`const` 常數放在 flash）；用 `heap_caps_malloc(size, MALLOC_CAP_DMA)` 配置最保險。
- 位址和長度最好 4 bytes 對齊。

**雙緩衝（ping-pong）**：準備兩塊緩衝區，DMA 送 A 的同時 CPU 填 B，送完再交換。CPU 計算和 DMA 傳輸重疊，總時間接近兩者中較慢的那個，而不是兩者相加。

**快取一致性（cache coherency）**：有資料快取的 CPU（Cortex-M7、ESP32-S3 用 PSRAM 時），CPU 寫的資料可能還在快取裡沒寫回 RAM，DMA 就會送出舊資料；反過來，DMA 寫進 RAM 的新資料，CPU 可能讀到快取裡的舊值。解法：

- DMA 送出前，**clean**（把快取寫回 RAM）。
- DMA 收進來後，**invalidate**（讓快取失效，強迫重讀 RAM）。
- 緩衝區對齊到快取行（cache line，常見 32 bytes），長度也是它的倍數，否則 invalidate 會把旁邊的變數一起清掉。
- 或把 DMA 緩衝區放在不經快取的記憶體。ESP32 的內部 SRAM 不經資料快取，所以本課不用處理，但面試很常問。

**對齊與 padding**：CPU 一次讀 4 bytes 時，位址最好是 4 的倍數。編譯器會在 struct 成員之間補空位（padding）讓每個成員對齊，所以成員順序會影響 sizeof。`__attribute__((packed))` 不補空位，常用在通訊封包格式，但存取比較慢。

**未對齊存取的後果因晶片而異**：

| 平台 | 32 位元讀取未對齊的位址 |
| --- | --- |
| Cortex-M0/M0+ | HardFault |
| Cortex-M3/M4/M7 | 一般 LDR 由硬體處理（較慢），但 LDRD、LDM 會 fault |
| ESP32 內部 DRAM | 硬體處理（較慢） |
| ESP32 IRAM | 只接受 32 位元、對齊的存取；位元組讀寫會 LoadStoreError |

所以解析封包時要用 `memcpy` 讀多位元組的欄位，在任何平台都正確，也避開 C 語言的 strict aliasing 未定義行為。

### Wokwi 練習 8：DMA 雙緩衝與記憶體實驗

程式專案是 `Day08_DMA_Cache_Alignment`，螢幕接線和第 7 課相同，但改用 ESP-IDF 的 `spi_master` 驅動才能控制 DMA。

| EXERCISE | 內容 |
| --- | --- |
| 0 | struct 對齊：同樣的成員，排法不同 sizeof 就不同；packed struct |
| 1 | memcpy 安全讀未對齊資料；指標強制轉型讀 DRAM 未對齊位址；用位元組讀 IRAM 觸發例外 |
| 2 | 畫漸層畫面：「算一塊、等它送完」vs「DMA 雙緩衝邊算邊送」 |
| 3 | 檢查堆疊、全域、malloc、DMA heap、flash 常數，哪些能給 DMA 用 |

練習 0 和 3 在 ESP32 模擬器（QEMU）上跑出來的結果：

| struct | sizeof | 各成員 offset |
| --- | --- | --- |
| `Bad`（u8、u32、u8、u16） | 12 | a=0、b=4、c=8、d=10 |
| `Good`（u32、u16、u8、u8） | 8 | b=0、d=4、a=6、c=7 |
| `Packed`（同 Bad 的順序） | 8 | a=0、b=1、c=5、d=6 |

| 記憶體 | 位址 | 可給 DMA |
| --- | --- | --- |
| 堆疊、全域陣列、malloc、DMA heap | 0x3FFxxxxx（內部 SRAM） | 可以 |
| flash 裡的 `const` 陣列 | 0x3F40xxxx | 不行 |

一整個畫面 153,600 bytes 要 38 個 descriptor（每個最多 4092 bytes）。

練習 1 的 C 部分（位元組讀 IRAM）要在實體板上才看得到例外，模擬器不一定模擬這個限制。練習 2 的時間也請以實體板為準。

雙緩衝的核心：

```c
for (int y = 0, k = 0; y < HEIGHT; y += LINES, k ^= 1) {
  render(buf[k], y);                         // CPU 算這一塊的同時，另一塊正在被 DMA 送出
  if (in_flight) {                           // 等上一塊送完，它的緩衝區才能再用
    spi_device_get_trans_result(spi, &done, portMAX_DELAY);
    in_flight--;
  }
  t[k].tx_buffer = buf[k];
  spi_device_queue_trans(spi, &t[k], portMAX_DELAY);   // 排進佇列，馬上返回
  in_flight++;
}
```

### 面試題與參考答案

**Q1. 什麼時候該用 DMA？什麼時候不該？**
大量、連續、規律的資料搬移（螢幕、音訊、ADC 連續取樣、高速通訊）用 DMA，CPU 可以同時做別的事，也省電。資料量很小、或每個位元組都要 CPU 判斷時不划算，設定 DMA 的開銷比直接搬還大。

**Q2. 什麼是 cache coherency 問題？怎麼處理？**
CPU 透過快取存取記憶體，DMA 直接存取 RAM，兩邊看到的內容可能不一樣。DMA 送出前 clean（寫回）、收進來後 invalidate（失效）；緩衝區對齊快取行、長度也是快取行的倍數；或把 DMA 緩衝區放在不經快取的區域。ESP32 的內部 SRAM 不經資料快取，ESP32-S3 用 PSRAM 或 Cortex-M7 時就要處理。

**Q3. 為什麼 DMA 緩衝區要對齊到快取行？**
invalidate 是以整條快取行為單位。如果緩衝區和別的變數共用一條快取行，invalidate 會把那個變數尚未寫回的修改一起丟掉，產生很難查的 bug。

**Q4. 這個 struct 的 sizeof 是多少？** `struct { uint8_t a; uint32_t b; uint8_t c; uint16_t d; }`
32 位元平台上是 12：a 在 0，補 3 bytes；b 在 4；c 在 8，補 1 byte；d 在 10；總長 12（已是 4 的倍數）。改成由大到小排列（b、d、a、c）就是 8。

**Q5. 什麼時候用 packed struct？有什麼風險？**
描述通訊封包、檔案格式、硬體暫存器排列，必須和外部格式一個位元組都不差時。風險是成員未對齊，存取變慢，在不支援未對齊存取的 CPU 上直接取成員的位址再用指標讀會出錯。另外還要處理位元組序（大小端）。

**Q6. 雙緩衝為什麼比較快？要注意什麼？**
它讓 CPU 準備資料和 DMA 傳輸同時進行，總時間從「計算 + 傳輸」變成大約「兩者較大者」。要注意 DMA 還在讀的緩衝區不能被 CPU 改寫，必須等傳輸完成（本課用 `spi_device_get_trans_result`）才能再填。

**Q7. 解析網路封包時讀一個 4 bytes 的欄位，怎麼寫才可攜？**
`memcpy(&v, p + offset, 4)`，再用 `ntohl` 或自己移位轉成主機位元組序。不要寫 `*(uint32_t *)(p + offset)`：可能未對齊、違反 strict aliasing，在某些 CPU 上會 fault。
