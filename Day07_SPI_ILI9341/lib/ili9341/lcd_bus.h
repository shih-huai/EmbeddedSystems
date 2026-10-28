/*
 * SPI 螢幕匯流排的抽象介面（HAL）
 *
 * 這類螢幕除了 SPI 的 SCK、MOSI、CS 之外，還多一條 D/C 線：
 * D/C = 0 表示這個位元組是「指令」，D/C = 1 表示是「資料」（參數或像素）。
 */
#ifndef LCD_BUS_H
#define LCD_BUS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  void (*cs)(void *ctx, int level);         // 晶片選擇，0 = 選中
  void (*dc)(void *ctx, int level);         // 0 = 指令、1 = 資料
  void (*write)(void *ctx, const uint8_t *buf, size_t len);
  void (*delay_ms)(void *ctx, uint32_t ms);
  void *ctx;
} lcd_bus_t;

#endif
