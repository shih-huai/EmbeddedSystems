# ESP32 韌體與驅動面試速成

Oct 23, 2026 · @shih huai huang

## 怎麼用這份計畫

目標是 12 堂課內，把韌體／驅動面試最常考的觀念各配一個能在 Wokwi 跑起來的練習，再配上面試題與參考答案。你已經熟 C，所以這裡不教語法，只教「C 碰到硬體時」會出事的地方。

- **節奏**：一天一堂，每堂約 2–3 小時（概念 30 分、Wokwi 練習 60–90 分、面試題口頭練習 30 分）。時間更緊就一天兩堂，7 天跑完。
- **工具**：先在 [Wokwi ESP32](https://wokwi.com/esp32) 開 ESP32 專案驗證，再把同一份程式燒到你手上的開發板確認，兩邊結果不同時正好是面試好題材（時序、彈跳、電氣特性）。
- **框架**：前幾堂用 Arduino 範本寫最快，但每堂都會指出 ESP-IDF 對應的 API，因為面試官多半期待你懂 ESP-IDF／FreeRTOS 這一層。
- **面試題怎麼練**：先遮住答案自己講 1 分鐘，再對照。答案的重點是「為什麼」，不是背定義。

## 課程大綱

12 堂課由底層往上疊：暫存器 → 周邊 → 通訊協定 → RTOS → 系統層。前 8 堂是驅動面試的核心，時間不夠就優先把它們做完。

| # | 主題 | Wokwi 練習 | 面試必問 |
| --- | --- | --- | --- |
| 1 | 記憶體映射暫存器、volatile、位元操作 | 不用 digitalWrite，直接寫 GPIO 暫存器點燈、讀按鈕 | volatile 做什麼／不做什麼、const volatile、寫絕對位址 |
| 2 | GPIO 與 pin mux（IO\_MUX、GPIO matrix）、上拉／下拉、去彈跳 | 按鈕去彈跳狀態機，切換 3 種 LED 模式 | open-drain vs push-pull、為什麼要去彈跳 |
| 3 | 中斷與 ISR | GPIO 中斷計數按鈕，比較 polling 與中斷 | ISR 內不能做什麼、中斷延遲、共享變數保護、IRAM\_ATTR |
| 4 | 硬體計時器、PWM（LEDC）、Watchdog | 計時器中斷做精準 1 kHz tick，PWM 調光 | prescaler 計算、watchdog 用途與餵狗策略 |
| 5 | UART 與 ring buffer 驅動 | 自寫 ring buffer，做簡單指令列介面 | 框格格式、baud 誤差、lock-free ring buffer |
| 6 | I2C | 不用現成函式庫，自寫 MPU6050 驅動讀加速度 | START/STOP/ACK、上拉電阻、clock stretching、匯流排卡死怎麼解 |
| 7 | SPI | 驅動 ILI9341 螢幕畫圖 | CPOL/CPHA 四種模式、跟 I2C 怎麼選 |
| 8 | DMA、快取、對齊 | SPI + DMA 傳大量像素，量測 CPU 使用率差異 | cache coherency、DMA 緩衝區為什麼要對齊、double buffering |
| 9 | FreeRTOS（一）：task、排程、堆疊 | 三個不同優先權的 task，觀察搮占 | preemptive vs cooperative、stack overflow 偵測、雙核 affinity |
| 10 | FreeRTOS（二）：queue、semaphore、mutex | ISR → queue → task 的資料流，重現 priority inversion | mutex vs binary semaphore、priority inheritance、\*FromISR API |
| 11 | 開機流程、linker script、bootloader、OTA | 印出各區段位址、修改 partition table | reset 到 main 之間發生什麼、.data/.bss、OTA 回滾 |
| 12 | 低功耗 + 驅動架構（HAL）+ 模擬面試 | deep sleep 加 GPIO／計時器喚醒，把前面驅動重構成 HAL | 睡眠模式差異、驅動分層、怎麼設計可測試的驅動 |

## 第 1 課：記憶體映射暫存器與 volatile

一句話：在 MCU 上，「控制硬體」就是「讀寫特定位址的記憶體」，而 volatile 是告訴編譯器「這個位址每次都要真的去讀寫」。

### 概念

**記憶體映射 I/O（MMIO）**：周邊（GPIO、UART、計時器…）的控制暫存器被放在位址空間裡。寫入 ESP32 的 `0x3FF44008` 不是存資料，而是讓某幾支腳輸出高電位。位址從晶片的 Technical Reference Manual（TRM）查，面試時要能說出「我會去查 TRM 的 register map」。

**為什麼需要 volatile**：編譯器假設記憶體只有程式會改。對一個普通指標讀兩次，它可能只讀一次；連寫兩次，可能只留最後一次。但硬體暫存器會自己變（狀態位元），而且「寫」本身就有副作用。volatile 強制每一次存取都照程式碼發生，且順序不變（相對於其他 volatile 存取）。

**volatile 不做的事**（面試加分點）：

- 不保證原子性：`count++` 仍是讀–改–寫三步，可能被中斷打斷。
- 不是記憶體屏障：不阻止 CPU 或快取重排，也不管非 volatile 變數的順序。多核（ESP32 是雙核）共享資料要用 mutex、critical section 或 C11 atomics。

**位元操作三件套**：

```c
reg |=  (1u << n);   // set
reg &= ~(1u << n);   // clear
reg ^=  (1u << n);   // toggle
if (reg & (1u << n)) // test
```

上面這些在暫存器上是「讀–改–寫」，如果中間被 ISR 或另一個核心改了同一個暫存器，就會覆蓋掉對方的修改。所以 ESP32 提供 **W1TS（write 1 to set）／W1TC（write 1 to clear）** 暫存器：只寫不讀，寫 1 的位元才有作用，天生原子。STM32 的 BSRR 是同樣的設計。

### Wokwi 練習 1：直接寫暫存器點燈

在 Wokwi 開一個 ESP32（Arduino）專案，放一顆 LED（串 220Ω 到 GPIO2）和一個按鈕（GPIO4 到 GND）。以下位址來自 ESP32 TRM 的 GPIO 章節，適用於原版 ESP32（Wokwi 預設的 DevKit V1）；S3、C3 位址不同。

```c
#include <Arduino.h>

#define GPIO_BASE            0x3FF44000u
#define REG32(off)           (*(volatile uint32_t *)(GPIO_BASE + (off)))
#define GPIO_OUT_W1TS        REG32(0x08)
#define GPIO_OUT_W1TC        REG32(0x0C)
#define GPIO_ENABLE_W1TS     REG32(0x24)
#define GPIO_IN              REG32(0x3C)

const int LED = 2, BTN = 4;

void setup() {
  pinMode(LED, OUTPUT);        // 先讓 IO_MUX 把腳位交給 GPIO（第 2 課會拆解）
  pinMode(BTN, INPUT_PULLUP);
  GPIO_ENABLE_W1TS = (1u << LED);
}

void loop() {
  bool pressed = !(GPIO_IN & (1u << BTN));   // 上拉，按下為 0
  if (pressed) GPIO_OUT_W1TS = (1u << LED);
  else         GPIO_OUT_W1TC = (1u << LED);
}
```

完成後試三件事：

1. 按住按鈕時 LED 以 2 Hz 閃爍，只用 W1TS／W1TC，不用 `^=`。
2. 拿掉 `REG32` 裡的 `volatile`，再寫一個 `while (!(GPIO_IN & (1u << BTN)));` 等待放開的迴圈，觀察行為是否改變（編譯器可能把讀取提到迴圈外，變成死迴圈）。
3. 把自寫的巨集改成 ESP-IDF 的 `#include "soc/gpio_reg.h"` 與 `REG_WRITE(GPIO_OUT_W1TS_REG, ...)`，確認位址一致。面試時說得出「IDF 的 `gpio_set_level()` 底下就是寫這個暫存器」很加分。

做完後燒到你的實體板。很多 DevKit 板上的藍色 LED 就接在 GPIO2，可以不接線。

### 面試題與參考答案

**Q1. volatile 是什麼？什麼時候用？** 告訴編譯器這個物件可能被程式以外的東西改變，每次存取都要真的發生，不可快取在暫存器、不可合併或刪除。三個典型場景：記憶體映射暫存器、ISR 與主程式共享的變數、被其他執行緒或 DMA 修改的記憶體。要主動補充：它不保證原子性，也不是記憶體屏障。

**Q2. 一個變數可以同時是 const 和 volatile 嗎？** 可以。`const` 是「程式不該寫它」，`volatile` 是「它自己會變」。例子是唯讀的狀態暫存器：`const volatile uint32_t *status`。

**Q3. `volatile int *p`、`int * volatile p`、`volatile int * volatile p` 差在哪？** 第一個是「指向的資料」會變（暫存器指標的常見寫法）；第二個是「指標本身」會變，例如被 ISR 更新的 buffer 指標；第三個兩者皆是。讀法是從變數名往左讀。

**Q4. 把 0xAA55 寫入絕對位址 0x67A9（一個 int）。** `*(volatile int *)0x67A9 = 0xAA55;`。加分點：指出 0x67A9 不是 4 位元組對齊，在很多架構（包括 ESP32 的 Xtensa）上會觸發例外；實務上暫存器位址一定是對齊的。

**Q5. 下面這個函式有什麼問題？** `int square(volatile int *p) { return *p * *p; }` 兩次讀取之間值可能改變，結果不一定是平方。修法：先讀到區域變數 `int v = *p; return v * v;`。

**Q6. 為什麼 ESP32／STM32 要提供 W1TS／W1TC（BSRR）這種暫存器？** `GPIO_OUT |= mask` 是讀–改–寫。如果讀完之後、寫回之前發生中斷，且 ISR 改了同一個暫存器的其他位元，寫回時就把 ISR 的修改蓋掉了。W1TS／W1TC 是單一寫入，只影響寫 1 的位元，不需關中斷。

**Q7. 用 struct bit-field 描述暫存器好嗎？** 可讀性好，ESP-IDF 自己的 `gpio_struct.h` 就這樣做。但 bit-field 的位元順序、填充、存取寬度是實作定義的，換編譯器可能不同；而且改一個欄位仍是讀–改–寫。跨平台的程式用 mask + shift 巨集較安全。

**Q8. 不用暫存器也可以用 digitalWrite，為什麼要懂這些？** 驅動工程師的工作就是寫 digitalWrite 這一層。加上效能（digitalWrite 有查表與檢查，直接寫暫存器是一個 store 指令）、除錯（用除錯器看暫存器值確認硬體狀態）、以及把新晶片帶起來時沒有現成函式庫可用。

## 測試與模擬方法

面試常問「你怎麼測試韌體？」，好的答案是分三層：邏輯在主機上測、整合在模擬器上測、時序與電氣在實體板上測。

| 層級 | 工具 | 測什麼 | 抓不到什麼 |
| --- | --- | --- | --- |
| 主機端單元測試 | gcc + [Unity](https://github.com/ThrowTheSwitch/Unity)（或 Ceedling、CMock）；ESP-IDF 也有 linux target | 狀態機、ring buffer、協定解析、驅動對暫存器寫了什麼 | 真實時序、中斷競爭 |
| 模擬器 | [Wokwi](https://wokwi.com/esp32)（網頁、VS Code 擴充、wokwi-cli 可放進 CI） | 整個韌體跑在模擬 ESP32 上，搭配模擬感測器、邏輯分析儀 | 電氣特性、按鈕彈跳（模擬的按鈕可設定彈跳但不完全等於實物） |
| 實體板 | 你的 ESP32 + 邏輯分析儀或示波器 | 時序、功耗、電氣、長時間穩定性 | 自動化較難 |

**讓驅動可以在主機上測的關鍵**：不要把暫存器位址寫死在程式裡。兩種常見做法：

1. 把暫存器基址當參數傳入，測試時指向一塊 RAM 陣列（fake registers），執行驅動後檢查陣列內容。
2. 驅動只依賴一個 HAL 介面（一組函式指標），測試時換成 mock。例如 MPU6050 驅動只呼叫 `i2c->write_read()`，測試時用 mock 回傳預設的位元組。

```c
typedef struct {
  int (*write_read)(void *ctx, uint8_t addr,
                    const uint8_t *tx, size_t txlen,
                    uint8_t *rx, size_t rxlen);
  void *ctx;
} i2c_bus_t;

int mpu6050_read_accel(const i2c_bus_t *bus, int16_t out[3]);
```

你做過遊戲框架，這就是你熟悉的「平台抽象層」思路，面試時可以直接拿來類比。第 5、6 課會實際用這個模式寫 ring buffer 與 MPU6050 驅動，並配一份 Unity 測試。
