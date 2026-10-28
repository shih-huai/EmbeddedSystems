/*
 * 把一次大傳輸切成多段 DMA 傳輸（純 C，可在電腦上測試）
 *
 * DMA 硬體通常限制每一段（descriptor）的最大長度，而且要求長度與位址對齊。
 * ESP32 的 DMA descriptor 每段最多 4095 bytes，SPI 搭配 DMA 時長度最好是 4 的倍數。
 */
#ifndef DMA_PLAN_H
#define DMA_PLAN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 把 total 個位元組切成多段，每段不超過 max_chunk，且除了最後一段，每段長度都是 align 的倍數。
 * 段長寫進 out[]，回傳段數；out 放不下、或參數不合理（align 為 0、max_chunk < align）時回傳 0。 */
size_t dma_plan(size_t total, size_t max_chunk, size_t align, size_t out[], size_t max_out);

/* 位址是否對齊到 align（align 必須是 2 的次方） */
static inline int dma_is_aligned(const void *p, size_t align) {
  return ((uintptr_t)p & (align - 1)) == 0;
}

#ifdef __cplusplus
}
#endif

#endif
