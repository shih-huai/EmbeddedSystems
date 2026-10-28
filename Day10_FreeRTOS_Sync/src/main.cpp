/*
 * Day 10：FreeRTOS（二）queue、semaphore、mutex、ISR 交接
 *
 * LED：GPIO18（串 220Ω 到 GND）
 * 按鈕：GPIO4 到 GND（內部上拉，按下 = LOW）
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：計時器 ISR 每 10 ms 把一筆取樣丟進 queue，task 取出處理；按鈕 ISR 用 task notification 叫醒 task
 *   1 = 練習 1 實驗：binary semaphore vs counting semaphore，ISR 連續觸發時誰會掉事件
 *   2 = 練習 2 實驗：兩個 task 改同一個變數，沒有 mutex 會遺失更新，加上 mutex 就正確
 *   3 = 練習 3 實驗：優先權反轉。用 binary semaphore 當鎖，高優先權 task 要等很久；換成 mutex（優先權繼承）就好了
 */
#include <Arduino.h>
#include "debounce.h"

#define EXERCISE 0

const int LED = 18;
const int BTN = 4;

// 吃掉 ms 毫秒的 CPU 時間（被搶占時不會計時，和 delayMicroseconds 不同）
static uint32_t loops_per_ms = 0;
__attribute__((unused)) static void calibrate_burn() {
  uint32_t t0 = micros(), n = 0;
  while (micros() - t0 < 100000) n++;
  loops_per_ms = n / 100;
}
__attribute__((unused)) static void burn_ms(uint32_t ms) {
  uint32_t t0 = micros();
  for (volatile uint32_t i = 0; i < loops_per_ms * ms; i++) {
    if (micros() - t0 > 0xFFFFFFF0u) break;   // 和 calibrate_burn 做一樣多的事
  }
}

#if EXERCISE == 0
typedef struct {
  uint32_t seq;      // 第幾筆，用來檢查有沒有漏
  uint32_t t_us;     // ISR 產生的時間，用來算延遲
} sample_t;

static QueueHandle_t sample_q;
static TaskHandle_t btn_task_handle;
static hw_timer_t *timer;
static volatile uint32_t isr_seq = 0;
static volatile uint32_t isr_drops = 0;
static volatile bool slow_consumer = false;

static void IRAM_ATTR on_timer() {
  sample_t s = { isr_seq++, (uint32_t)micros() };
  BaseType_t woken = pdFALSE;
  if (xQueueSendFromISR(sample_q, &s, &woken) != pdTRUE)   // ISR 裡只能用 FromISR 版本，而且不能等
    isr_drops++;
  if (woken) portYIELD_FROM_ISR();                         // 叫醒了更高優先權的 task，離開 ISR 就切過去
}

static void IRAM_ATTR on_button() {
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(btn_task_handle, &woken);         // 比 semaphore 更輕的「叫醒某個 task」
  if (woken) portYIELD_FROM_ISR();
}

static void sample_task(void *) {
  uint32_t expect = 0, gaps = 0, n = 0, max_lat = 0;
  for (;;) {
    sample_t s;
    xQueueReceive(sample_q, &s, portMAX_DELAY);           // 沒資料就 Blocked，不佔 CPU
    uint32_t lat = (uint32_t)micros() - s.t_us;
    if (lat > max_lat) max_lat = lat;
    if (s.seq != expect) gaps += s.seq - expect;
    expect = s.seq + 1;
    if (slow_consumer) delay(15);                         // 處理一筆要 15 ms，比產生的速度（10 ms）慢
    if (++n % 100 == 0) {
      Serial.printf("[sample] 收 %lu 筆、最大延遲 %lu µs、序號缺口 %lu、ISR 丟棄 %lu、queue 內 %u 筆\n",
                    (unsigned long)n, (unsigned long)max_lat, (unsigned long)gaps,
                    (unsigned long)isr_drops, (unsigned)uxQueueMessagesWaiting(sample_q));
      max_lat = 0;
    }
  }
}

