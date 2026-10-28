/*
 * ILI9341 240x320 TFT 螢幕的最小驅動：初始化、設定視窗、填矩形
 * 只依賴 lcd_bus_t，可以在電腦上用假匯流排測試。
 */
#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>
#include "lcd_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9341_WIDTH  240
#define ILI9341_HEIGHT 320

/* 指令（ILI9341 資料手冊第 8 章） */
#define ILI9341_SWRESET 0x01
#define ILI9341_SLPOUT  0x11
#define ILI9341_DISPON  0x29
#define ILI9341_CASET   0x2A   // 欄位範圍
#define ILI9341_PASET   0x2B   // 列範圍
#define ILI9341_RAMWR   0x2C   // 開始寫像素
#define ILI9341_MADCTL  0x36   // 掃描方向與 RGB/BGR
#define ILI9341_COLMOD  0x3A   // 像素格式

typedef struct {
  const lcd_bus_t *bus;
  uint16_t width, height;
} ili9341_t;

/* 8 位元 RGB 轉成 RGB565：紅 5 位元、綠 6 位元、藍 5 位元 */
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void ili9341_init(ili9341_t *dev, const lcd_bus_t *bus);
void ili9341_fill_rect(ili9341_t *dev, int x, int y, int w, int h, uint16_t color);
void ili9341_draw_pixel(ili9341_t *dev, int x, int y, uint16_t color);
static inline void ili9341_fill_screen(ili9341_t *dev, uint16_t color) {
  ili9341_fill_rect(dev, 0, 0, dev->width, dev->height, color);
}

#ifdef __cplusplus
}
#endif

#endif
