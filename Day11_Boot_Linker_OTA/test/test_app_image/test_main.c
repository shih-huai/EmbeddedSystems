/*
 * app_image 的主機端單元測試（Unity）：在記憶體裡組一個假的映像檔
 */
#include <string.h>
#include <unity.h>
#include "app_image.h"

static uint8_t img[256];
static uint32_t img_len;

static int mem_read(void *ctx, uint32_t off, void *buf, uint32_t len) {
  (void)ctx;
  if (off + len > img_len) return -1;
  memcpy(buf, img + off, len);
  return 0;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

// 兩個 segment：IRAM 5 bytes、DRAM 3 bytes
static void build(int hash) {
  memset(img, 0, sizeof img);
  img[0] = IMG_MAGIC; img[1] = 2;
  put32(&img[4], 0x40080404);
  img[23] = (uint8_t)hash;
  uint32_t off = 24;
  uint8_t sum = 0xEF;
  const uint8_t a[] = {1, 2, 3, 4, 5}, b[] = {0xAA, 0x55, 0x0F};
  put32(&img[off], 0x40080000); put32(&img[off + 4], 5); memcpy(&img[off + 8], a, 5); off += 13;
  put32(&img[off], 0x3FFB0000); put32(&img[off + 4], 3); memcpy(&img[off + 8], b, 3); off += 11;
  for (int i = 0; i < 5; i++) sum ^= a[i];
  for (int i = 0; i < 3; i++) sum ^= b[i];
  off |= 15;
  img[off] = sum;
  img_len = off + 1 + (hash ? 32 : 0);
}

void setUp(void) { build(0); }
void tearDown(void) {}

void test_parse_ok(void) {
  img_info_t info;
  TEST_ASSERT_EQUAL(IMG_OK, img_parse(mem_read, NULL, img_len, &info));
  TEST_ASSERT_EQUAL(2, info.seg_count);
  TEST_ASSERT_EQUAL_HEX32(0x40080404, info.entry);
  TEST_ASSERT_EQUAL_HEX32(0x3FFB0000, info.seg[1].load_addr);
  TEST_ASSERT_EQUAL_UINT32(24 + 13 + 8, info.seg[1].offset);   // 檔頭 + 第一段（8+5）+ 第二段的 segment 檔頭
}

void test_checksum_position(void) {
  // 檔頭 24 + segment 13 + 11 = 48；48|15 = 63，checksum 在 offset 63，總長 64
  img_info_t info;
  img_parse(mem_read, NULL, img_len, &info);
  TEST_ASSERT_EQUAL_UINT32(64, info.total_len);
}

void test_hash_appended_adds_32(void) {
  build(1);
  img_info_t info;
  TEST_ASSERT_EQUAL(IMG_OK, img_parse(mem_read, NULL, img_len, &info));
  TEST_ASSERT_EQUAL_UINT32(96, info.total_len);
}

void test_corrupted_byte_detected(void) {
  img[24 + 8 + 2] ^= 0x01;                          // 改掉一個 bit
  img_info_t info;
  TEST_ASSERT_EQUAL(IMG_ERR_CHECKSUM, img_parse(mem_read, NULL, img_len, &info));
}

void test_bad_magic(void) {
  img[0] = 0xFF;                                    // 已抹除的 flash 讀起來全是 0xFF
  img_info_t info;
  TEST_ASSERT_EQUAL(IMG_ERR_MAGIC, img_parse(mem_read, NULL, img_len, &info));
}

void test_truncated(void) {
  img_info_t info;
  TEST_ASSERT_EQUAL(IMG_ERR_READ, img_parse(mem_read, NULL, 40, &info));
}

void test_region(void) {
  TEST_ASSERT_EQUAL_STRING("IRAM（內部 SRAM）", img_region(0x40080000));
  TEST_ASSERT_EQUAL_STRING("IROM（flash 程式，經快取）", img_region(0x400D1234));
  TEST_ASSERT_EQUAL_STRING("RTC SLOW", img_region(0x50000010));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_parse_ok);
  RUN_TEST(test_checksum_position);
  RUN_TEST(test_hash_appended_adds_32);
  RUN_TEST(test_corrupted_byte_detected);
  RUN_TEST(test_bad_magic);
  RUN_TEST(test_truncated);
  RUN_TEST(test_region);
  return UNITY_END();
}
