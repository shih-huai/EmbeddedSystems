/*
 * I2C 匯流排的抽象介面（HAL）
 *
 * 驅動只認得這組函式指標，不知道底下是 Arduino Wire、ESP-IDF 還是測試用的假匯流排。
 * 回傳 0 代表成功，負數代表錯誤（例如位址沒有 ACK）。
 */
#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  /* START、位址+W、data[0..len-1]、STOP */
  int (*write)(void *ctx, uint8_t addr, const uint8_t *data, size_t len);
  /* START、位址+W、tx、repeated START、位址+R、讀 rxlen 個位元組、STOP */
  int (*write_read)(void *ctx, uint8_t addr, const uint8_t *tx, size_t txlen,
                    uint8_t *rx, size_t rxlen);
  void *ctx;
} i2c_bus_t;

#endif
