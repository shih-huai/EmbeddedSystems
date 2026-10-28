/*
 * Day 08：DMA、快取、對齊
 *
 * 螢幕接線和 Day 07 一樣：SCK=18、MOSI=23、MISO=19、CS=17、D/C=16、RST=4。
 * 這一課改用 ESP-IDF 的 spi_master 驅動，才能控制 DMA。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：struct 的對齊與 padding，印出 sizeof 與 offsetof
 *   1 = 練習 1 實驗：memcpy 安全讀未對齊資料；指標強制轉型讀 DRAM 未對齊位址；位元組讀 IRAM 觸發例外（會重開機）
 *   2 = 練習 2 參考解：畫漸層畫面，比較「算一段、送一段（等待）」和「DMA 雙緩衝（邊算邊送）」
 *   3 = 練習 3 參考解：哪些記憶體可以給 DMA 用？檢查堆疊、全域、heap、flash 常數的位址
 */
#include <Arduino.h>
#include <stddef.h>
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "soc/soc_memory_types.h"
#include "ili9341.h"
#include "dma_plan.h"

#define EXERCISE 0

const int PIN_SCK = 18, PIN_MISO = 19, PIN_MOSI = 23, PIN_CS = 17, PIN_DC = 16, PIN_RST = 4;

#if EXERCISE == 0
struct Bad {        // 成員順序沒想過
  uint8_t  a;
  uint32_t b;
  uint8_t  c;
  uint16_t d;
};
struct Good {       // 由大到小排，padding 最少
  uint32_t b;
  uint16_t d;
  uint8_t  a;
  uint8_t  c;
};
struct __attribute__((packed)) Packed {   // 完全不補，常用在通訊協定的封包格式
  uint8_t  a;
  uint32_t b;
  uint8_t  c;
  uint16_t d;
};

void setup() {
  Serial.begin(115200);
  Serial.println("Day 08 EXERCISE 0：struct 對齊");
  Serial.printf("Bad    sizeof=%u  offsetof a=%u b=%u c=%u d=%u\n", sizeof(Bad),
                offsetof(Bad, a), offsetof(Bad, b), offsetof(Bad, c), offsetof(Bad, d));
  Serial.printf("Good   sizeof=%u  offsetof b=%u d=%u a=%u c=%u\n", sizeof(Good),
                offsetof(Good, b), offsetof(Good, d), offsetof(Good, a), offsetof(Good, c));
  Serial.printf("Packed sizeof=%u  offsetof a=%u b=%u c=%u d=%u\n", sizeof(Packed),
                offsetof(Packed, a), offsetof(Packed, b), offsetof(Packed, c), offsetof(Packed, d));
  Packed p = {1, 0x11223344, 2, 0x5566};
  // 讀 packed struct 的成員，編譯器知道它可能沒對齊，會改用逐位元組讀取，所以是安全的（但比較慢）
  Serial.printf("Packed.b = 0x%08lX\n", (unsigned long)p.b);
}
void loop() { delay(1000); }

#elif EXERCISE == 1
// 從一包資料的第 1 個位元組開始讀一個 uint32_t（例如解析通訊封包時常見）
static uint8_t packet[8] __attribute__((aligned(4))) = {0xAA, 0x44, 0x33, 0x22, 0x11, 0, 0, 0};

static uint32_t read_u32_safe(const uint8_t *p) {
  uint32_t v;
  memcpy(&v, p, sizeof(v));       // 正確做法：memcpy，編譯器會產生在任何 CPU 上都安全的讀法
  return v;
}

static void IRAM_ATTR some_iram_function() { }

