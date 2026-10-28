/*
 * timing_calc 的主機端單元測試（Unity）
 * 執行：pio test -e native
 */
#include <unity.h>
#include "timing_calc.h"

void setUp(void) {}
void tearDown(void) {}

void test_alarm_1khz_tick_with_1mhz_timer(void) {
  // 80 MHz / 80 = 1 MHz，1 ms = 1000 tick
  TEST_ASSERT_EQUAL_UINT32(1000, tc_alarm_ticks(80000000, 80, 1000));
}

void test_alarm_large_period_does_not_overflow(void) {
  // 10 秒、prescaler 2：40 MHz × 10 s = 4e8，乘法中間值超過 32 位元
  TEST_ASSERT_EQUAL_UINT32(400000000u, tc_alarm_ticks(80000000, 2, 10000000));
}

void test_alarm_invalid_inputs(void) {
  TEST_ASSERT_EQUAL_UINT32(0, tc_alarm_ticks(80000000, 0, 1000));
  TEST_ASSERT_EQUAL_UINT32(0, tc_alarm_ticks(80000000, 1, 100000000));  // 8e9 超過 32 位元
}

void test_pwm_bits_common_cases(void) {
  TEST_ASSERT_EQUAL_UINT8(13, tc_pwm_max_bits(80000000, 5000));     // 80M/5k = 16000 -> 13 位元
  TEST_ASSERT_EQUAL_UINT8(6,  tc_pwm_max_bits(80000000, 1000000));  // 80 個時脈 -> 6 位元
  TEST_ASSERT_EQUAL_UINT8(20, tc_pwm_max_bits(80000000, 50));       // 理論 20.6，LEDC 上限 20
  TEST_ASSERT_EQUAL_UINT8(1,  tc_pwm_max_bits(80000000, 40000000));
  TEST_ASSERT_EQUAL_UINT8(0,  tc_pwm_max_bits(80000000, 50000000)); // 一個週期不到 2 個時脈
  TEST_ASSERT_EQUAL_UINT8(0,  tc_pwm_max_bits(80000000, 0));
}

void test_duty_from_permille(void) {
  TEST_ASSERT_EQUAL_UINT32(0,    tc_duty_from_permille(13, 0));
  TEST_ASSERT_EQUAL_UINT32(4096, tc_duty_from_permille(13, 500));   // 8191 × 0.5 = 4095.5 -> 4096
  TEST_ASSERT_EQUAL_UINT32(8191, tc_duty_from_permille(13, 1000));
  TEST_ASSERT_EQUAL_UINT32(8191, tc_duty_from_permille(13, 5000));  // 超過 100% 夾到最大值
  TEST_ASSERT_EQUAL_UINT32(255,  tc_duty_from_permille(8, 1000));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_alarm_1khz_tick_with_1mhz_timer);
  RUN_TEST(test_alarm_large_period_does_not_overflow);
  RUN_TEST(test_alarm_invalid_inputs);
  RUN_TEST(test_pwm_bits_common_cases);
  RUN_TEST(test_duty_from_permille);
  return UNITY_END();
}
