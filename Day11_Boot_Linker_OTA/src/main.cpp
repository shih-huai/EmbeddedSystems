/*
 * Day 11：開機流程、linker、分割表、OTA
 *
 * 不需要外接元件，看 Serial Monitor 就好。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：各種變數和函式被 linker 放到哪裡，以及正在執行的韌體映像檔有哪些 segment
 *   1 = 練習 1：分割表、目前從哪個分割區開機、reset 原因；看哪些變數在重開機後還留著（會自己重開機兩次）
 *   2 = 練習 2：自己做一次 OTA。把目前的韌體複製到另一個 OTA 分割區，先故意寫壞一個 byte 看驗證擋下來，
 *       再寫正確的版本、切換開機分割區、重開機（會自己重開機兩次）
 *   3 = 練習 3：setup() 之前發生了什麼：constructor、C++ 全域物件、loopTask
 */
#include <Arduino.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_rom_sys.h>
#include <esp_log.h>
#include "app_image.h"

#define EXERCISE 0

// 讓 app_image 從 flash 分割區讀資料
__attribute__((unused)) static int part_read(void *ctx, uint32_t off, void *buf, uint32_t len) {
  return esp_partition_read((const esp_partition_t *)ctx, off, buf, len) == ESP_OK ? 0 : -1;
}

__attribute__((unused)) static const char *reset_name(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "上電（POWERON）";
    case ESP_RST_EXT:       return "外部 reset 腳（EXT）";
    case ESP_RST_SW:        return "軟體 esp_restart（SW）";
    case ESP_RST_PANIC:     return "例外或 panic（PANIC）";
    case ESP_RST_INT_WDT:   return "中斷看門狗（INT_WDT）";
    case ESP_RST_TASK_WDT:  return "task 看門狗（TASK_WDT）";
    case ESP_RST_WDT:       return "其他看門狗（WDT）";
    case ESP_RST_DEEPSLEEP: return "深度睡眠喚醒（DEEPSLEEP）";
    case ESP_RST_BROWNOUT:  return "電壓不足（BROWNOUT）";
    default:                return "其他";
  }
}

#if EXERCISE == 0
const char flash_str[] = "我是常數字串";        // .rodata：留在 flash，透過快取讀
static int data_var = 42;                        // .data：有初始值，開機時要放進 RAM
static int bss_var;                              // .bss：沒有初始值，開機時清成 0
RTC_DATA_ATTR static int rtc_var = 7;            // RTC slow memory：深度睡眠時不斷電
static void IRAM_ATTR iram_fn() { data_var++; }  // .iram1：放在內部 RAM，快取關閉時也能執行
static void flash_fn() { bss_var++; }            // .text：留在 flash，透過快取執行

extern "C" int _data_start, _data_end, _bss_start, _bss_end;   // linker script 定義的符號

static void show(const char *what, const void *p) {
  Serial.printf("  %-28s %p  %s\n", what, p, img_region((uint32_t)p));
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Day 11 EXERCISE 0：東西放在哪裡");
  iram_fn();
  flash_fn();
  int local = rtc_var;
  void *heap = malloc(16);
  show("常數字串 flash_str", flash_str);
  show("一般函式 flash_fn", (void *)flash_fn);
  show("IRAM_ATTR 函式 iram_fn", (void *)iram_fn);
  show("有初值的全域 data_var", &data_var);
  show("沒初值的全域 bss_var", &bss_var);
  show("RTC_DATA_ATTR rtc_var", &rtc_var);
  show("malloc 的記憶體", heap);
  show("區域變數（task 堆疊）", &local);
  Serial.printf("  .data %p..%p（%u bytes），.bss %p..%p（%u bytes）\n",
                &_data_start, &_data_end, (unsigned)((char *)&_data_end - (char *)&_data_start),
                &_bss_start, &_bss_end, (unsigned)((char *)&_bss_end - (char *)&_bss_start));
  free(heap);

  const esp_partition_t *run = esp_ota_get_running_partition();
  img_info_t info;
  int r = img_parse(part_read, (void *)run, run->size, &info);
  Serial.printf("\n正在執行的映像檔（分割區 %s @0x%06x）：解析結果 %d，進入點 0x%08x，總長 %u bytes\n",
                run->label, (unsigned)run->address, r, (unsigned)info.entry, (unsigned)info.total_len);
  for (int i = 0; i < info.seg_count; i++)
    Serial.printf("  segment %d：載入到 0x%08x，%6u bytes  %s\n", i, (unsigned)info.seg[i].load_addr,
                  (unsigned)info.seg[i].len, img_region(info.seg[i].load_addr));
}
void loop() { delay(1000); }

