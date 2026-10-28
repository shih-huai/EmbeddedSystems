/*
 * period_stats 的主機端單元測試（Unity）
 */
#include <unity.h>
#include "period_stats.h"

static period_stats_t s;

void setUp(void) { ps_init(&s, 100000); }   // 期望 100 ms
void tearDown(void) {}

void test_perfect_period(void) {
  for (uint32_t i = 0; i < 11; i++) ps_add(&s, 5000 + i * 100000);
  TEST_ASSERT_EQUAL_UINT32(100000, ps_avg_us(&s));
  TEST_ASSERT_EQUAL_UINT32(100000, s.min_us);
  TEST_ASSERT_EQUAL_UINT32(100000, s.max_us);
  TEST_ASSERT_EQUAL_INT32(0, ps_drift_us(&s));
}

void test_drift_accumulates(void) {
  // vTaskDelay(100 ms) 加上每次 3 ms 的工作：週期變 103 ms，10 次後漂移 30 ms
  for (uint32_t i = 0; i < 11; i++) ps_add(&s, i * 103000);
  TEST_ASSERT_EQUAL_UINT32(103000, ps_avg_us(&s));
  TEST_ASSERT_EQUAL_INT32(30000, ps_drift_us(&s));
}

void test_jitter_without_drift(void) {
  // 週期有抖動（95、105 ms 交替），但長期不漂移
  uint32_t t = 0;
  ps_add(&s, t);
  for (int i = 0; i < 10; i++) { t += (i % 2) ? 105000 : 95000; ps_add(&s, t); }
  TEST_ASSERT_EQUAL_UINT32(95000, s.min_us);
  TEST_ASSERT_EQUAL_UINT32(105000, s.max_us);
  TEST_ASSERT_EQUAL_INT32(0, ps_drift_us(&s));
}

void test_wraparound(void) {
  ps_add(&s, 0xFFFFFFFFu - 50000);
  ps_add(&s, 49999);                         // 經過 100 ms，中間繞回 0
  TEST_ASSERT_EQUAL_UINT32(100000, s.min_us);
  TEST_ASSERT_EQUAL_INT32(0, ps_drift_us(&s));
}

void test_too_few_samples(void) {
  TEST_ASSERT_EQUAL_UINT32(0, ps_avg_us(&s));
  ps_add(&s, 123);
  TEST_ASSERT_EQUAL_UINT32(0, ps_avg_us(&s));
  TEST_ASSERT_EQUAL_INT32(0, ps_drift_us(&s));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_perfect_period);
  RUN_TEST(test_drift_accumulates);
  RUN_TEST(test_jitter_without_drift);
  RUN_TEST(test_wraparound);
  RUN_TEST(test_too_few_samples);
  return UNITY_END();
}
