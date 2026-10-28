/*
 * Day 12：低功耗、HAL 重構、總複習
 *
 * LED：GPIO18（串 220Ω 到 GND）
 * 按鈕：GPIO33 到 GND（GPIO33 是 RTC GPIO，深度睡眠時也能喚醒）
 * 可變電阻：中間腳接 GPIO34（ADC1），兩端接 3V3 和 GND
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：深度睡眠，計時器每 2 秒喚醒；RTC 記憶體裡的計數和睡前時間會保留
 *   1 = 練習 1：深度睡眠，按按鈕（ext0）或 10 秒到期喚醒，分辨喚醒原因
 *   2 = 練習 2：淺度睡眠（light sleep），程式從呼叫的下一行繼續、RAM 都還在
 *   3 = 練習 3 總複習：應用邏輯（lib/sensor_app）只透過 HAL 碰硬體；按鈕切換取樣，閒置 10 秒就深度睡眠，按按鈕叫醒
 */
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <sys/time.h>
#include "sensor_app.h"

#define EXERCISE 0

const int LED = 18;
const gpio_num_t BTN = GPIO_NUM_33;
const int POT = 34;

__attribute__((unused)) static const char *wake_name(esp_sleep_wakeup_cause_t c) {
  switch (c) {
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "不是從睡眠醒來（上電或 reset）";
    case ESP_SLEEP_WAKEUP_EXT0:      return "ext0（單一 RTC GPIO）";
    case ESP_SLEEP_WAKEUP_EXT1:      return "ext1（多個 RTC GPIO）";
    case ESP_SLEEP_WAKEUP_TIMER:     return "RTC 計時器";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:  return "觸控";
    case ESP_SLEEP_WAKEUP_ULP:       return "ULP 協處理器";
    case ESP_SLEEP_WAKEUP_GPIO:      return "GPIO（light sleep）";
    default:                         return "其他";
  }
}

__attribute__((unused)) static int64_t now_us() {   // RTC 計時器在深度睡眠時繼續跑，gettimeofday 不會歸零
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

__attribute__((unused)) static void button_wakeup_setup() {
  rtc_gpio_pullup_en(BTN);                          // 深度睡眠時一般 GPIO 的上拉會關掉，要用 RTC 的
  rtc_gpio_pulldown_dis(BTN);
  esp_sleep_enable_ext0_wakeup(BTN, 0);             // 電位變 0（按下）就醒
}

#if EXERCISE == 0
RTC_DATA_ATTR static int boots = 0;
RTC_DATA_ATTR static int64_t slept_at_us = 0;

void setup() {
  Serial.begin(115200);
  delay(50);
  boots++;
  esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
  Serial.printf("\n第 %d 次開機，原因：%s\n", boots, wake_name(why));
  if (why == ESP_SLEEP_WAKEUP_TIMER)
    Serial.printf("  實際睡了 %lld ms\n", (long long)((now_us() - slept_at_us) / 1000));
  if (boots >= 4) {
    Serial.println("醒來 3 次，練習結束（按 reset 重來）。");
    return;
  }
  Serial.println("  深度睡眠 2 秒。醒來會從 setup() 重新開始，只有 RTC 記憶體保留下來");
  Serial.flush();                                   // 睡前把 UART 送完，不然最後幾個字會不見
  slept_at_us = now_us();
  esp_sleep_enable_timer_wakeup(2 * 1000000ULL);
  esp_deep_sleep_start();                           // 不會回來
}
void loop() { delay(1000); }

#elif EXERCISE == 1
RTC_DATA_ATTR static int boots = 0;

void setup() {
  Serial.begin(115200);
  delay(50);
  boots++;
  esp_sleep_wakeup_cause_t why = esp_sleep_get_wakeup_cause();
  Serial.printf("\n第 %d 次開機，原因：%s\n", boots, wake_name(why));
  if (why == ESP_SLEEP_WAKEUP_EXT0) {
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    Serial.println("  被按鈕叫醒，LED 亮 1 秒");
    delay(1000);
    digitalWrite(LED, LOW);
  }
  while (digitalRead(BTN) == LOW) delay(10);        // 等按鈕放開，否則一睡就馬上被叫醒
  Serial.println("  進入深度睡眠：按按鈕或 10 秒後醒來");
  Serial.flush();
  button_wakeup_setup();
  esp_sleep_enable_timer_wakeup(10 * 1000000ULL);
  esp_deep_sleep_start();
}
void loop() { delay(1000); }

#elif EXERCISE == 2
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Day 12 EXERCISE 2：light sleep");
  pinMode(LED, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  int counter = 0;                                  // 一般區域變數：light sleep 後還在
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED, i & 1);
    esp_sleep_enable_timer_wakeup(500 * 1000);
    gpio_wakeup_enable(BTN, GPIO_INTR_LOW_LEVEL);   // light sleep 可以用任何 GPIO 喚醒
    esp_sleep_enable_gpio_wakeup();
    Serial.flush();
    int64_t t0 = now_us();
    esp_light_sleep_start();                        // 睡醒後從下一行繼續
    counter++;
    Serial.printf("  第 %d 次醒來，原因：%s，睡了 %lld ms，counter=%d\n", i + 1,
                  wake_name(esp_sleep_get_wakeup_cause()), (long long)((now_us() - t0) / 1000), counter);
  }
  Serial.println("結束：light sleep 保留 RAM 和 CPU 狀態，醒來比深度睡眠快，但耗電較高");
}
void loop() { delay(1000); }

#elif EXERCISE == 3
// ---- ESP32 版的 HAL：應用邏輯只看得到這幾個函式 ----
static uint32_t hal_now(void *) { return millis(); }
static int hal_button(void *) { return digitalRead(BTN); }
static int hal_sensor(void *) { return analogRead(POT); }
static void hal_led(void *, int on) { digitalWrite(LED, on ? HIGH : LOW); }
static void hal_report(void *, int avg, int mn, int mx) {
  Serial.printf("[app] 10 筆：平均 %d、最小 %d、最大 %d\n", avg, mn, mx);
}
static const sensor_hal_t esp32_hal = { hal_now, hal_button, hal_sensor, hal_led, hal_report, nullptr };

static sensor_app_t app;

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.printf("\nDay 12 EXERCISE 3：總複習。開機原因：%s\n", wake_name(esp_sleep_get_wakeup_cause()));
  Serial.println("按按鈕開始／停止取樣（每 100 ms 讀可變電阻），閒置 10 秒會深度睡眠，按按鈕叫醒");
  pinMode(LED, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  while (digitalRead(BTN) == LOW) delay(10);        // 叫醒我的那次按壓不算
  app_init(&app, &esp32_hal, 100, 10, 10000);
}

void loop() {
  if (app_step(&app) == APP_SLEEP) {
    Serial.println("[app] 閒置 10 秒，深度睡眠");
    Serial.flush();
    button_wakeup_setup();
    esp_deep_sleep_start();
  }
  delay(5);
}
#endif