#elif EXERCISE == 1
RTC_NOINIT_ATTR static uint32_t noinit_magic;    // 任何 reset 都不會初始化（上電時是亂數）
RTC_NOINIT_ATTR static uint32_t noinit_boots;
RTC_DATA_ATTR static uint32_t rtc_boots = 0;     // 只有深度睡眠喚醒時保留，其他 reset 會重新載入初值
static uint32_t ram_boots = 0;                   // 一般 .bss，每次開機都是 0

void setup() {
  Serial.begin(115200);
  delay(100);
  if (noinit_magic != 0xB007B007) { noinit_magic = 0xB007B007; noinit_boots = 0; }   // 上電後先驗證再用
  noinit_boots++; rtc_boots++; ram_boots++;
  esp_reset_reason_t why = esp_reset_reason();
  Serial.printf("\n==== Day 11 EXERCISE 1：第 %u 次開機，原因：%s ====\n", (unsigned)noinit_boots, reset_name(why));
  Serial.printf("計數器：RTC_NOINIT %u、RTC_DATA %u、一般全域 %u\n",
                (unsigned)noinit_boots, (unsigned)rtc_boots, (unsigned)ram_boots);

  if (noinit_boots == 1) {
    Serial.println("分割表：");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr);
    for (; it; it = esp_partition_next(it)) {
      const esp_partition_t *p = esp_partition_get(it);
      Serial.printf("  %-9s 類型 %d/0x%02x  位址 0x%06x  大小 %4u KB\n", p->label, p->type, p->subtype,
                    (unsigned)p->address, (unsigned)(p->size / 1024));
    }
    esp_partition_iterator_release(it);
    const esp_app_desc_t *d = esp_ota_get_app_description();
    Serial.printf("執行中：%s；開機設定：%s；韌體 %s，編譯於 %s %s，IDF %s\n",
                  esp_ota_get_running_partition()->label, esp_ota_get_boot_partition()->label,
                  d->project_name, d->date, d->time, d->idf_ver);
    Serial.println("2 秒後用 esp_restart() 軟體重開機...");
    delay(2000);
    esp_restart();
  } else if (noinit_boots == 2) {
    Serial.println("2 秒後故意讀 NULL 指標，讓它 panic 重開機...");
    delay(2000);
    volatile uint32_t *bad = nullptr;
    Serial.println(*bad);
  } else {
    Serial.println("結束。RTC_NOINIT 撐過了兩次 reset，RTC_DATA 和一般全域都回到初值。");
  }
}
void loop() { delay(1000); }

#elif EXERCISE == 2
RTC_NOINIT_ATTR static uint32_t ota_magic;
RTC_NOINIT_ATTR static uint32_t ota_stage;

static esp_err_t copy_image(const esp_partition_t *src, const esp_partition_t *dst, uint32_t len, int corrupt_at) {
  esp_ota_handle_t h;
  esp_err_t e = esp_ota_begin(dst, len, &h);           // 先抹除 dst 需要的區塊
  if (e != ESP_OK) return e;
  static uint8_t buf[4096];
  for (uint32_t off = 0; off < len; off += sizeof buf) {
    uint32_t n = len - off < sizeof buf ? len - off : sizeof buf;
    esp_partition_read(src, off, buf, n);
    if (corrupt_at >= 0 && (uint32_t)corrupt_at >= off && (uint32_t)corrupt_at < off + n)
      buf[corrupt_at - off] ^= 0x01;                    // 模擬傳輸時錯了一個 bit
    e = esp_ota_write(h, buf, n);
    if (e != ESP_OK) { esp_ota_abort(h); return e; }
  }
  return esp_ota_end(h);                                // 這裡會驗證整個映像檔（checksum、SHA-256）
}

