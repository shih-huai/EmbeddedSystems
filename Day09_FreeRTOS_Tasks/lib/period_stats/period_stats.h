/*
 * 週期統計：記錄每次執行的時間點，算出週期的最小、最大、平均和累積漂移
 * 純 C，可在電腦上測試。
 */
#ifndef PERIOD_STATS_H
#define PERIOD_STATS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t expected_us;   // 期望週期
  uint32_t first_us;      // 第一次的時間點
  uint32_t last_us;
  uint32_t count;         // 收到幾個時間點
  uint32_t min_us, max_us;
} period_stats_t;

void ps_init(period_stats_t *s, uint32_t expected_us);
void ps_add(period_stats_t *s, uint32_t now_us);           // 允許 now_us 溢位繞回
uint32_t ps_avg_us(const period_stats_t *s);               // 平均週期
int32_t ps_drift_us(const period_stats_t *s);              // 實際經過時間 - 期望經過時間（正數 = 越跑越慢）

#ifdef __cplusplus
}
#endif

#endif
