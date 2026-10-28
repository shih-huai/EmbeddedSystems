/*
 * ring buffer 的主機端單元測試（Unity）
 */
#include <unity.h>
#include "ringbuf.h"

static uint8_t storage[8];
static ringbuf_t rb;

void setUp(void) { TEST_ASSERT_TRUE(rb_init(&rb, storage, sizeof(storage))); }
void tearDown(void) {}

void test_init_rejects_non_power_of_two(void) {
  ringbuf_t r;
  uint8_t s[12];
  TEST_ASSERT_FALSE(rb_init(&r, s, 12));
  TEST_ASSERT_FALSE(rb_init(&r, s, 0));
  TEST_ASSERT_FALSE(rb_init(&r, 0, 8));
}

void test_fifo_order(void) {
  for (uint8_t i = 1; i <= 5; i++) TEST_ASSERT_TRUE(rb_put(&rb, i));
  TEST_ASSERT_EQUAL_UINT32(5, rb_count(&rb));
  for (uint8_t i = 1; i <= 5; i++) {
    uint8_t b;
    TEST_ASSERT_TRUE(rb_get(&rb, &b));
    TEST_ASSERT_EQUAL_UINT8(i, b);
  }
  uint8_t b;
  TEST_ASSERT_FALSE(rb_get(&rb, &b));
}

void test_full_uses_every_slot_and_counts_drops(void) {
  for (int i = 0; i < 8; i++) TEST_ASSERT_TRUE(rb_put(&rb, (uint8_t)i));
  TEST_ASSERT_FALSE(rb_put(&rb, 99));
  TEST_ASSERT_FALSE(rb_put(&rb, 99));
  TEST_ASSERT_EQUAL_UINT32(2, rb.dropped);
  TEST_ASSERT_EQUAL_UINT32(8, rb_count(&rb));
}

void test_wraps_around_many_times(void) {
  uint8_t expect = 0, got;
  for (int round = 0; round < 1000; round++) {
    for (int i = 0; i < 3; i++) TEST_ASSERT_TRUE(rb_put(&rb, (uint8_t)(round * 3 + i)));
    for (int i = 0; i < 3; i++) {
      TEST_ASSERT_TRUE(rb_get(&rb, &got));
      TEST_ASSERT_EQUAL_UINT8(expect++, got);
    }
  }
}

void test_index_overflow_of_uint32(void) {
  // 把索引放到快要溢位的地方，確認繞回 0 之後計數仍正確
  rb.head = rb.tail = 0xFFFFFFFEu;
  for (int i = 0; i < 8; i++) TEST_ASSERT_TRUE(rb_put(&rb, (uint8_t)i));
  TEST_ASSERT_FALSE(rb_put(&rb, 0));
  TEST_ASSERT_EQUAL_UINT32(8, rb_count(&rb));
  uint8_t b;
  TEST_ASSERT_TRUE(rb_get(&rb, &b));
  TEST_ASSERT_EQUAL_UINT8(0, b);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_init_rejects_non_power_of_two);
  RUN_TEST(test_fifo_order);
  RUN_TEST(test_full_uses_every_slot_and_counts_drops);
  RUN_TEST(test_wraps_around_many_times);
  RUN_TEST(test_index_overflow_of_uint32);
  return UNITY_END();
}
