#include "dma_plan.h"

size_t dma_plan(size_t total, size_t max_chunk, size_t align, size_t out[], size_t max_out) {
  if (align == 0 || max_chunk < align) return 0;
  size_t step = max_chunk - (max_chunk % align);   // 每段的最大長度，往下取到 align 的倍數
  size_t n = 0;
  while (total > 0) {
    if (n == max_out) return 0;
    size_t len = total > step ? step : total;
    out[n++] = len;
    total -= len;
  }
  return n;
}
