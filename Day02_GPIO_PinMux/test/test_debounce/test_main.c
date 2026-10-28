/*
 * 去彈跳狀態機的主機端單元測試（Unity，PlatformIO 內建）
 * 執行：pio test -e native
 */
#include <unity.h>
#include "debounce.h"

static debounce_t db;

void setUp(void) { debounce_init(&db, 20); }
void tearDown(void) {}

// 依序餵入 (電位, 時間)，回傳總共收到幾個 PRESS 事件
static int feed(const bool *raw, const uint32_t *t, int n) {
  int presses = 0;
  for (int i = 0; i < n; i++) {
    if (debounce_update(&db, raw[i], t[i]) == DB_EVT_PRESS) presses++;
  }
  return presses;
}

void test_clean_press_gives_one_event_after_hold_time(void) {
  TEST_ASSERT_EQUAL(DB_EVT_NONE,  debounce_update(&db, true, 100));
  TEST_ASSERT_EQUAL(DB_EVT_NONE,  debounce_update(&db, true, 119));
  TEST_ASSERT_EQUAL(DB_EVT_PRESS, debounce_update(&db, true, 120));
  TEST_ASSERT_EQUAL(DB_EVT_NONE,  debounce_update(&db, true, 500));  // 按住不重複觸發
}

void test_bouncing_press_counts_once(void) {
  // 按下瞬間在 6 ms 內抖了好幾次，之後穩定
  bool     raw[] = {1, 0, 1, 0, 1, 1, 1, 1};
  uint32_t t[]   = {0, 1, 2, 4, 6, 10, 20, 30};
  TEST_ASSERT_EQUAL_INT(1, feed(raw, t, 8));
}

void test_short_glitch_is_ignored(void) {
  bool     raw[] = {1, 1, 0, 0};
  uint32_t t[]   = {0, 5, 10, 50};   // 雜訊只維持 10 ms
  TEST_ASSERT_EQUAL_INT(0, feed(raw, t, 4));
  TEST_ASSERT_EQUAL(DB_RELEASED, db.state);
}

void test_release_event_after_hold_time(void) {
  debounce_update(&db, true, 0);
  debounce_update(&db, true, 20);                                    // PRESS
  TEST_ASSERT_EQUAL(DB_EVT_NONE,    debounce_update(&db, false, 100));
  TEST_ASSERT_EQUAL(DB_EVT_NONE,    debounce_update(&db, true,  103)); // 放開時彈跳
  TEST_ASSERT_EQUAL(DB_EVT_NONE,    debounce_update(&db, false, 105));
  TEST_ASSERT_EQUAL(DB_EVT_RELEASE, debounce_update(&db, false, 125));
}

void test_millis_wraparound(void) {
  // millis() 約 49.7 天後會從 0xFFFFFFFF 繞回 0
  TEST_ASSERT_EQUAL(DB_EVT_NONE,  debounce_update(&db, true, 0xFFFFFFF0u));
  TEST_ASSERT_EQUAL(DB_EVT_PRESS, debounce_update(&db, true, 0x00000004u));  // 經過 20 ms
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_clean_press_gives_one_event_after_hold_time);
  RUN_TEST(test_bouncing_press_counts_once);
  RUN_TEST(test_short_glitch_is_ignored);
  RUN_TEST(test_release_event_after_hold_time);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