void setup() {
  Serial.begin(115200);
  Serial.println("Day 08 EXERCISE 1：對齊實驗");
  Serial.printf("A. memcpy 讀未對齊位址：0x%08lX\n", (unsigned long)read_u32_safe(&packet[1]));

  // ESP32 的內部 DRAM 由硬體處理未對齊讀取，所以這行在 ESP32 上「剛好」能動（比較慢）。
  // 但同樣的程式在 Cortex-M0 上會 HardFault，在 C 語言裡也是未定義行為，不要這樣寫。
  volatile uint32_t *unaligned = (volatile uint32_t *)&packet[1];
  Serial.printf("B. 指標強制轉型讀 DRAM 未對齊位址：0x%08lX（ESP32 可以，換晶片就不一定）\n",
                (unsigned long)*unaligned);

  // ESP32 的 IRAM（0x4008xxxx）只接受 32 位元、4 bytes 對齊的存取。用位元組去讀，CPU 會丟出例外。
  Serial.println("C. 用位元組讀 IRAM，CPU 會丟出 LoadStoreError 例外並重開機...");
  delay(100);
  volatile uint8_t *iram = (volatile uint8_t *)&some_iram_function;
  Serial.printf("讀到 0x%02X（不應該看到這行）\n", *iram);
}
void loop() { delay(1000); }

#else
// ---- 練習 2、3 共用：spi_master + DMA ----
static spi_device_handle_t spi;
static volatile int dc_level = 0;

// 每個 SPI 傳輸開始前，驅動在中斷裡呼叫這個函式設定 D/C 線
static void IRAM_ATTR pre_cb(spi_transaction_t *t) {
  gpio_set_level((gpio_num_t)PIN_DC, (int)(intptr_t)t->user);
}

// 給 ili9341 驅動用的匯流排：CS 交給 spi_master 自動控制，寫入用 polling（小資料，等它做完）
static void bus_cs(void *, int) {}
static void bus_dc(void *, int level) { dc_level = level; }
static void bus_write(void *, const uint8_t *buf, size_t len) {
  spi_transaction_t t = {};
  t.length = len * 8;               // 單位是位元
  t.tx_buffer = buf;
  t.user = (void *)(intptr_t)dc_level;
  spi_device_polling_transmit(spi, &t);
}
static void bus_delay(void *, uint32_t ms) { delay(ms); }
static const lcd_bus_t bus = {bus_cs, bus_dc, bus_write, bus_delay, nullptr};
static ili9341_t lcd;

__attribute__((unused)) static void lcd_setup() {
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);  delay(10);
  digitalWrite(PIN_RST, HIGH); delay(120);

  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = PIN_MOSI;
  buscfg.miso_io_num = PIN_MISO;
  buscfg.sclk_io_num = PIN_SCK;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 8192;                 // 一次傳輸最多 8 KB，驅動會自動串好多個 descriptor
  ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t devcfg = {};
  devcfg.clock_speed_hz = 40 * 1000 * 1000;
  devcfg.mode = 0;
  devcfg.spics_io_num = PIN_CS;
  devcfg.queue_size = 4;                         // 最多同時排 4 個傳輸
  devcfg.pre_cb = pre_cb;
  ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
  ili9341_init(&lcd, &bus);
}

#if EXERCISE == 2
static const int LINES = 16;                                   // 一次處理 16 行
static const size_t BUF_BYTES = ILI9341_WIDTH * LINES * 2;     // 7,680 bytes

// 產生一段漸層像素（故意算得有點慢，模擬真正的繪圖工作）
static void render(uint8_t *buf, int y0, uint32_t frame) {
  for (int y = 0; y < LINES; y++)
    for (int x = 0; x < ILI9341_WIDTH; x++) {
      uint16_t c = rgb565((x + frame) & 0xFF, ((y0 + y) * 255 / ILI9341_HEIGHT), (x * y) & 0xFF);
      size_t i = ((size_t)y * ILI9341_WIDTH + x) * 2;
      buf[i] = c >> 8;
      buf[i + 1] = c & 0xFF;
    }
}

static uint8_t *buf[2];

static uint32_t frame_blocking(uint32_t frame) {
  uint32_t t0 = micros();
  ili9341_begin_ramwr(&lcd, 0, 0, ILI9341_WIDTH, ILI9341_HEIGHT);
  for (int y = 0; y < ILI9341_HEIGHT; y += LINES) {
    render(buf[0], y, frame);                    // CPU 算
    spi_transaction_t t = {};
    t.length = BUF_BYTES * 8;
    t.tx_buffer = buf[0];
    t.user = (void *)1;
    spi_device_transmit(spi, &t);                // 然後乾等 DMA 送完
  }
  return micros() - t0;
}

