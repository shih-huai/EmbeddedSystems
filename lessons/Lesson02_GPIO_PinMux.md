# ESP32 韌體與驅動面試速成：第 2 課

## 第 2 課：GPIO 與 pin mux

一句話：ESP32 的一支腳要先經過 IO\_MUX 決定「這支腳給誰用」，再經過 GPIO matrix 決定「哪個訊號接到這支腳」，最後才輪到第 1 課寫的 GPIO\_OUT 暫存器。

### 概念

**一支腳的三層路徑**：

1. **IO\_MUX**：每支腳有一個 IO\_MUX 暫存器。`MCU_SEL` 選功能（值 2 = GPIO，IDF 叫 `PIN_FUNC_GPIO`），`FUN_IE` 是輸入致能，`FUN_WPU`／`FUN_WPD` 是上拉／下拉，`FUN_DRV` 是驅動強度（0–3，預設 2）。
2. **GPIO matrix**：`GPIO_FUNCn_OUT_SEL_CFG_REG` 決定第 n 支腳輸出哪個周邊訊號，填 256（`SIG_GPIO_OUT_IDX`）表示由 GPIO\_OUT 暫存器控制。輸入方向則用 `GPIO_FUNCy_IN_SEL_CFG_REG` 把某支腳接給某個周邊。所以 ESP32 的 UART、I2C 幾乎可以接到任意腳。
3. **GPIO 本體**：`GPIO_ENABLE` 決定輸出致能，`GPIO_OUT`／`GPIO_IN` 寫入與讀取電位，`GPIO_PINn_REG` 的 `PAD_DRIVER` 位元選 open-drain。

例外是高速訊號（SPI flash、HSPI／VSPI 跑 80 MHz）可以走 IO\_MUX 直連、跳過 matrix，因為 matrix 會多一段延遲。面試問「有 matrix 為什麼還要 IO\_MUX」就答這點。

**輸出模式**：

- **push-pull**：上下兩顆電晶體，能主動輸出高和低。LED、SPI 的片選線用這個。
- **open-drain**：只能拉低，高電位靠外部上拉電阻。I2C、多顆晶片共用的中斷線一定用這個：任何一顆拉低整條線就是低（wired-AND），也不會因為一顆輸出高、一顆輸出低而短路。

**輸入與上下拉**：沒接東西的輸入腳會隨雜訊亂跳，一定要上拉或下拉。ESP32 內建上拉約 45 kΩ，數值隨製程與溫度變動，需要精確時改用外部電阻。

**ESP32 腳位地雷**（面試與實務都會遇到）：

| 腳位 | 注意 |
| --- | --- |
| GPIO6–11 | 接內部 SPI flash，不要用 |
| GPIO34–39 | 只能輸入，沒有內建上下拉 |
| GPIO0、2、5、12、15 | strapping pin，開機瞬間的電位決定開機模式與 flash 電壓。GPIO12 開機時被拉高可能讓板子開不了機 |
| ADC2 的腳位 | Wi-Fi 開啟時不能用 ADC2 |

所以從這課開始 LED 改接 GPIO18、19、21，不再用 GPIO2。

**去彈跳（debounce）**：機械按鈕接觸的瞬間會在 1–10 ms 內抖動好幾次，直接計數會把一次按下算成好幾次。軟體作法是「電位穩定維持一段時間才承認」，用 4 個狀態的狀態機寫；硬體作法是 RC 濾波加施密特觸發器。

| 目前狀態 | 讀到的電位 | 下一個狀態 | 送出事件 |
| --- | --- | --- | --- |
| RELEASED | 按下 | MAYBE\_PRESSED（記下時間） |  |
| MAYBE\_PRESSED | 放開 | RELEASED（只是彈跳） |  |
| MAYBE\_PRESSED | 按下且已穩定 20 ms | PRESSED | PRESS |
| PRESSED | 放開 | MAYBE\_RELEASED（記下時間） |  |
| MAYBE\_RELEASED | 按下 | PRESSED（只是彈跳） |  |
| MAYBE\_RELEASED | 放開且已穩定 20 ms | RELEASED | RELEASE |

狀態機不讀腳位、不呼叫 `millis()`，電位和時間都由呼叫端傳進去。這樣它可以在電腦上做單元測試，這正是「測試與模擬方法」講的作法。

### Wokwi 練習 2：按鈕切換 3 種 LED 模式

程式專案在教材資料夾的 `Day02_GPIO_PinMux`，用法和 Day 01 一樣（PlatformIO 編譯、Wokwi 模擬）。電路是三顆 LED 各串 220Ω 接 GPIO18／19／21，按鈕接 GPIO4 到 GND。改 `main.cpp` 的 `EXERCISE`：

