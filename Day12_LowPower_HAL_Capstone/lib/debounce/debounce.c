#include "debounce.h"

void db_init(debounce_t *d, uint8_t level, uint16_t stable_ms) {
  d->stable = level ? 1 : 0;
  d->last_raw = d->stable;
  d->since_ms = 0;
  d->stable_ms = stable_ms;
}

db_event_t db_update(debounce_t *d, uint8_t raw, uint32_t now_ms) {
  raw = raw ? 1 : 0;
  if (raw != d->last_raw) {              // 電位變了：重新開始計時
    d->last_raw = raw;
    d->since_ms = now_ms;
    return DB_NONE;
  }
  if (raw == d->stable) return DB_NONE;
  if ((uint32_t)(now_ms - d->since_ms) < d->stable_ms) return DB_NONE;   // 無號相減，繞回也正確
  d->stable = raw;
  return raw ? DB_RELEASED : DB_PRESSED;
}
