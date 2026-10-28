# ESP32 韌體與驅動面試速成：第 12 課

## 第 12 課：低功耗、HAL 重構、總複習

一句話：低功耗的核心是「做完就睡，能睡多深就睡多深」；好的驅動架構讓應用邏輯只透過 HAL 碰硬體，才能在電腦上測試、換晶片時只改一層。

### 概念

**ESP32 的電源模式**（數字來自 datasheet，指晶片本身，開發板上的 USB 晶片和 LED 會多吃很多）：

| 模式 | 還在動的 | 約略電流 | 醒來後 |
| --- | --- | --- | --- |
| Active | CPU、Wi-Fi／BT | 數十到兩百多 mA | — |
| Modem sleep | CPU，無線關閉 | 約 20–70 mA | — |
| Light sleep | CPU 暫停，RAM 保留 | 約 0.8 mA | 從呼叫的下一行繼續 |
| Deep sleep | 只剩 RTC 和 RTC 記憶體 | 約 10 µA | 從頭開機，只有 RTC 記憶體保留 |
| Hibernation | 只剩 RTC 計時器 | 約 5 µA | 從頭開機 |

**喚醒來源**：RTC 計時器、ext0（單一 RTC GPIO 的電位）、ext1（多個 RTC GPIO 的 AND／OR）、觸控、ULP 協處理器；light sleep 還可以用一般 GPIO 和 UART。只有 RTC GPIO（例如 GPIO0、2、4、12–15、25–27、32–39）能在深度睡眠時喚醒。

**平均電流怎麼算**：一個每 10 分鐘醒來 2 秒、醒著時 80 mA、睡著時 10 µA 的裝置，平均電流 ≈ (80 mA × 2 + 0.01 mA × 598) / 600 ≈ 0.28 mA。降低平均耗電通常靠「醒得更短」和「醒得更少」，而不是把睡眠電流再壓低。

**低功耗設計清單**：關掉不用的周邊和時脈；Wi-Fi 能不開就不開，開了就用省電模式；感測器也要進低功耗模式或斷電；浮接的腳位設成固定電位避免漏電；睡前把 UART 送完；量測時排除開發板上其他元件的電流。

**HAL 分層**：

| 層 | 內容 | 換晶片時 |
| --- | --- | --- |
| 應用邏輯 | 狀態機、演算法 | 不用改，可以在電腦上測試 |
| 裝置驅動 | MPU6050、ILI9341 這類外部晶片 | 不用改，只依賴匯流排 HAL |
| HAL | `i2c_write_read`、`gpio_write`、`now_ms` 這類函式指標或函式介面 | 重寫這一層 |
| 晶片 SDK／暫存器 | ESP-IDF、暫存器操作 | 換掉 |

這套課程的每個驅動（第 5 課的環形緩衝區、第 6 課的 MPU6050、第 7 課的 ILI9341）都是這樣寫的，所以都能在電腦上用假的匯流排測試。

### Wokwi 練習 12：睡眠和總複習應用

程式專案是 `Day12_LowPower_HAL_Capstone`。LED 接 GPIO18，按鈕接 GPIO33（RTC GPIO）到 GND，可變電阻中間腳接 GPIO34。應用邏輯放在 `lib/sensor_app`（沿用第 10 課的 `lib/debounce`），附 5 個單元測試。

| EXERCISE | 內容 |
| --- | --- |
| 0 | 深度睡眠 2 秒、計時器喚醒，重複 3 次；`RTC_DATA_ATTR` 的計數和睡前時間會保留 |
| 1 | 深度睡眠，按按鈕（ext0）或 10 秒到期喚醒，分辨喚醒原因 |
| 2 | 淺度睡眠 500 ms × 5 次，按鈕可提早叫醒；程式從下一行繼續、區域變數還在 |
| 3 | 總複習：按鈕切換取樣（每 100 ms 讀可變電阻、每 10 筆回報），閒置 10 秒深度睡眠，按鈕叫醒 |

驗證狀況：`lib/sensor_app` 的 5 個單元測試在電腦上用假 HAL 和模擬時間跑過，全部通過；四個練習都編譯過、0 warning。ESP32 模擬器（QEMU）沒有模擬睡眠電路，`esp_deep_sleep_start` 在 QEMU 裡會當機，所以睡眠部分要在 Wokwi 或實機上看。

```c
typedef struct {
  uint32_t (*now_ms)(void *ctx);
  int  (*read_button)(void *ctx);
  int  (*read_sensor)(void *ctx);
  void (*set_led)(void *ctx, int on);
  void (*report)(void *ctx, int avg, int min, int max);
  void *ctx;
} sensor_hal_t;                       // 應用邏輯只看得到這些

void loop() {
  if (app_step(&app) == APP_SLEEP) {  // 應用邏輯決定「可以睡了」，怎麼睡由平台決定
    Serial.flush();
    rtc_gpio_pullup_en(GPIO_NUM_33);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0);
    esp_deep_sleep_start();
  }
  delay(5);
}
```

