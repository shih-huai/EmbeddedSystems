#include "sensor_app.h"

static void reset_stats(sensor_app_t *a) {
  a->sum = 0;
  a->n = 0;
  a->min = 0x7FFF;
  a->max = -1;
}

void app_init(sensor_app_t *a, const sensor_hal_t *hal, uint32_t sample_ms, int per_report, uint32_t idle_sleep_ms) {
  a->hal = *hal;
  a->mode = APP_IDLE;
  a->sample_ms = sample_ms;
  a->per_report = per_report;
  a->idle_sleep_ms = idle_sleep_ms;
  uint32_t now = hal->now_ms(hal->ctx);
  a->last_sample = now;
  a->last_activity = now;
  db_init(&a->btn, 1, 20);
  reset_stats(a);
  hal->set_led(hal->ctx, 0);
}

app_action_t app_step(sensor_app_t *a) {
  const sensor_hal_t *h = &a->hal;
  uint32_t now = h->now_ms(h->ctx);

  if (db_update(&a->btn, (uint8_t)h->read_button(h->ctx), now) == DB_PRESSED) {
    a->mode = (a->mode == APP_IDLE) ? APP_SAMPLING : APP_IDLE;
    a->last_activity = now;
    a->last_sample = now;
    reset_stats(a);
    h->set_led(h->ctx, a->mode == APP_SAMPLING);
  }

  if (a->mode == APP_SAMPLING) {
    if ((uint32_t)(now - a->last_sample) >= a->sample_ms) {
      a->last_sample += a->sample_ms;                  // 以預定時間為基準，不漂移
      int v = h->read_sensor(h->ctx);
      a->sum += v;
      if (v < a->min) a->min = v;
      if (v > a->max) a->max = v;
      if (++a->n >= a->per_report) {
        h->report(h->ctx, (int)(a->sum / a->n), a->min, a->max);
        reset_stats(a);
      }
    }
    return APP_CONTINUE;                               // 取樣中不睡
  }
  if ((uint32_t)(now - a->last_activity) >= a->idle_sleep_ms) return APP_SLEEP;
  return APP_CONTINUE;
}