void setup() {
  Serial.begin(115200);
  delay(100);
  if (ota_magic != 0x07A07A00) { ota_magic = 0x07A07A00; ota_stage = 0; }
  const esp_partition_t *run = esp_ota_get_running_partition();
  Serial.printf("\n==== Day 11 EXERCISE 2：階段 %u，正在從 %s 執行 ====\n", (unsigned)ota_stage, run->label);

  if (ota_stage == 0) {
    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    img_info_t info;
    if (img_parse(part_read, (void *)run, run->size, &info) != IMG_OK) { Serial.println("讀不到自己的映像檔"); return; }
    Serial.printf("自己的映像檔 %u bytes，要寫到 %s\n", (unsigned)info.total_len, next->label);

    uint32_t t0 = millis();
    esp_err_t e = copy_image(run, next, info.total_len, info.seg[1].offset + 10);
    Serial.printf("寫入「壞掉一個 bit」的版本：%s（%lu ms）\n", esp_err_to_name(e), (unsigned long)(millis() - t0));
    Serial.printf("開機分割區還是 %s\n", esp_ota_get_boot_partition()->label);

    t0 = millis();
    e = copy_image(run, next, info.total_len, -1);
    Serial.printf("寫入正確的版本：%s（%lu ms）\n", esp_err_to_name(e), (unsigned long)(millis() - t0));
    if (e != ESP_OK) return;
    esp_ota_set_boot_partition(next);                   // 寫 otadata：下次開機從 next 開始
    Serial.printf("開機分割區改成 %s，重開機...\n", esp_ota_get_boot_partition()->label);
    ota_stage = 1;
    delay(500);
    esp_restart();
  } else if (ota_stage == 1) {
    Serial.println("OTA 成功：bootloader 讀了 otadata，從新的分割區開機。");
    Serial.println("（真實產品在這裡做自我檢查，通過後呼叫 esp_ota_mark_app_valid_cancel_rollback()）");
    const esp_partition_t *other = esp_ota_get_next_update_partition(nullptr);
    esp_ota_set_boot_partition(other);                  // 練習結束，切回原來的分割區
    Serial.printf("把開機分割區切回 %s，重開機...\n", other->label);
    ota_stage = 2;
    delay(500);
    esp_restart();
  } else {
    Serial.println("回到原來的分割區，練習結束。");
    ota_magic = 0;
  }
}
void loop() { delay(1000); }

#elif EXERCISE == 3
static int order = 0;
static int ctor_order = 0, global_obj_order = 0;
static uint32_t ctor_ms = 0;

__attribute__((constructor)) static void early_init() {   // 在 app_main / setup 之前執行
  ctor_order = ++order;
  ctor_ms = esp_log_timestamp();
  esp_rom_printf("[constructor] 這時 Serial 還沒初始化，只能用 ROM 的 printf（UART0 已經被 bootloader 設好）\n");
}

struct GlobalThing {
  int value;
  GlobalThing() : value(123) {                             // C++ 全域物件的建構子，也在 setup 之前
    global_obj_order = ++order;
  }
};
static GlobalThing thing;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Day 11 EXERCISE 3：setup() 之前發生了什麼");
  Serial.printf("constructor 第 %d 個執行（開機後 %lu ms）；全域物件第 %d 個建構，value=%d\n",
                ctor_order, (unsigned long)ctor_ms, global_obj_order, thing.value);
  Serial.printf("setup() 跑在 task「%s」，核心 %d，優先權 %u，開機後 %lu ms\n", pcTaskGetName(nullptr),
                xPortGetCoreID(), (unsigned)uxTaskPriorityGet(nullptr), (unsigned long)millis());
  Serial.printf("剩餘 heap %u bytes，內部 RAM 最大連續區塊 %u bytes\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  Serial.printf("flash %u MB，晶片版本 %d，CPU %u MHz\n", (unsigned)(ESP.getFlashChipSize() >> 20),
                ESP.getChipRevision(), (unsigned)getCpuFrequencyMhz());
}
void loop() { delay(1000); }
#endif
