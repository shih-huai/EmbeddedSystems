/*
 * 解析 ESP32 的韌體映像檔（app image）格式
 * 純 C，可在電腦上測試。讀資料透過函式指標，所以同一份程式可以讀 flash 分割區，也可以讀記憶體中的陣列。
 *
 * 格式：
 *   檔頭 24 bytes：magic 0xE9、segment 數、SPI 模式、SPI 速度/大小、進入點（4 bytes）、延伸檔頭 16 bytes
 *   每個 segment：載入位址（4 bytes）、長度（4 bytes）、資料
 *   補 0 到「下一個位元組會落在 16 的倍數」，最後 1 byte 是 checksum（0xEF XOR 所有 segment 資料）
 *   延伸檔頭的 hash_appended = 1 時，後面再接 32 bytes SHA-256
 */
#ifndef APP_IMAGE_H
#define APP_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMG_MAGIC      0xE9
#define IMG_HDR_LEN    24
#define IMG_MAX_SEGS   16

enum {
  IMG_OK = 0,
  IMG_ERR_READ = -1,       // 讀取失敗或超出範圍
  IMG_ERR_MAGIC = -2,      // 第一個 byte 不是 0xE9
  IMG_ERR_SEGS = -3,       // segment 數是 0 或太多
  IMG_ERR_CHECKSUM = -4,   // checksum 不符
};

typedef struct {
  uint32_t load_addr;      // 開機時要放到哪個位址
  uint32_t len;
  uint32_t offset;         // 資料在映像檔中的位置
} img_seg_t;

typedef struct {
  uint32_t entry;          // 進入點位址
  uint8_t seg_count;
  uint8_t hash_appended;
  img_seg_t seg[IMG_MAX_SEGS];
  uint8_t checksum;        // 映像檔裡記錄的 checksum
  uint32_t total_len;      // 整個映像檔的長度（含 checksum 和 SHA-256）
} img_info_t;

// 從 off 讀 len bytes 到 buf，成功回傳 0
typedef int (*img_read_fn)(void *ctx, uint32_t off, void *buf, uint32_t len);

int img_parse(img_read_fn rd, void *ctx, uint32_t max_len, img_info_t *out);
const char *img_region(uint32_t addr);   // 位址屬於哪一塊記憶體

#ifdef __cplusplus
}
#endif

#endif
