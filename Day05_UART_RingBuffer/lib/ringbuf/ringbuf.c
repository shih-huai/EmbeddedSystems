#include "ringbuf.h"

/*
 * 為什麼用 __atomic 而不只是 volatile：
 * 生產者要先把資料寫進 buf，才更新 head。如果編譯器或 CPU 把這兩步換了順序，
 * 消費者可能看到新的 head 卻讀到還沒寫好的資料。release/acquire 保證這個順序，
 * 在雙核心的 ESP32 上兩個核心各跑一邊也正確。
 */
#define LOAD_RELAXED(p)  __atomic_load_n((p), __ATOMIC_RELAXED)
#define LOAD_ACQUIRE(p)  __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define STORE_RELEASE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)

bool rb_init(ringbuf_t *rb, uint8_t *storage, uint32_t size) {
  if (storage == 0 || size < 2 || (size & (size - 1)) != 0) return false;
  rb->buf = storage;
  rb->mask = size - 1;
  rb->head = 0;
  rb->tail = 0;
  rb->dropped = 0;
  return true;
}

bool rb_put(ringbuf_t *rb, uint8_t byte) {
  uint32_t head = LOAD_RELAXED(&rb->head);   // 自己寫的，不需要同步
  uint32_t tail = LOAD_ACQUIRE(&rb->tail);   // 對方寫的
  if (head - tail > rb->mask) {              // 已經有 size 個：滿了
    rb->dropped++;
    return false;
  }
  rb->buf[head & rb->mask] = byte;
  STORE_RELEASE(&rb->head, head + 1);        // 資料寫好之後才公布
  return true;
}

bool rb_get(ringbuf_t *rb, uint8_t *byte) {
  uint32_t tail = LOAD_RELAXED(&rb->tail);
  uint32_t head = LOAD_ACQUIRE(&rb->head);
  if (head == tail) return false;            // 空的
  *byte = rb->buf[tail & rb->mask];
  STORE_RELEASE(&rb->tail, tail + 1);        // 讀完之後才讓出位置
  return true;
}

uint32_t rb_count(const ringbuf_t *rb) {
  return LOAD_ACQUIRE(&rb->head) - LOAD_ACQUIRE(&rb->tail);
}

uint32_t rb_capacity(const ringbuf_t *rb) {
  return rb->mask + 1;
}
