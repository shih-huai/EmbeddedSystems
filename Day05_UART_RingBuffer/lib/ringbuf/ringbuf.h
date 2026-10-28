/*
 * 單一生產者、單一消費者（SPSC）的無鎖 ring buffer
 *
 * 規則：只有生產者呼叫 rb_put()（例如 UART 接收 ISR），只有消費者呼叫 rb_get()（例如主迴圈）。
 * 這樣 head 只有生產者寫、tail 只有消費者寫，兩邊都不需要上鎖或關中斷。
 * 容量必須是 2 的次方，索引用「一直往上加」的 uint32_t，取餘數改成 & mask。
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t *buf;
  uint32_t mask;      // 容量 - 1
  uint32_t head;      // 下一個要寫的位置，只有生產者改
  uint32_t tail;      // 下一個要讀的位置，只有消費者改
  uint32_t dropped;   // 滿了被丟掉的位元組數，只有生產者改
} ringbuf_t;

/* storage 由呼叫端提供（通常是 static 陣列），size 必須是 2 的次方。 */
bool rb_init(ringbuf_t *rb, uint8_t *storage, uint32_t size);

bool rb_put(ringbuf_t *rb, uint8_t byte);    // 滿了回傳 false，並累加 dropped
bool rb_get(ringbuf_t *rb, uint8_t *byte);   // 空的回傳 false
uint32_t rb_count(const ringbuf_t *rb);      // 目前有幾個位元組（任一方呼叫都可以，結果可能馬上過時）
uint32_t rb_capacity(const ringbuf_t *rb);

#ifdef __cplusplus
}
#endif

#endif
