#include "timing_calc.h"

#define LEDC_MAX_BITS 20

uint32_t tc_alarm_ticks(uint32_t src_hz, uint32_t prescaler, uint32_t period_us) {
  if (prescaler == 0) return 0;
  // 先乘再除，用 64 位元避免溢位；先除再乘會丟掉小數造成誤差
  uint64_t ticks = (uint64_t)src_hz * period_us / ((uint64_t)prescaler * 1000000u);
  return ticks > UINT32_MAX ? 0 : (uint32_t)ticks;
}

uint8_t tc_pwm_max_bits(uint32_t src_hz, uint32_t pwm_hz) {
  if (pwm_hz == 0) return 0;
  uint32_t ratio = src_hz / pwm_hz;   // 一個 PWM 週期裡有幾個時脈
  uint8_t bits = 0;
  while (bits < LEDC_MAX_BITS && (ratio >> (bits + 1)) != 0) bits++;
  return bits;
}

uint32_t tc_duty_from_permille(uint8_t bits, uint32_t permille) {
  if (permille > 1000) permille = 1000;
  uint32_t max = (1u << bits) - 1;
  return (uint32_t)(((uint64_t)max * permille + 500) / 1000);
}
