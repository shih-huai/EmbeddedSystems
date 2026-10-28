/*
 * Day 09：FreeRTOS（一）task、排程、優先權、堆疊
 *
 * LED：GPIO18（串 220Ω 到 GND），由一個 task 負責閃爍。
 * Arduino 的 setup()/loop() 本身就跑在一個叫 loopTask 的 FreeRTOS task 裡（核心 1、優先權 1）。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：高優先權 task 搶占一直在計算的低優先權 task，量測它被喚醒的準時程度
 *   1 = 練習 1 參考解：vTaskDelay vs vTaskDelayUntil，每次多做 3 ms 工作，看誰會漂移
 *   2 = 練習 2 實驗：一個不會讓出 CPU 的高優先權 task 讓同核心的低優先權 task 餓死
 *   3 = 練習 3 實驗：量測 task 的堆疊用量，再故意讓一個 task 堆疊溢位（會重開機）
 */
#include <Arduino.h>
#include "period_stats.h"

#define EXERCISE 0

const int LED = 18;

__attribute__((unused)) static void blink_task(void *) {
  pinMode(LED, OUTPUT);
  for (;;) {
    digitalWrite(LED, !digitalRead(LED));
    vTaskDelay(pdMS_TO_TICKS(500));      // 讓出 CPU 500 ms，期間這個 task 是 Blocked
  }
}

#if EXERCISE == 0
static volatile uint32_t busy_loops = 0;

static void busy_task(void *) {           // 優先權 1，從不主動讓出
  for (;;) busy_loops++;
}

static void high_task(void *) {           // 優先權 5，每 100 ms 醒來一次
  period_stats_t st;
  ps_init(&st, 100000);
  TickType_t last = xTaskGetTickCount();
  for (int i = 1;; i++) {
    vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    ps_add(&st, (uint32_t)esp_timer_get_time());
    if (i % 20 == 0) {
      Serial.printf("[high, core %d] 週期 平均 %lu µs、最短 %lu、最長 %lu；同核心的 busy_task 跑了 %lu 圈\n",
                    xPortGetCoreID(), (unsigned long)ps_avg_us(&st), (unsigned long)st.min_us,
                    (unsigned long)st.max_us, (unsigned long)busy_loops);
      ps_init(&st, 100000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 09 EXERCISE 0：高優先權 task 搶占");
  xTaskCreatePinnedToCore(blink_task, "blink", 2048, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(busy_task, "busy", 2048, nullptr, 1, nullptr, 1);
  xTaskCreatePinnedToCore(high_task, "high", 4096, nullptr, 5, nullptr, 1);
}
void loop() { vTaskDelete(nullptr); }     // loopTask 用不到，刪掉自己

#elif EXERCISE == 1
static void periodic(void *arg) {
  const bool use_until = (bool)(intptr_t)arg;
  period_stats_t st;
  ps_init(&st, 100000);
  TickType_t last = xTaskGetTickCount();
  for (int i = 1; i <= 51; i++) {
    ps_add(&st, (uint32_t)esp_timer_get_time());
    delayMicroseconds(3000);                         // 模擬每次 3 ms 的工作
    if (use_until) {
      vTaskDelayUntil(&last, pdMS_TO_TICKS(100));   // 以「上次預定的時間」為基準
    } else {
      vTaskDelay(pdMS_TO_TICKS(100));               // 以「現在」為基準，工作時間會累積
    }
  }
  Serial.printf("%-15s 50 個週期：平均 %lu µs，累積漂移 %+ld µs\n",
                use_until ? "vTaskDelayUntil" : "vTaskDelay",
                (unsigned long)ps_avg_us(&st), (long)ps_drift_us(&st));
  vTaskDelete(nullptr);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 09 EXERCISE 1：vTaskDelay vs vTaskDelayUntil（約 5 秒後出結果）");
  xTaskCreatePinnedToCore(blink_task, "blink", 2048, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(periodic, "delay", 4096, (void *)0, 2, nullptr, 1);
  xTaskCreatePinnedToCore(periodic, "until", 4096, (void *)1, 2, nullptr, 0);
}
void loop() { vTaskDelete(nullptr); }

#elif EXERCISE == 2
static volatile uint32_t loop_count = 0;

static void hog_task(void *) {            // 優先權 5、核心 1，3 秒內完全不讓出 CPU
  uint32_t t0 = millis();
  while (millis() - t0 < 3000) { }
  Serial.println("[hog] 霸佔 3 秒結束，刪除自己");
  vTaskDelete(nullptr);
}

static void watcher_task(void *) {        // 核心 0，每秒回報 loop() 跑了幾圈
  uint32_t prev = 0;
  for (int s = 1; s <= 8; s++) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    uint32_t now = loop_count;
    Serial.printf("[watcher] 第 %d 秒：loop() 這一秒跑了 %lu 圈\n", s, (unsigned long)(now - prev));
    prev = now;
    if (s == 2) {
      Serial.println("[watcher] 建立 hog_task（優先權 5，釘在核心 1）");
      xTaskCreatePinnedToCore(hog_task, "hog", 2048, nullptr, 5, nullptr, 1);
    }
  }
  vTaskDelete(nullptr);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 09 EXERCISE 2：餓死（starvation）");
  xTaskCreatePinnedToCore(watcher_task, "watcher", 4096, nullptr, 2, nullptr, 0);
}
void loop() {                             // loopTask：核心 1、優先權 1
  loop_count++;
  delay(1);
}

#elif EXERCISE == 3

static int recurse(int depth) {
  volatile uint8_t local[128];            // 每一層吃掉 128 bytes 以上的堆疊
  local[0] = (uint8_t)depth;
  if (depth == 0) return local[0];
  return recurse(depth - 1) + local[0];
}

static void worker(void *arg) {
  int depth = (int)(intptr_t)arg;
  UBaseType_t before = uxTaskGetStackHighWaterMark(nullptr);   // 單位是 byte（ESP-IDF）
  int r = recurse(depth);
  UBaseType_t after = uxTaskGetStackHighWaterMark(nullptr);
  Serial.printf("[%s] 遞迴 %d 層：剩餘堆疊最低點 %u -> %u bytes（r=%d）\n",
                pcTaskGetName(nullptr), depth, before, after, r);
  vTaskDelete(nullptr);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 09 EXERCISE 3：堆疊用量與溢位");
  xTaskCreate(worker, "ok_4k", 4096, (void *)5, 2, nullptr);
  delay(500);
  xTaskCreate(worker, "ok_4k_deep", 4096, (void *)20, 2, nullptr);
  delay(500);
  Serial.println("接下來 2 KB 的堆疊要遞迴 40 層，會溢位...");
  delay(100);
  xTaskCreate(worker, "too_small", 2048, (void *)40, 2, nullptr);
}
void loop() { delay(1000); }
#endif