| 值 | 內容 |
| --- | --- |
| 0 | 沒有去彈跳，按一次看計數跳幾次（Wokwi 的按鈕有模擬彈跳） |
| 1 | 用去彈跳狀態機，一次按下只算一次 |
| 2 | 按鈕切換：全滅 → 跑馬燈 → 三顆一起 2 Hz 閃，全程不用 `delay()` |
| 3 | 不呼叫 `pinMode`，自己寫 IO\_MUX 與 GPIO matrix，印出和 `pinMode` 的結果對照 |

練習 3 的核心，就是 `pinMode(pin, OUTPUT)` 底下做的四件事：

```c
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"   // GPIO_PIN_MUX_REG[]：GPIO 編號 → IO_MUX 暫存器
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"  // SIG_GPIO_OUT_IDX = 256

static void reg_output_init(int pin) {   // 只適用 GPIO0～31
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);              // 1. IO_MUX：交給 GPIO
  REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin, SIG_GPIO_OUT_IDX);  // 2. matrix：由 GPIO_OUT 驅動
  REG_CLR_BIT(GPIO_PIN0_REG + 4 * pin, GPIO_PIN0_PAD_DRIVER);         // 3. push-pull
  REG_WRITE(GPIO_ENABLE_W1TS_REG, 1u << pin);                         // 4. 輸出致能
}
```

專案另附 `lib/debounce` 的單元測試（5 個案例，含按下彈跳、短暫雜訊、`millis()` 溢位繞回），可以用 `pio test -e native` 或直接用 gcc 在電腦上跑，步驟在專案的 README。燒到實體板時，可以把 `hold_ms` 從 20 改成 5 和 50，感受真實按鈕的彈跳時間。

### 面試題與參考答案

**Q1. push-pull 和 open-drain 差在哪？各用在哪？** push-pull 能主動輸出高和低，速度快，用在 LED、SPI。open-drain 只能拉低，高電位靠上拉電阻，所以多個裝置可以共用一條線而不短路，用在 I2C、共用中斷線，也能透過不同的上拉電壓做簡單的電位轉換。缺點是上升邊緣由 RC 決定，比較慢。

**Q2. 輸入腳為什麼要上拉或下拉？上拉電阻怎麼選？** 浮接的輸入是高阻抗，會被雜訊感應成隨機電位，還可能讓輸入級半導通而多耗電。電阻太大，上升時間慢、抗雜訊差；太小，按下時電流大、耗電。按鈕常用 10 kΩ。I2C 依匯流排電容和速度算，常見 2.2–4.7 kΩ。

**Q3. 什麼是按鈕彈跳？怎麼處理？** 機械接點閉合時會在數 ms 內反覆通斷。軟體可以用「穩定 N ms 才承認」的狀態機，或定時取樣、連續 N 次相同才算數；硬體可以用 RC 濾波加施密特觸發器。不要在 ISR 裡用 `delay()` 去彈跳，正確作法是 ISR 只記錄事件，交給 task 或計時器處理（第 3 課會做）。

**Q4. ESP32 的 IO\_MUX 和 GPIO matrix 是什麼？** IO\_MUX 決定每支腳的功能、上下拉和驅動強度。GPIO matrix 是一個交換器，讓周邊訊號可以接到幾乎任何腳。高速訊號走 IO\_MUX 直連以避開 matrix 的延遲。STM32 對應的是 alternate function（AF），每支腳只能選固定幾種周邊。

**Q5. 什麼是 strapping pin？設計電路時要注意什麼？** 晶片在 reset 放開的瞬間取樣這些腳的電位，決定開機模式（例如 GPIO0 拉低進下載模式）或 flash 電壓（GPIO12）。外接電路不能在開機時把它們拉到錯的電位，否則會出現「接上感測器就開不了機」的問題。

**Q6. 為什麼 `delay()` 不適合放在主迴圈裡做閃爍？** `delay()` 期間 CPU 什麼都不做，按鈕和其他工作都被延後。用「目前時間減上次時間」判斷是否該動作（非阻塞），主迴圈才能同時處理多件事。進一步就是用計時器中斷或 RTOS task（第 4、9 課）。

**Q7. `millis()` 大約 49.7 天後會溢位，你的計時程式會壞嗎？** 如果寫成 `millis() - last >= interval`，用無號整數相減，溢位繞回時差值仍然正確，不會壞。寫成 `millis() >= last + interval` 在繞回時就會出錯。本課的單元測試有一個案例專門測這件事。

**Q8. 為什麼要把去彈跳邏輯寫成不碰硬體的模組？** 把電位和時間當參數傳入，邏輯就和硬體分離，可以在電腦上做單元測試，也能重用在任何 MCU。這是驅動分層的基本概念：硬體存取集中在最底層，上層是可測試的純邏輯。

