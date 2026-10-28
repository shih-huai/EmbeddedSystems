/*
 * Day 07：SPI 與 ILI9341 螢幕驅動
 *
 * ILI9341 接 VSPI：SCK=GPIO18、MOSI=GPIO23、MISO=GPIO19，CS=GPIO17、D/C=GPIO16、RST=GPIO4，
 * VCC 與 LED（背光）接 3.3V、GND 接 GND。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：初始化螢幕，輪流填滿紅綠藍，印出每次填滿要幾 ms
 *   1 = 練習 1 參考解：比較 SPI 時脈 10、20、40 MHz 的填滿時間
 *   2 = 練習 2 參考解：同一塊 100x100，一個像素一個像素畫 vs 一次 fill_rect，時間差多少
 *   3 = 練習 3 參考解：彈跳方塊動畫（只擦掉舊位置、畫新位置，不重畫整個畫面）
 */
#include <Arduino.h>
#include <SPI.h>
#include "ili9341.h"

#define EXERCISE 0

const int PIN_SCK = 18, PIN_MISO = 19, PIN_MOSI = 23, PIN_CS = 17, PIN_DC = 16, PIN_RST = 4;

static uint32_t spi_hz = 40000000;

// ---- 把 Arduino SPI 包成驅動要的 lcd_bus_t ----
static void bus_cs(void *, int level) {
  if (level == 0) {
    SPI.beginTransaction(SPISettings(spi_hz, MSBFIRST, SPI_MODE0));   // ILI9341 用 mode 0
    digitalWrite(PIN_CS, LOW);
  } else {
    digitalWrite(PIN_CS, HIGH);
    SPI.endTransaction();
  }
}
static void bus_dc(void *, int level) { digitalWrite(PIN_DC, level); }
static void bus_write(void *, const uint8_t *buf, size_t len) { SPI.writeBytes(buf, len); }
static void bus_delay(void *, uint32_t ms) { delay(ms); }
static const lcd_bus_t spi_bus = {bus_cs, bus_dc, bus_write, bus_delay, nullptr};

static ili9341_t lcd;

static void lcd_hw_setup() {
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_RST, LOW);    // 硬體重置：拉低至少 10 µs
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(120);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, -1);   // CS 自己控制，所以傳 -1
  ili9341_init(&lcd, &spi_bus);
}

__attribute__((unused)) static uint32_t time_fill(uint16_t color) {
  uint32_t t0 = micros();
  ili9341_fill_screen(&lcd, color);
  return micros() - t0;
}

#if EXERCISE == 0
void setup() {
  Serial.begin(115200);
  lcd_hw_setup();
  Serial.println("Day 07 EXERCISE 0：輪流填滿紅綠藍");
}
void loop() {
  const uint16_t colors[] = {rgb565(255, 0, 0), rgb565(0, 255, 0), rgb565(0, 0, 255)};
  for (uint16_t c : colors) {
    Serial.printf("填滿 0x%04X：%lu ms\n", c, (unsigned long)(time_fill(c) / 1000));
    delay(500);
  }
}

#elif EXERCISE == 1
void setup() {
  Serial.begin(115200);
  lcd_hw_setup();
  Serial.println("Day 07 EXERCISE 1：SPI 時脈 vs 填滿時間（240x320x2 = 153,600 bytes）");
  const uint32_t clocks[] = {10000000, 20000000, 40000000};
  for (uint32_t hz : clocks) {
    spi_hz = hz;
    uint32_t us = time_fill(rgb565(hz / 160000, 64, 128));
    // 理論時間 = 位元數 / 時脈
    Serial.printf("  %2lu MHz：實際 %lu ms，理論 %lu ms\n", (unsigned long)(hz / 1000000),
                  (unsigned long)(us / 1000), (unsigned long)(153600ULL * 8 * 1000 / hz));
  }
}
void loop() { delay(1000); }

#elif EXERCISE == 2
void setup() {
  Serial.begin(115200);
  lcd_hw_setup();
  ili9341_fill_screen(&lcd, 0);
  uint32_t t0 = micros();
  for (int y = 0; y < 100; y++)
    for (int x = 0; x < 100; x++) ili9341_draw_pixel(&lcd, 20 + x, 20 + y, rgb565(255, 128, 0));
  uint32_t per_pixel = micros() - t0;
  t0 = micros();
  ili9341_fill_rect(&lcd, 120, 20, 100, 100, rgb565(0, 128, 255));
  uint32_t bulk = micros() - t0;
  Serial.printf("Day 07 EXERCISE 2：逐點 %lu ms，整塊 %lu ms，差 %lu 倍\n",
                (unsigned long)(per_pixel / 1000), (unsigned long)(bulk / 1000),
                (unsigned long)(per_pixel / (bulk ? bulk : 1)));
}
void loop() { delay(1000); }

#elif EXERCISE == 3
void setup() {
  Serial.begin(115200);
  lcd_hw_setup();
  ili9341_fill_screen(&lcd, 0);
  Serial.println("Day 07 EXERCISE 3：彈跳方塊");
}
void loop() {
  static int x = 10, y = 10, dx = 3, dy = 2;
  const int S = 30;
  ili9341_fill_rect(&lcd, x, y, S, S, 0);                 // 擦掉舊的
  x += dx; y += dy;
  if (x <= 0 || x + S >= ILI9341_WIDTH) dx = -dx;
  if (y <= 0 || y + S >= ILI9341_HEIGHT) dy = -dy;
  ili9341_fill_rect(&lcd, x, y, S, S, rgb565(255, 220, 0)); // 畫新的
  delay(16);                                              // 約 60 fps
}
#endif
