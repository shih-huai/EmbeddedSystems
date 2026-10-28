#include "debounce.h"

void debounce_init(debounce_t *d, uint32_t hold_ms) {
  d->state = DB_RELEASED;
  d->t_change = 0;
  d->hold_ms = hold_ms;
}

db_event_t debounce_update(debounce_t *d, bool raw_pressed, uint32_t now_ms) {
  // now_ms - t_change 用無號數相減，millis() 溢位繞回時結果仍然正確
  uint32_t elapsed = now_ms - d->t_change;

  switch (d->state) {
    case DB_RELEASED:
      if (raw_pressed) {
        d->state = DB_MAYBE_PRESSED;
        d->t_change = now_ms;
      }
      break;

    case DB_MAYBE_PRESSED:
      if (!raw_pressed) {
        d->state = DB_RELEASED;            // 只是彈跳，退回
      } else if (elapsed >= d->hold_ms) {
        d->state = DB_PRESSED;
        return DB_EVT_PRESS;
      }
      break;

    case DB_PRESSED:
      if (!raw_pressed) {
        d->state = DB_MAYBE_RELEASED;
        d->t_change = now_ms;
      }
      break;

    case DB_MAYBE_RELEASED:
      if (raw_pressed) {
        d->state = DB_PRESSED;             // 只是彈跳，退回
      } else if (elapsed >= d->hold_ms) {
        d->state = DB_RELEASED;
        return DB_EVT_RELEASE;
      }
      break;
  }
  return DB_EVT_NONE;
}
