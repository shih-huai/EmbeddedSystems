/*
 * sensor_app 的主機端單元測試（Unity）：用假的 HAL，時間由測試自己推進
 */
#include <unity.h>
#include "sensor_app.h"

typedef struct {
  uint32_t now;
  int button;          // 1 = 放開
  int sensor;
  int led;
  int reports, last_avg, last_min, last_max;
} fake_t;

static uint32_t f_now(void *c) { return ((fake_t *)c)->now; }
static int f_btn(void *c) { return ((fake_t *)c)->button; }
static int f_sensor(void *c) { fake_t *f = c; return f->sensor; }
static void f_led(void *c, int on) { ((fake_t *)c)->led = on; }
static void f_report(void *c, int avg, int mn, int mx) {
  fake_t *f = c;
  f->reports++; f->last_avg = avg; f->last_min = mn; f->last_max = mx;
}

static fake_t f;
static sensor_app_t app;

// 時間往前推 ms 毫秒，每 5 ms 呼叫一次 app_step，回傳是否曾要求睡眠
static int run_for(uint32_t ms) {
  int slept = 0;
  for (uint32_t t = 0; t < ms; t += 5) {
    f.now += 5;
    if (app_step(&app) == APP_SLEEP) slept = 1;
  }
  return slept;
}

static void press(void) {
  f.button = 0; run_for(50);
  f.button = 1; run_for(50);
}

void setUp(void) {
  f = (fake_t){ .now = 1000, .button = 1, .sensor = 100 };
  sensor_hal_t hal = { f_now, f_btn, f_sensor, f_led, f_report, &f };
  app_init(&app, &hal, 100, 10, 5000);    // 每 100 ms 取樣、10 筆回報、閒置 5 秒睡
}
void tearDown(void) {}

void test_idle_requests_sleep_after_timeout(void) {
  TEST_ASSERT_EQUAL(0, f.led);
  TEST_ASSERT_FALSE(run_for(4900));
  TEST_ASSERT_TRUE(run_for(200));
}

void test_press_starts_sampling_and_blocks_sleep(void) {
  press();
  TEST_ASSERT_EQUAL(APP_SAMPLING, app.mode);
  TEST_ASSERT_EQUAL(1, f.led);
  TEST_ASSERT_FALSE(run_for(20000));
}

void test_report_every_n_samples(void) {
  press();
  f.reports = 0;
  int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
  for (int i = 0; i < 10; i++) { f.sensor = vals[i]; run_for(100); }
  TEST_ASSERT_EQUAL(1, f.reports);
  TEST_ASSERT_EQUAL(55, f.last_avg);
  TEST_ASSERT_EQUAL(10, f.last_min);
  TEST_ASSERT_EQUAL(100, f.last_max);
}

void test_bouncy_press_toggles_once(void) {
  int bounce[] = {0, 1, 0, 1, 0};
  for (int i = 0; i < 5; i++) { f.button = bounce[i]; f.now += 2; app_step(&app); }
  f.button = 0; run_for(50);
  f.button = 1; run_for(50);
  TEST_ASSERT_EQUAL(APP_SAMPLING, app.mode);
}

void test_second_press_back_to_idle_and_timer_restarts(void) {
  press();
  run_for(3000);
  press();
  TEST_ASSERT_EQUAL(APP_IDLE, app.mode);
  TEST_ASSERT_EQUAL(0, f.led);
  TEST_ASSERT_FALSE(run_for(4800));       // 從第二次按下開始重新計時
  TEST_ASSERT_TRUE(run_for(300));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_idle_requests_sleep_after_timeout);
  RUN_TEST(test_press_starts_sampling_and_blocks_sleep);
  RUN_TEST(test_report_every_n_samples);
  RUN_TEST(test_bouncy_press_toggles_once);
  RUN_TEST(test_second_press_back_to_idle_and_timer_restarts);
  return UNITY_END();
}
