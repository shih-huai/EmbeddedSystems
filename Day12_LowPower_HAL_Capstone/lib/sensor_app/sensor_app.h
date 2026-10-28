/*
 * 總複習的小應用：按鈕切換「閒置／取樣」，取樣時定時讀感測器、每 N 筆回報平均；閒置太久就要求睡眠。
 * 應用邏輯只透過 sensor_hal_t 碰硬體，所以可以在電腦上用假的 HAL 測試，換晶片時只要重寫 HAL。
 */
#ifndef SENSOR_APP_H
#define SENSOR_APP_H

#include <stdint.h>
#include "debounce.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t (*now_ms)(void *ctx);
  int (*read_button)(void *ctx);          // 原始電位：1 = 放開，0 = 按下
  int (*read_sensor)(void *ctx);          // 0..4095
  void (*set_led)(void *ctx, int on);
  void (*report)(void *ctx, int avg, int min, int max);
  void *ctx;
} sensor_hal_t;

typedef enum { APP_IDLE = 0, APP_SAMPLING } app_mode_t;
typedef enum { APP_CONTINUE = 0, APP_SLEEP } app_action_t;

typedef struct {
  sensor_hal_t hal;
  debounce_t btn;
  app_mode_t mode;
  uint32_t sample_ms, idle_sleep_ms;
  int per_report;
  uint32_t last_sample, last_activity;
  int32_t sum;
  int n, min, max;
} sensor_app_t;

void app_init(sensor_app_t *a, const sensor_hal_t *hal, uint32_t sample_ms, int per_report, uint32_t idle_sleep_ms);
app_action_t app_step(sensor_app_t *a);    // 主迴圈每幾 ms 呼叫一次

#ifdef __cplusplus
}
#endif

#endif
