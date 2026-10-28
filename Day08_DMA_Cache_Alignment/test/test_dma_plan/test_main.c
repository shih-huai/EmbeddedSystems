/*
 * dma_plan 與 ili9341_begin_ramwr 的主機端單元測試（Unity）
 */
#include <string.h>
#include <unity.h>
#include "dma_plan.h"

void setUp(void) {}
void tearDown(void) {}

void test_esp32_descriptor_limit(void) {
  // 一整個畫面 153,600 bytes，每段最多 4095、要 4 的倍數 -> 每段 4092
  size_t out[64];
  size_t n = dma_plan(153600, 4095, 4, out, 64);
  TEST_ASSERT_EQUAL_size_t(38, n);                // 37 × 4092 = 151,404，剩 2,196
  for (size_t i = 0; i + 1 < n; i++) TEST_ASSERT_EQUAL_size_t(4092, out[i]);
  TEST_ASSERT_EQUAL_size_t(2196, out[n - 1]);
  size_t sum = 0;
  for (size_t i = 0; i < n; i++) sum += out[i];
  TEST_ASSERT_EQUAL_size_t(153600, sum);
}

void test_small_transfer_is_one_chunk(void) {
  size_t out[4];
  TEST_ASSERT_EQUAL_size_t(1, dma_plan(10, 4095, 4, out, 4));
  TEST_ASSERT_EQUAL_size_t(10, out[0]);
}

void test_zero_length(void) {
  size_t out[4];
  TEST_ASSERT_EQUAL_size_t(0, dma_plan(0, 4095, 4, out, 4));
}

void test_not_enough_slots(void) {
  size_t out[2];
  TEST_ASSERT_EQUAL_size_t(0, dma_plan(10000, 4095, 4, out, 2));
}

void test_bad_arguments(void) {
  size_t out[4];
  TEST_ASSERT_EQUAL_size_t(0, dma_plan(100, 4095, 0, out, 4));
  TEST_ASSERT_EQUAL_size_t(0, dma_plan(100, 2, 4, out, 4));
}

void test_alignment_check(void) {
  uint32_t words[2] = {0, 0};
  uint8_t *p = (uint8_t *)words;
  TEST_ASSERT_TRUE(dma_is_aligned(p, 4));
  TEST_ASSERT_FALSE(dma_is_aligned(p + 1, 4));
  TEST_ASSERT_TRUE(dma_is_aligned(p + 2, 2));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_esp32_descriptor_limit);
  RUN_TEST(test_small_transfer_is_one_chunk);
  RUN_TEST(test_zero_length);
  RUN_TEST(test_not_enough_slots);
  RUN_TEST(test_bad_arguments);
  RUN_TEST(test_alignment_check);
  return UNITY_END();
}
