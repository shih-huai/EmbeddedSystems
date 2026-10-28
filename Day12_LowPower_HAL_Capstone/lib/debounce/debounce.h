/*
 * 按鈕去彈跳：輸入「現在讀到的電位」和「現在時間」，電位穩定夠久才算數
 * 純 C，可在電腦上測試。按鈕接法是 active low（按下 = 0）。
 */
#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { DB_NONE = 0, DB_PRESSED, DB_RELEASED } db_event_t;

typedef struct {
  uint8_t stable;        // 目前認定的穩定電位
  uint8_t last_raw;      // 上一次讀到的電位
  uint32_t since_ms;     // last_raw 從什麼時候開始沒變
  uint16_t stable_ms;    // 要穩定多久才算數
} debounce_t;

void db_init(debounce_t *d, uint8_t level, uint16_t stable_ms);
db_event_t db_update(debounce_t *d, uint8_t raw, uint32_t now_ms);   // 允許 now_ms 溢位繞回

#ifdef __cplusplus
}
#endif

#endif