### 面試題與參考答案

**Q1. 一個電池供電的感測器，要怎麼讓它撐一年？**
先算預算：電池容量除以 8760 小時得到允許的平均電流。然後讓裝置大部分時間深度睡眠，用 RTC 計時器定時醒來，醒來後快速量測、批次累積資料，久久才開一次 Wi-Fi 上傳。感測器和周邊在不用時斷電或進低功耗模式，用實際量測（電流探棒、功率分析儀）驗證每個階段的電流和時間。

**Q2. light sleep 和 deep sleep 怎麼選？**
light sleep 保留 RAM 和 CPU 狀態，醒來快（約 1 ms 內）、程式從原處繼續，適合頻繁短暫休息，例如等下一個 UART 字元或 Wi-Fi 的 DTIM 間隔。deep sleep 耗電低兩個數量級，但醒來要重新開機（數十到數百 ms），只有 RTC 記憶體保留，適合長時間休息。

**Q3. 深度睡眠醒來後怎麼知道之前的狀態？**
用 `RTC_DATA_ATTR` 存少量狀態（計數、上次量測值、要不要上傳），用 `esp_sleep_get_wakeup_cause` 判斷是被誰叫醒，大量資料存在 NVS 或 flash。注意 RTC 記憶體只有 8 KB，而且在上電或一般 reset 時會被重新初始化。

**Q4. 你怎麼設計一個可移植、可測試的驅動？**
驅動只依賴一組 HAL 函式（例如 `bus_write_read`、`delay_ms`），透過函式指標或連結時替換注入；驅動自己不呼叫 SDK。狀態放在 handle 結構裡，不用全域變數，所以可以有多個實例。錯誤用回傳值傳出。單元測試用假的匯流排記錄每一筆交易、模擬錯誤和逾時。

### 總複習：模擬面試速答

面試前把下面每一題用 30 秒講完，講不出來就回去看那一課。

| 問題 | 重點 | 課 |
| --- | --- | --- |
| `volatile` 解決什麼、不解決什麼？ | 每次真的讀寫記憶體；不保證原子性和順序 | 1 |
| 怎麼只改暫存器的某幾個 bit？ | 讀－改－寫，或用 W1TS／W1TC 這類只寫 1 生效的暫存器 | 1、2 |
| 推挽和開汲極差在哪？ | 開汲極只能拉低，可線與，I2C 用它 | 2 |
| ISR 要注意什麼？ | 短、不阻塞、不 printf、共享變數用 volatile 加 critical section、放 IRAM | 3 |
| race condition 怎麼避免？ | critical section、atomic、mutex，或只讓一方寫 | 3、10 |
| PWM 頻率和解析度的關係？ | 時脈 = 頻率 × 2^解析度，頻率越高解析度越低 | 4 |
| watchdog 怎麼用才對？ | 在確認系統健康的地方餵，不在計時器中斷裡無條件餵 | 4 |
| UART 收資料怎麼不掉？ | ISR 放進環形緩衝區，task 處理；溢位要能偵測 | 5 |
| I2C 的 ACK／NACK、clock stretching、卡死怎麼救？ | 9 個 clock 解鎖、STOP，再重新初始化 | 6 |
| SPI 四種模式？ | CPOL、CPHA，看 datasheet 的時序圖 | 7 |
| DMA 要注意什麼？ | 記憶體能不能被 DMA 存取、對齊、快取一致性、緩衝區生命週期 | 8 |
| 結構 padding 和 packed 的取捨？ | 對齊存取快；packed 省空間但可能不對齊存取 | 8 |
| `vTaskDelay` vs `vTaskDelayUntil`？ | 相對時間會漂移，絕對時間不會 | 9 |
| 優先權反轉？ | 用 mutex 的優先權繼承 | 10 |
| 從上電到 main？ | ROM → bootloader → 搬 segment → 清 .bss → constructor → main | 11 |
| OTA 怎麼不變磚？ | A/B 分割區、驗證、切換、自我檢查、rollback | 11 |
| 電池裝置怎麼省電？ | 深度睡眠、醒得短醒得少、算平均電流 | 12 |

**講專案經驗的方法**：每個專案準備一個 2 分鐘版本，依序講背景（做什麼產品、你負責哪塊）、遇到的困難（例如偶發當機、資料遺失、耗電太高）、你怎麼查（用了哪些工具、量了什麼數據）、結果（用數字說，例如「遺失率從 2% 降到 0」「待機電流從 3 mA 降到 40 µA」）。你做過完整的遊戲框架，可以講狀態機設計、固定時間步長的主迴圈、資源管理和效能分析，這些和韌體的 super loop、排程、記憶體配置、量測方法是同一套思維。

**除錯題的回答框架**：先重現並縮小範圍，再收集證據（log、reset 原因、core dump、示波器或邏輯分析儀），提出假設、一次只改一個變數驗證，修好後加測試或監控防止再發生。
