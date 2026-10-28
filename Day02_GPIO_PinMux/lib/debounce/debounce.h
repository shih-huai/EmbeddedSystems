/*
 * 按鈕去彈跳狀態機（純 C，不碰硬體）
 *
 * 不讀腳位、不呼叫 millis()：電位與時間都由呼叫端傳進來。
 * 這樣同一份程式碼可以在 ESP32 上跑，也可以在電腦上做單元測試。
 */
#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  DB_RELEASED,
  DB_MAYBE_PRESSED,
  DB_PRESSED,
  DB_MAYBE_RELEASED,
} db_state_t;

typedef enum {
  DB_EVT_NONE,
  DB_EVT_PRESS,
  DB_EVT_RELEASE,
} db_event_t;

typedef struct {
  db_state_t state;
  uint32_t t_change;   // 進入 MAYBE_* 狀態的時間
  uint32_t hold_ms;    // 電位要穩定多久才承認
} debounce_t;

void debounce_init(debounce_t *d, uint32_t hold_ms);

/* raw_pressed：這次讀到的電位（已換算成「按下 = true」）
 * now_ms：目前時間，允許 uint32_t 溢位繞回
 * 回傳：這次呼叫是否產生了按下／放開事件 */
db_event_t debounce_update(debounce_t *d, bool raw_pressed, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
