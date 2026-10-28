#include "period_stats.h"

void ps_init(period_stats_t *s, uint32_t expected_us) {
  s->expected_us = expected_us;
  s->first_us = s->last_us = 0;
  s->count = 0;
  s->min_us = UINT32_MAX;
  s->max_us = 0;
}

void ps_add(period_stats_t *s, uint32_t now_us) {
  if (s->count == 0) {
    s->first_us = now_us;
  } else {
    uint32_t p = now_us - s->last_us;    // 無號相減，溢位繞回也正確
    if (p < s->min_us) s->min_us = p;
    if (p > s->max_us) s->max_us = p;
  }
  s->last_us = now_us;
  s->count++;
}

uint32_t ps_avg_us(const period_stats_t *s) {
  if (s->count < 2) return 0;
  return (s->last_us - s->first_us) / (s->count - 1);
}

int32_t ps_drift_us(const period_stats_t *s) {
  if (s->count < 2) return 0;
  uint32_t actual = s->last_us - s->first_us;
  uint32_t expected = s->expected_us * (s->count - 1);
  return (int32_t)(actual - expected);
}
