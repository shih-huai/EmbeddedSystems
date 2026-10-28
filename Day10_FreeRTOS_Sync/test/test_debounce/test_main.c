/*
 * debounce 的主機端單元測試（Unity）
 */
#include <unity.h>
#include "debounce.h"

static debounce_t d;

void setUp(void) { db_init(&d, 1, 20); }   // 放開狀態（1），要穩定 20 ms
void tearDown(void) {}

void test_clean_press_and_release(void) {
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 0, 100));
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 0, 119));
  TEST_ASSERT_EQUAL(DB_PRESSED, db_update(&d, 0, 120));
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 0, 130));        // 只報一次
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 1, 500));
  TEST_ASSERT_EQUAL(DB_RELEASED, db_update(&d, 1, 520));
}

void test_bouncy_press_gives_one_event(void) {
  uint8_t bounce[] = {0, 1, 0, 1, 0, 0, 1, 0};
  int events = 0;
  uint32_t t = 0;
  for (unsigned i = 0; i < sizeof bounce; i++, t += 2)
    if (db_update(&d, bounce[i], t) != DB_NONE) events++;
  for (; t < 100; t += 5)
    if (db_update(&d, 0, t) == DB_PRESSED) events++;
  TEST_ASSERT_EQUAL(1, events);
}

void test_short_glitch_ignored(void) {
  db_update(&d, 0, 100);
  db_update(&d, 0, 110);                                      // 只低了 10 ms
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 1, 112));
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 1, 200));
  TEST_ASSERT_EQUAL(1, d.stable);
}

void test_wraparound(void) {
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 0, 0xFFFFFFFFu - 9));
  TEST_ASSERT_EQUAL(DB_NONE, db_update(&d, 0, 5));             // 經過 15 ms
  TEST_ASSERT_EQUAL(DB_PRESSED, db_update(&d, 0, 10));         // 經過 20 ms
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_clean_press_and_release);
  RUN_TEST(test_bouncy_press_gives_one_event);
  RUN_TEST(test_short_glitch_ignored);
  RUN_TEST(test_wraparound);
  return UNITY_END();
}
