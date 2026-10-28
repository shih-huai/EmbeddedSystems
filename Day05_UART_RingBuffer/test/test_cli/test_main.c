/*
 * 指令列的主機端單元測試（Unity）
 */
#include <string.h>
#include <unity.h>
#include "cli.h"

static line_reader_t lr;

void setUp(void) { lr_init(&lr); }
void tearDown(void) {}

static bool feed_str(const char *s) {
  bool got = false;
  for (; *s; s++) got = lr_feed(&lr, *s);
  return got;
}

void test_line_with_crlf(void) {
  TEST_ASSERT_TRUE(feed_str("led on\r\n"));
  TEST_ASSERT_EQUAL_STRING("led on", lr.buf);
}

void test_backspace(void) {
  TEST_ASSERT_TRUE(feed_str("lex\x08" "d\n"));
  TEST_ASSERT_EQUAL_STRING("led", lr.buf);
}

void test_too_long_line_is_dropped_whole(void) {
  char longline[CLI_LINE_MAX + 10];
  memset(longline, 'a', sizeof(longline) - 2);
  longline[sizeof(longline) - 2] = '\n';
  longline[sizeof(longline) - 1] = '\0';
  TEST_ASSERT_FALSE(feed_str(longline));
  TEST_ASSERT_TRUE(feed_str("ok\n"));            // 下一行正常
  TEST_ASSERT_EQUAL_STRING("ok", lr.buf);
}

void test_two_lines_in_a_row(void) {
  TEST_ASSERT_TRUE(feed_str("a\n"));
  TEST_ASSERT_TRUE(feed_str("b\n"));
  TEST_ASSERT_EQUAL_STRING("b", lr.buf);
}

void test_split_extra_spaces(void) {
  char line[] = "  pwm \t 50   extra ";
  char *argv[4];
  int argc = cli_split(line, argv, 4);
  TEST_ASSERT_EQUAL_INT(3, argc);
  TEST_ASSERT_EQUAL_STRING("pwm", argv[0]);
  TEST_ASSERT_EQUAL_STRING("50", argv[1]);
  TEST_ASSERT_EQUAL_STRING("extra", argv[2]);
}

void test_split_respects_max(void) {
  char line[] = "a b c d e";
  char *argv[2];
  TEST_ASSERT_EQUAL_INT(2, cli_split(line, argv, 2));
}

void test_split_empty(void) {
  char line[] = "   ";
  char *argv[2];
  TEST_ASSERT_EQUAL_INT(0, cli_split(line, argv, 2));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_line_with_crlf);
  RUN_TEST(test_backspace);
  RUN_TEST(test_too_long_line_is_dropped_whole);
  RUN_TEST(test_two_lines_in_a_row);
  RUN_TEST(test_split_extra_spaces);
  RUN_TEST(test_split_respects_max);
  RUN_TEST(test_split_empty);
  return UNITY_END();
}
