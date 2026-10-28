#include "app_image.h"

static uint32_t rd32(const uint8_t *p) {   // 映像檔是 little endian
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int img_parse(img_read_fn rd, void *ctx, uint32_t max_len, img_info_t *out) {
  uint8_t hdr[IMG_HDR_LEN];
  if (max_len < IMG_HDR_LEN || rd(ctx, 0, hdr, IMG_HDR_LEN) != 0) return IMG_ERR_READ;
  if (hdr[0] != IMG_MAGIC) return IMG_ERR_MAGIC;
  out->seg_count = hdr[1];
  out->entry = rd32(&hdr[4]);
  out->hash_appended = hdr[23];
  if (out->seg_count == 0 || out->seg_count > IMG_MAX_SEGS) return IMG_ERR_SEGS;

  uint8_t sum = 0xEF;
  uint32_t off = IMG_HDR_LEN;
  for (int i = 0; i < out->seg_count; i++) {
    uint8_t sh[8];
    if (off + 8 > max_len || rd(ctx, off, sh, 8) != 0) return IMG_ERR_READ;
    img_seg_t *s = &out->seg[i];
    s->load_addr = rd32(&sh[0]);
    s->len = rd32(&sh[4]);
    s->offset = off + 8;
    if (s->len > max_len || s->offset + s->len > max_len) return IMG_ERR_READ;
    uint8_t buf[256];
    for (uint32_t done = 0; done < s->len;) {            // 分段讀，不需要整個映像檔放進 RAM
      uint32_t n = s->len - done < sizeof buf ? s->len - done : sizeof buf;
      if (rd(ctx, s->offset + done, buf, n) != 0) return IMG_ERR_READ;
      for (uint32_t k = 0; k < n; k++) sum ^= buf[k];
      done += n;
    }
    off = s->offset + s->len;
  }

  uint32_t cks_off = (off | 15);                          // 補到 16 的倍數，checksum 在最後一個 byte
  if (cks_off + 1 > max_len || rd(ctx, cks_off, &out->checksum, 1) != 0) return IMG_ERR_READ;
  out->total_len = cks_off + 1 + (out->hash_appended ? 32 : 0);
  if (out->total_len > max_len) return IMG_ERR_READ;
  return out->checksum == sum ? IMG_OK : IMG_ERR_CHECKSUM;
}

const char *img_region(uint32_t a) {
  if (a >= 0x3F400000 && a < 0x3F800000) return "DROM（flash 常數，經快取）";
  if (a >= 0x3FF80000 && a < 0x3FF82000) return "RTC FAST（資料）";
  if (a >= 0x3FFAE000 && a < 0x40000000) return "DRAM（內部 SRAM）";
  if (a >= 0x40070000 && a < 0x400A0000) return "IRAM（內部 SRAM）";
  if (a >= 0x400C0000 && a < 0x400C2000) return "RTC FAST（程式）";
  if (a >= 0x400D0000 && a < 0x40C00000) return "IROM（flash 程式，經快取）";
  if (a >= 0x50000000 && a < 0x50002000) return "RTC SLOW";
  return "其他";
}