static uint32_t frame_double_buffer(uint32_t frame) {
  uint32_t t0 = micros();
  ili9341_begin_ramwr(&lcd, 0, 0, ILI9341_WIDTH, ILI9341_HEIGHT);
  spi_transaction_t t[2] = {};
  int in_flight = 0;
  for (int y = 0, k = 0; y < ILI9341_HEIGHT; y += LINES, k ^= 1) {
    render(buf[k], y, frame);                    // 算這一塊的同時，另一塊正在被 DMA 送出
    if (in_flight) {                             // 等上一塊送完，才能再排新的
      spi_transaction_t *done;
      spi_device_get_trans_result(spi, &done, portMAX_DELAY);
      in_flight--;
    }
    t[k] = {};
    t[k].length = BUF_BYTES * 8;
    t[k].tx_buffer = buf[k];
    t[k].user = (void *)1;
    spi_device_queue_trans(spi, &t[k], portMAX_DELAY);   // 排進佇列，馬上返回
    in_flight++;
  }
  while (in_flight--) {
    spi_transaction_t *done;
    spi_device_get_trans_result(spi, &done, portMAX_DELAY);
  }
  return micros() - t0;
}

void setup() {
  Serial.begin(115200);
  lcd_setup();
  // DMA 緩衝區必須在內部 RAM，而且 4 bytes 對齊
  for (int i = 0; i < 2; i++) buf[i] = (uint8_t *)heap_caps_malloc(BUF_BYTES, MALLOC_CAP_DMA);
  Serial.printf("Day 08 EXERCISE 2：緩衝區 %p、%p，DMA 可用=%d%d，4 bytes 對齊=%d%d\n",
                buf[0], buf[1], esp_ptr_dma_capable(buf[0]), esp_ptr_dma_capable(buf[1]),
                dma_is_aligned(buf[0], 4), dma_is_aligned(buf[1], 4));
}

void loop() {
  static uint32_t frame = 0;
  uint32_t a = frame_blocking(frame++);
  uint32_t b = frame_double_buffer(frame++);
  Serial.printf("一塊一塊等：%lu ms   DMA 雙緩衝：%lu ms\n", (unsigned long)(a / 1000), (unsigned long)(b / 1000));
  delay(500);
}

#elif EXERCISE == 3
static uint8_t global_buf[64];
static const uint8_t flash_table[64] = {1, 2, 3};   // const 全域陣列會放在 flash（.rodata）

static void check(const char *name, const void *p) {
  Serial.printf("  %-22s %p  DMA 可用=%d  內部 RAM=%d  4 bytes 對齊=%d\n", name, p,
                esp_ptr_dma_capable(p), esp_ptr_internal(p), dma_is_aligned(p, 4));
}

void setup() {
  Serial.begin(115200);
  uint8_t stack_buf[64];
  void *heap_any = malloc(64);
  void *heap_dma = heap_caps_malloc(64, MALLOC_CAP_DMA);
  Serial.println("Day 08 EXERCISE 3：哪些記憶體可以給 DMA？");
  check("堆疊上的陣列", stack_buf);
  check("全域陣列", global_buf);
  check("malloc", heap_any);
  check("heap_caps_malloc(DMA)", heap_dma);
  check("flash 裡的 const 陣列", flash_table);
  check("未對齊的位址", (uint8_t *)heap_dma + 1);
  Serial.printf("可給 DMA 用的 heap 剩 %u bytes，最大一塊 %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));

  size_t chunks[64];
  size_t n = dma_plan(153600, 4095, 4, chunks, 64);
  Serial.printf("一整個畫面 153,600 bytes 要 %u 個 DMA descriptor（每個最多 4092 bytes）\n", n);
  free(heap_any);
  heap_caps_free(heap_dma);
}
void loop() { delay(1000); }
#endif
#endif
