/*
 * Day 04：硬體計時器、PWM（LEDC）、Watchdog
 *
 * 紅 LED：GPIO18（PWM 呼吸燈）；綠 LED：GPIO19（心跳燈）；各串 220Ω 到 GND。
 * 按鈕：GPIO4 到 GND（內部上拉，按下為 0）。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：硬體計時器產生 1 kHz tick，心跳燈每 500 tick 切換一次
 *   1 = 練習 1 參考解：LEDC PWM 呼吸燈（5 kHz、13 位元）
 *   2 = 練習 2 參考解：Task Watchdog。按住按鈕模擬程式卡死，3 秒後重開機，開機時印出重開原因
 *   3 = 練習 3 參考解：用 lib/timing_calc 算出各頻率的最大 PWM 解析度，再讓 ledcSetup 驗證
 * 建議先自己寫練習 1～3，再對照參考解。
 */
#include <Arduino.h>
#include "esp_task_wdt.h"
#include "timing_calc.h"

#define EXERCISE 0

const int LED_PWM = 18, LED_BEAT = 19, BTN = 4;
const uint32_t APB_HZ = 80000000;

#if EXERCISE == 0
static hw_timer_t *timer = nullptr;
static volatile uint32_t ticks = 0;

static void IRAM_ATTR on_tick() {
  ticks++;                                   // 只有 ISR 寫、loop() 只讀，32 位元，安全
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BEAT, OUTPUT);
  const uint32_t prescaler = 80;             // 80 MHz / 80 = 1 MHz
  uint32_t alarm = tc_alarm_ticks(APB_HZ, prescaler, 1000);   // 1 ms
  timer = timerBegin(0, prescaler, true);
  timerAttachInterrupt(timer, on_tick, true);
  timerAlarmWrite(timer, alarm, true);       // autoreload：每次到點自動從 0 重數
  timerAlarmEnable(timer);
  Serial.printf("Day 04 EXERCISE 0：prescaler=%lu alarm=%lu -> 1 kHz tick\n",
                (unsigned long)prescaler, (unsigned long)alarm);
}

void loop() {
  static uint32_t last_toggle = 0, last_print = 0;
  uint32_t t = ticks;
  if (t - last_toggle >= 500) {
    last_toggle += 500;
    digitalWrite(LED_BEAT, !digitalRead(LED_BEAT));
  }
  if (t - last_print >= 2000) {
    last_print = t;
    Serial.printf("ticks=%lu millis=%lu\n", (unsigned long)t, (unsigned long)millis());
  }
}

#elif EXERCISE == 1
const uint8_t CH = 0, BITS = 13;
const uint32_t PWM_HZ = 5000;

void setup() {
  Serial.begin(115200);
  uint32_t real = ledcSetup(CH, PWM_HZ, BITS);   // 回傳實際頻率，0 代表參數組合做不到
  ledcAttachPin(LED_PWM, CH);
  Serial.printf("Day 04 EXERCISE 1：LEDC ch%d %lu Hz %d bits（實際 %lu Hz）\n",
                CH, (unsigned long)PWM_HZ, BITS, (unsigned long)real);
}

void loop() {
  // 0 -> 1000 -> 0 千分比，約 2 秒一個循環。不用 delay 也可以，這裡為了簡單用 delay(1)
  static int permille = 0, dir = 1;
  permille += dir;
  if (permille >= 1000 || permille <= 0) dir = -dir;
  // 人眼對亮度是非線性的，用平方近似 gamma 校正，暗部變化才看得出來
  uint32_t perceived = (uint32_t)permille * permille / 1000;
  ledcWrite(CH, tc_duty_from_permille(BITS, perceived));
  delay(1);
}

#elif EXERCISE == 2
void setup() {
  Serial.begin(115200);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(LED_BEAT, OUTPUT);
  esp_reset_reason_t why = esp_reset_reason();
  Serial.printf("Day 04 EXERCISE 2：開機，重開原因 = %d%s\n", why,
                why == ESP_RST_TASK_WDT ? "（Task Watchdog 逾時）" :
                why == ESP_RST_PANIC    ? "（panic，Arduino 2.x 的 TWDT 逾時會走這條）" :
                why == ESP_RST_POWERON  ? "（上電）" : "");
  esp_task_wdt_init(3, true);   // 逾時 3 秒，逾時就 panic 重開
  esp_task_wdt_add(NULL);       // NULL = 目前這個 task（Arduino 的 loopTask）
  Serial.println("按住按鈕模擬程式卡死");
}

void loop() {
  if (digitalRead(BTN) == LOW) {
    Serial.println("進入無窮迴圈，不再餵狗...");
    while (true) { }            // 模擬卡死
  }
  esp_task_wdt_reset();         // 餵狗：只在「確定一切正常」的地方餵
  digitalWrite(LED_BEAT, (millis() / 250) % 2);
  delay(10);
}

#elif EXERCISE == 3
void setup() {
  Serial.begin(115200);
  Serial.println("Day 04 EXERCISE 3：PWM 頻率 vs 最大解析度");
  Serial.println("  頻率(Hz)  計算位元  ledcSetup 實際頻率(Hz)");
  const uint32_t freqs[] = {50, 1000, 5000, 20000, 312500, 1000000};
  for (uint32_t f : freqs) {
    uint8_t bits = tc_pwm_max_bits(APB_HZ, f);
    uint32_t real = ledcSetup(0, f, bits);
    Serial.printf("  %8lu  %8u  %10lu\n", (unsigned long)f, bits, (unsigned long)real);
  }
  Serial.println("再多 1 位元試試看（應該失敗，回傳 0）：");
  uint32_t f = 5000;
  uint8_t bits = tc_pwm_max_bits(APB_HZ, f) + 1;
  Serial.printf("  %lu Hz %u bits -> %lu\n", (unsigned long)f, bits, (unsigned long)ledcSetup(0, f, bits));
}

void loop() { delay(1000); }
#endif