static void button_task(void *) {
  debounce_t db;
  db_init(&db, 1, 20);
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);              // 等 ISR 叫醒；彈跳造成的多次通知會合併
    uint32_t t0 = millis();
    while (millis() - t0 < 100) {                         // 醒來後在 task 裡慢慢去彈跳
      db_event_t e = db_update(&db, digitalRead(BTN), millis());
      if (e == DB_PRESSED) {
        digitalWrite(LED, !digitalRead(LED));
        Serial.println("[button] 按下，切換 LED");
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 10 EXERCISE 0：ISR -> queue -> task");
  pinMode(LED, OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
  sample_q = xQueueCreate(8, sizeof(sample_t));           // 8 筆，每筆複製一份 sample_t 進去
  xTaskCreatePinnedToCore(sample_task, "sample", 4096, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(button_task, "button", 4096, nullptr, 2, &btn_task_handle, 1);
  attachInterrupt(BTN, on_button, FALLING);
  timer = timerBegin(0, 80, true);                        // 80 MHz / 80 = 1 MHz
  timerAttachInterrupt(timer, on_timer, true);
  timerAlarmWrite(timer, 10000, true);                    // 每 10 ms
  timerAlarmEnable(timer);
}

void loop() {
  delay(3000);
  if (!slow_consumer) {
    Serial.println("---- 消費者變慢（每筆 15 ms），queue 會滿，ISR 開始丟資料 ----");
    slow_consumer = true;
  }
}

#elif EXERCISE == 1
static SemaphoreHandle_t bin_sem, cnt_sem;
static hw_timer_t *timer;
static volatile uint32_t ticks = 0, given = 0;
static volatile uint32_t handled[2] = {0, 0};

static void IRAM_ATTR on_timer() {                        // 每 1 ms 進來一次
  uint32_t t = ticks++;
  if (t % 100 < 5) {                                      // 每 100 ms 連續發生 5 個事件（間隔 1 ms）
    BaseType_t woken = pdFALSE;
    given++;
    xSemaphoreGiveFromISR(bin_sem, &woken);               // binary：已經是 1 就給不進去
    xSemaphoreGiveFromISR(cnt_sem, &woken);               // counting：一直往上加
    if (woken) portYIELD_FROM_ISR();
  }
}

static void worker(void *arg) {
  int idx = (int)(intptr_t)arg;
  SemaphoreHandle_t s = idx ? cnt_sem : bin_sem;
  for (;;) {
    xSemaphoreTake(s, portMAX_DELAY);
    delayMicroseconds(3000);                              // 每個事件要處理 3 ms
    handled[idx]++;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 10 EXERCISE 1：binary vs counting semaphore（3 秒後出結果）");
  bin_sem = xSemaphoreCreateBinary();
  cnt_sem = xSemaphoreCreateCounting(100, 0);             // 最多記 100 個，初始 0
  xTaskCreatePinnedToCore(worker, "bin", 2048, (void *)0, 3, nullptr, 0);
  xTaskCreatePinnedToCore(worker, "cnt", 2048, (void *)1, 3, nullptr, 1);
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, on_timer, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
  delay(3000);
  timerAlarmDisable(timer);
  delay(100);
  Serial.printf("ISR 發生 %lu 個事件\n", (unsigned long)given);
  Serial.printf("  binary semaphore 處理了 %lu 個（遺失 %lu）\n", (unsigned long)handled[0],
                (unsigned long)(given - handled[0]));
  Serial.printf("  counting semaphore 處理了 %lu 個（遺失 %lu）\n", (unsigned long)handled[1],
                (unsigned long)(given - handled[1]));
}
void loop() { delay(1000); }

#elif EXERCISE == 2
static volatile uint32_t shared_counter = 0;
static SemaphoreHandle_t lock;
static SemaphoreHandle_t done;
static bool use_mutex;

static void adder(void *) {
  for (int i = 0; i < 1000; i++) {
    if (use_mutex) xSemaphoreTake(lock, portMAX_DELAY);
    uint32_t v = shared_counter;    // 讀
    taskYIELD();                    // 模擬「讀完、還沒寫回」時被 tick 中斷切走
    shared_counter = v + 1;         // 寫回
    if (use_mutex) xSemaphoreGive(lock);
  }
  xSemaphoreGive(done);
  vTaskDelete(nullptr);
}

static void run(bool with_mutex) {
  shared_counter = 0;
  use_mutex = with_mutex;
  xTaskCreatePinnedToCore(adder, "A", 2048, nullptr, 2, nullptr, 1);   // 同核心、同優先權
  xTaskCreatePinnedToCore(adder, "B", 2048, nullptr, 2, nullptr, 1);
  xSemaphoreTake(done, portMAX_DELAY);
  xSemaphoreTake(done, portMAX_DELAY);
  Serial.printf("%-10s 兩個 task 各加 1000 次，結果 %lu（應該是 2000）\n",
                with_mutex ? "有 mutex" : "沒有保護", (unsigned long)shared_counter);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 10 EXERCISE 2：共享資料與 mutex");
  vTaskPrioritySet(nullptr, 3);   // 先把自己調高，A、B 建立後都還沒開始跑，等我 block 時才一起開始
  lock = xSemaphoreCreateMutex();
  done = xSemaphoreCreateCounting(2, 0);
  run(false);
  run(true);
}
void loop() { delay(1000); }

#elif EXERCISE == 3
static SemaphoreHandle_t lock;
static volatile uint32_t t_start;
static volatile uint32_t high_wait_ms;
static volatile uint32_t medium_end_ms, low_release_ms;

static void low_task(void *) {          // 優先權 1：拿鎖，做 50 ms 的事
  xSemaphoreTake(lock, portMAX_DELAY);
  burn_ms(50);
  low_release_ms = millis() - t_start;
  xSemaphoreGive(lock);
  vTaskDelete(nullptr);
}

static void medium_task(void *) {       // 優先權 2：和鎖無關，連續算 500 ms
  burn_ms(500);
  medium_end_ms = millis() - t_start;
  vTaskDelete(nullptr);
}

static void high_task(void *) {         // 優先權 3：需要鎖
  uint32_t t0 = millis();
  xSemaphoreTake(lock, portMAX_DELAY);
  high_wait_ms = millis() - t0;
  xSemaphoreGive(lock);
  vTaskDelete(nullptr);
}

static void scenario(const char *name, SemaphoreHandle_t l) {
  lock = l;
  t_start = millis();
  xTaskCreatePinnedToCore(low_task, "low", 2048, nullptr, 1, nullptr, 1);
  vTaskDelay(pdMS_TO_TICKS(10));        // low 先拿到鎖
  xTaskCreatePinnedToCore(high_task, "high", 2048, nullptr, 3, nullptr, 1);   // high 等鎖 -> Blocked
  xTaskCreatePinnedToCore(medium_task, "medium", 2048, nullptr, 2, nullptr, 1);
  vTaskDelay(pdMS_TO_TICKS(800));
  Serial.printf("%-18s high 等鎖 %lu ms；low 在第 %lu ms 放鎖，medium 在第 %lu ms 做完\n", name,
                (unsigned long)high_wait_ms, (unsigned long)low_release_ms, (unsigned long)medium_end_ms);
  vSemaphoreDelete(l);
}

static void controller(void *) {        // 優先權 5，大部分時間在 vTaskDelay
  calibrate_burn();
  SemaphoreHandle_t bin = xSemaphoreCreateBinary();
  xSemaphoreGive(bin);                  // binary semaphore 建立時是 0，先 give 一次才能當鎖用
  scenario("binary semaphore", bin);
  scenario("mutex", xSemaphoreCreateMutex());   // mutex 有優先權繼承
  vTaskDelete(nullptr);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Day 10 EXERCISE 3：優先權反轉");
  xTaskCreatePinnedToCore(controller, "ctrl", 4096, nullptr, 5, nullptr, 1);
}
void loop() { vTaskDelete(nullptr); }   // loopTask 也在核心 1，刪掉免得干擾
#endif
