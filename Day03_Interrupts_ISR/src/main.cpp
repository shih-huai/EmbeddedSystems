/*
 * Day 03：中斷與 ISR
 *
 * LED：GPIO18（串 220Ω 到 GND）；按鈕：GPIO4 到 GND（內部上拉，按下為 0）。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：按鈕下降緣觸發中斷，ISR 只計數，loop() 負責印出
 *   1 = 練習 1 參考解：在 ISR 裡用時間鎖定去彈跳，並直接用暫存器切換 LED
 *   2 = 練習 2 實驗：主程式和計時器 ISR 同時改一個共享變數，觀察「遺失的更新」
 *   3 = 練習 3 參考解：用 critical section 修好練習 2
 * 建議先自己寫練習 1、3，再對照參考解。
 */
#include <Arduino.h>
#include "soc/gpio_reg.h"

#define EXERCISE 0

const int LED = 18, BTN = 4;

#if EXERCISE == 0
// ISR 和 loop() 共享的變數一定要 volatile，否則 loop() 可能永遠讀到暫存器裡的舊值
static volatile uint32_t isr_count = 0;

// IRAM_ATTR：把 ISR 放進內部 RAM。寫 flash 時 cache 會被關掉，
// 這時如果中斷觸發而 ISR 在 flash 裡，CPU 讀不到指令就會當機
static void IRAM_ATTR on_button() {
  isr_count++;          // 單一 32 位元變數只有 ISR 在寫，loop() 只讀，這樣是安全的
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN), on_button, FALLING);
  Serial.println("Day 03 EXERCISE 0：按按鈕，看中斷被觸發幾次");
}

void loop() {
  static uint32_t shown = 0;
  uint32_t now = isr_count;            // 讀一次存到區域變數，後面都用這份
  if (now != shown) {
    Serial.printf("中斷次數：%lu\n", (unsigned long)now);  // 印出放在 loop()，不放在 ISR
    shown = now;
  }
}

#elif EXERCISE == 1
static volatile uint32_t presses = 0;
static volatile uint32_t last_us = 0;
static const uint32_t LOCKOUT_US = 50000;   // 第一個邊緣之後 50 ms 內的邊緣都視為彈跳

static void IRAM_ATTR on_button() {
  uint32_t now = micros();                  // micros() 可以在 ISR 中呼叫
  if (now - last_us < LOCKOUT_US) return;
  last_us = now;
  presses++;
  // ISR 裡可以直接寫 W1TS／W1TC：單一寫入，不會跟 loop() 搶同一個暫存器
  if (REG_READ(GPIO_OUT_REG) & (1u << LED)) REG_WRITE(GPIO_OUT_W1TC_REG, 1u << LED);
  else                                      REG_WRITE(GPIO_OUT_W1TS_REG, 1u << LED);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN), on_button, FALLING);
  Serial.println("Day 03 EXERCISE 1：ISR 去彈跳，每按一次 LED 切換一次");
}

void loop() {
  static uint32_t shown = 0;
  uint32_t now = presses;
  if (now != shown) {
    Serial.printf("按下次數：%lu\n", (unsigned long)now);
    shown = now;
  }
}

#elif EXERCISE == 2 || EXERCISE == 3
// 計時器 ISR 每 20 µs 對 shared 加 1，loop() 也對 shared 加 1。
// 如果每次加法都沒被打斷，shared 應該剛好等於 isr_adds + main_adds。
static volatile uint32_t shared = 0;
static volatile uint32_t isr_adds = 0;
static hw_timer_t *timer = nullptr;
__attribute__((unused)) static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;   // ESP32 是雙核心，critical section 需要一把 spinlock

static void IRAM_ATTR on_timer() {
#if EXERCISE == 3
  portENTER_CRITICAL_ISR(&mux);
#endif
  shared++;
  isr_adds++;
#if EXERCISE == 3
  portEXIT_CRITICAL_ISR(&mux);
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.printf("Day 03 EXERCISE %d：%s\n", EXERCISE, EXERCISE == 2 ? "沒有保護" : "有 critical section");
  timer = timerBegin(0, 80, true);           // 80 MHz APB / 80 = 1 MHz，每 tick 1 µs
  timerAttachInterrupt(timer, on_timer, true);
  timerAlarmWrite(timer, 20, true);          // 每 20 µs 觸發一次，自動重載
  timerAlarmEnable(timer);
}

void loop() {
  static int round_no = 0;
  if (round_no >= 5) { delay(1000); return; }
  delay(100);

  // 先讓 ISR 停一下，取得一致的起點
  timerAlarmDisable(timer);
  shared = 0;
  isr_adds = 0;
  timerWrite(timer, 0);        // 計數器停用期間仍在跑，要歸零，否則已超過比較值的計數器不會再觸發
  timerAlarmEnable(timer);

  const uint32_t MAIN_ADDS = 1000000;
  for (uint32_t i = 0; i < MAIN_ADDS; i++) {
#if EXERCISE == 3
    portENTER_CRITICAL(&mux);
#endif
    shared++;            // 讀–改–寫三個步驟，中間可能被 ISR 打斷
#if EXERCISE == 3
    portEXIT_CRITICAL(&mux);
#endif
  }

  timerAlarmDisable(timer);
  uint32_t expected = isr_adds + MAIN_ADDS;
  Serial.printf("第 %d 輪：shared=%lu 預期=%lu 遺失=%ld\n", ++round_no,
                (unsigned long)shared, (unsigned long)expected, (long)(expected - shared));
}
#endif
