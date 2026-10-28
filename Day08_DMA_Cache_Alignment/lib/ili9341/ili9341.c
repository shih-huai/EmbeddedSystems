#include "ili9341.h"

static void send_cmd(ili9341_t *dev, uint8_t cmd, const uint8_t *data, size_t len) {
  const lcd_bus_t *b = dev->bus;
  b->cs(b->ctx, 0);
  b->dc(b->ctx, 0);
  b->write(b->ctx, &cmd, 1);
  if (len) {
    b->dc(b->ctx, 1);
    b->write(b->ctx, data, len);
  }
  b->cs(b->ctx, 1);
}

void ili9341_init(ili9341_t *dev, const lcd_bus_t *bus) {
  dev->bus = bus;
  dev->width = ILI9341_WIDTH;
  dev->height = ILI9341_HEIGHT;

  send_cmd(dev, ILI9341_SWRESET, 0, 0);
  bus->delay_ms(bus->ctx, 150);              // 資料手冊：重置後要等 120 ms 以上
  send_cmd(dev, ILI9341_SLPOUT, 0, 0);
  bus->delay_ms(bus->ctx, 120);              // 離開睡眠也要等
  const uint8_t colmod = 0x55;               // 16 位元／像素（RGB565）
  send_cmd(dev, ILI9341_COLMOD, &colmod, 1);
  const uint8_t madctl = 0x48;               // 直立方向、BGR 順序
  send_cmd(dev, ILI9341_MADCTL, &madctl, 1);
  send_cmd(dev, ILI9341_DISPON, 0, 0);
}

static void set_window(ili9341_t *dev, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  // 座標用 16 位元、大端序送出：起點高、起點低、終點高、終點低
  uint8_t col[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
  uint8_t row[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
  send_cmd(dev, ILI9341_CASET, col, 4);
  send_cmd(dev, ILI9341_PASET, row, 4);
}

void ili9341_fill_rect(ili9341_t *dev, int x, int y, int w, int h, uint16_t color) {
  // 裁切到螢幕範圍內，完全在外面就什麼都不做
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > dev->width)  w = dev->width - x;
  if (y + h > dev->height) h = dev->height - y;
  if (w <= 0 || h <= 0) return;

  set_window(dev, (uint16_t)x, (uint16_t)y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

  // 一次送一整塊：先準備一小段同色像素，重複送出。SPI 是高位元組先送，所以顏色要拆成 hi、lo
  enum { CHUNK = 64 };
  uint8_t buf[CHUNK * 2];
  for (int i = 0; i < CHUNK; i++) {
    buf[2 * i] = (uint8_t)(color >> 8);
    buf[2 * i + 1] = (uint8_t)color;
  }
  const lcd_bus_t *b = dev->bus;
  uint8_t cmd = ILI9341_RAMWR;
  b->cs(b->ctx, 0);
  b->dc(b->ctx, 0);
  b->write(b->ctx, &cmd, 1);
  b->dc(b->ctx, 1);
  uint32_t left = (uint32_t)w * (uint32_t)h;
  while (left) {
    uint32_t n = left > CHUNK ? CHUNK : left;
    b->write(b->ctx, buf, n * 2);
    left -= n;
  }
  b->cs(b->ctx, 1);
}

void ili9341_draw_pixel(ili9341_t *dev, int x, int y, uint16_t color) {
  ili9341_fill_rect(dev, x, y, 1, 1, color);
}

void ili9341_begin_ramwr(ili9341_t *dev, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  set_window(dev, x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
  send_cmd(dev, ILI9341_RAMWR, 0, 0);
}
