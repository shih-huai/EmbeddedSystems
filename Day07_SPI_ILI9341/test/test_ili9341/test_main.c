/*
 * ILI9341 驅動的主機端單元測試：假匯流排記錄每一個指令和資料位元組
 */
#include <string.h>
#include <unity.h>
#include "ili9341.h"

typedef struct {
  uint8_t cmds[64];            // 依序收到的指令
  int ncmd;
  uint8_t last_params[2][4];   // 最近一次 CASET、PASET 的參數
  uint32_t pixel_bytes;        // RAMWR 之後收到的資料位元組數
  uint8_t first_pixel[2];
  int dc, cs;
  int cs_errors;               // 在 CS 沒選中時寫資料的次數
  uint32_t delay_total;
} fake_lcd_t;

static void f_cs(void *ctx, int level) { ((fake_lcd_t *)ctx)->cs = level; }
static void f_dc(void *ctx, int level) { ((fake_lcd_t *)ctx)->dc = level; }
static void f_delay(void *ctx, uint32_t ms) { ((fake_lcd_t *)ctx)->delay_total += ms; }
static void f_write(void *ctx, const uint8_t *buf, size_t len) {
  fake_lcd_t *f = ctx;
  if (f->cs != 0) f->cs_errors++;
  if (f->dc == 0) {                       // 指令
    for (size_t i = 0; i < len; i++) f->cmds[f->ncmd++] = buf[i];
    return;
  }
  uint8_t last = f->cmds[f->ncmd - 1];
  if (last == ILI9341_CASET) memcpy(f->last_params[0], buf, 4);
  if (last == ILI9341_PASET) memcpy(f->last_params[1], buf, 4);
  if (last == ILI9341_RAMWR) {
    if (f->pixel_bytes == 0) memcpy(f->first_pixel, buf, 2);
    f->pixel_bytes += (uint32_t)len;
  }
}

static fake_lcd_t fake;
static lcd_bus_t bus = {f_cs, f_dc, f_write, f_delay, &fake};
static ili9341_t lcd;

void setUp(void) {
  memset(&fake, 0, sizeof(fake));
  fake.cs = 1;
  ili9341_init(&lcd, &bus);
}
void tearDown(void) {}

void test_init_sequence_and_delays(void) {
  const uint8_t expect[] = {ILI9341_SWRESET, ILI9341_SLPOUT, ILI9341_COLMOD, ILI9341_MADCTL, ILI9341_DISPON};
  TEST_ASSERT_EQUAL_INT(5, fake.ncmd);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, fake.cmds, 5);
  TEST_ASSERT_TRUE(fake.delay_total >= 240);
  TEST_ASSERT_EQUAL_INT(1, fake.cs);       // 結束時 CS 要放開
}

void test_fill_rect_window_and_pixel_count(void) {
  ili9341_fill_rect(&lcd, 10, 300, 20, 5, 0xF800);
  const uint8_t col[4] = {0, 10, 0, 29};
  const uint8_t row[4] = {0x01, 0x2C, 0x01, 0x30};   // 300 = 0x012C，304 = 0x0130
  TEST_ASSERT_EQUAL_HEX8_ARRAY(col, fake.last_params[0], 4);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(row, fake.last_params[1], 4);
  TEST_ASSERT_EQUAL_UINT32(20 * 5 * 2, fake.pixel_bytes);
  TEST_ASSERT_EQUAL_HEX8(0xF8, fake.first_pixel[0]);   // 高位元組先送
  TEST_ASSERT_EQUAL_HEX8(0x00, fake.first_pixel[1]);
  TEST_ASSERT_EQUAL_INT(0, fake.cs_errors);
}

void test_fill_rect_clips_to_screen(void) {
  ili9341_fill_rect(&lcd, -5, 315, 10, 10, 0xFFFF);    // 左下角超出
  const uint8_t col[4] = {0, 0, 0, 4};
  const uint8_t row[4] = {0x01, 0x3B, 0x01, 0x3F};     // 315..319
  TEST_ASSERT_EQUAL_HEX8_ARRAY(col, fake.last_params[0], 4);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(row, fake.last_params[1], 4);
  TEST_ASSERT_EQUAL_UINT32(5 * 5 * 2, fake.pixel_bytes);
}

void test_fill_rect_fully_outside_does_nothing(void) {
  int before = fake.ncmd;
  ili9341_fill_rect(&lcd, 240, 0, 10, 10, 0);
  ili9341_fill_rect(&lcd, 0, -20, 10, 10, 0);
  TEST_ASSERT_EQUAL_INT(before, fake.ncmd);
}

void test_full_screen_pixel_count(void) {
  ili9341_fill_screen(&lcd, 0x001F);
  TEST_ASSERT_EQUAL_UINT32(240u * 320u * 2u, fake.pixel_bytes);
}

void test_rgb565(void) {
  TEST_ASSERT_EQUAL_HEX16(0xF800, rgb565(255, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x07E0, rgb565(0, 255, 0));
  TEST_ASSERT_EQUAL_HEX16(0x001F, rgb565(0, 0, 255));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(255, 255, 255));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_init_sequence_and_delays);
  RUN_TEST(test_fill_rect_window_and_pixel_count);
  RUN_TEST(test_fill_rect_clips_to_screen);
  RUN_TEST(test_fill_rect_fully_outside_does_nothing);
  RUN_TEST(test_full_screen_pixel_count);
  RUN_TEST(test_rgb565);
  return UNITY_END();
}
