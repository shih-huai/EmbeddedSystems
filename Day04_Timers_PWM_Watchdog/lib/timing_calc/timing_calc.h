/*
 * 計時器與 PWM 參數計算（純 C，不碰硬體，可在電腦上做單元測試）
 */
#ifndef TIMING_CALC_H
#define TIMING_CALC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 計時器要數幾個 tick 才到 period_us。
 * src_hz：計時器時脈（ESP32 的 APB = 80 MHz），prescaler：除頻值。
 * 結果超過 32 位元或 prescaler 為 0 時回傳 0。 */
uint32_t tc_alarm_ticks(uint32_t src_hz, uint32_t prescaler, uint32_t period_us);

/* PWM 在 pwm_hz 下最多能有幾個位元的解析度：2^bits * pwm_hz <= src_hz。
 * ESP32 LEDC 最多 20 位元；連 1 位元都做不到時回傳 0。 */
uint8_t tc_pwm_max_bits(uint32_t src_hz, uint32_t pwm_hz);

/* 把 0～1000（千分比）的亮度換成 bits 位元解析度下的 duty 值（四捨五入）。 */
uint32_t tc_duty_from_permille(uint8_t bits, uint32_t permille);

#ifdef __cplusplus
}
#endif

#endif
