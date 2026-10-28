/*
 * Day 02：GPIO 與 pin mux
 *
 * 三顆 LED：GPIO18（紅）、GPIO19（黃）、GPIO21（綠），各串 220Ω 到 GND。
 * 按鈕：GPIO4 到 GND（內部上拉，按下為 0）。
 * 不再用 GPIO2：它是 strapping pin，開機時的電位會影響開機模式。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂實驗：沒有去彈跳，每次偵測到按下就計數，看一次按下被算成幾次
 *   1 = 練習 1 參考解：用 lib/debounce 狀態機，一次按下只算一次
 *   2 = 練習 2 參考解：按鈕切換 3 種 LED 模式（全滅 → 跑馬燈 → 三顆一起閃）
 *   3 = 練習 3 參考解：不呼叫 pinMode，自己寫 IO_MUX 與 GPIO matrix 暫存器
 * 建議先自己寫練習 1～3，再對照參考解。
 */
#include <Arduino.h>
#include "soc/io_mux_reg.h"     // PIN_FUNC_SELECT、FUN_PU 等 IO_MUX 欄位
#include "soc/gpio_periph.h"    // GPIO_PIN_MUX_REG[]：GPIO 編號 → IO_MUX 暫存器位址
#include "soc/gpio_reg.h"       // GPIO_OUT_W1TS_REG、GPIO_FUNCn_OUT_SEL_CFG_REG 等
#include "soc/gpio_sig_map.h"   // SIG_GPIO_OUT_IDX（= 256）
#include "debounce.h"

#define EXERCISE 2

const int LEDS[] = {18, 19, 21};
const int NUM_LEDS = sizeof(LEDS) / sizeof(LEDS[0]);
const int BTN = 4;

// 這三個函式只適用 GPIO0～31；GPIO32 以上要改用 GPIO_OUT1_* / GPIO_ENABLE1_* 暫存器
static inline void led_on(int pin)  { REG_WRITE(GPIO_OUT_W1TS_REG, 1u << pin); }
static inline void led_off(int pin) { REG_WRITE(GPIO_OUT_W1TC_REG, 1u << pin); }
static inline bool button_raw_pressed() { return !(REG_READ(GPIO_IN_REG) & (1u << BTN)); }

static void all_leds(bool on) {
  for (int i = 0; i < NUM_LEDS; i++) on ? led_on(LEDS[i]) : led_off(LEDS[i]);
}

// 印出一支腳在 IO_MUX 與 GPIO matrix 的設定，對照 TRM 的欄位說明
__attribute__((unused)) static void print_pin_regs(const char *label, int pin) {
  uint32_t mux = REG_READ(GPIO_PIN_MUX_REG[pin]);
  uint32_t out_sel = REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin);
  Serial.printf("[%s] GPIO%-2d IO_MUX=0x%04lx (MCU_SEL=%lu IE=%lu WPU=%lu WPD=%lu DRV=%lu)  OUT_SEL=%lu  ENABLE=%lu\n",
                label, pin, (unsigned long)mux,
                (unsigned long)((mux >> MCU_SEL_S) & MCU_SEL_V),
                (unsigned long)((mux & FUN_IE) ? 1 : 0),
                (unsigned long)((mux & FUN_PU) ? 1 : 0),
                (unsigned long)((mux & FUN_PD) ? 1 : 0),
                (unsigned long)((mux >> FUN_DRV_S) & FUN_DRV_V),
                (unsigned long)(out_sel & 0x1FF),
                (unsigned long)((REG_READ(GPIO_ENABLE_REG) >> pin) & 1));
}

#if EXERCISE == 3
// ---- 練習 3：用暫存器取代 pinMode ----
static void reg_output_init(int pin) {
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);              // 1. IO_MUX：這支腳交給 GPIO
  REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin, SIG_GPIO_OUT_IDX);  // 2. matrix：輸出由 GPIO_OUT 決定
  REG_CLR_BIT(GPIO_PIN0_REG + 4 * pin, GPIO_PIN0_PAD_DRIVER);         // 3. push-pull（設 1 = open-drain）
  REG_WRITE(GPIO_ENABLE_W1TS_REG, 1u << pin);                         // 4. 輸出致能
}

static void reg_input_pullup_init(int pin) {
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[pin]);                            // FUN_IE：沒開就讀不到電位
  REG_SET_BIT(GPIO_PIN_MUX_REG[pin], FUN_PU);                         // 內部上拉
  REG_CLR_BIT(GPIO_PIN_MUX_REG[pin], FUN_PD);
  REG_WRITE(GPIO_ENABLE_W1TC_REG, 1u << pin);                         // 關掉輸出
}
#endif

static debounce_t db;

void setup() {
  Serial.begin(115200);
  Serial.printf("Day 02 start, EXERCISE = %d\n", EXERCISE);

#if EXERCISE == 3
  print_pin_regs("reset ", LEDS[0]);
  print_pin_regs("reset ", BTN);
  for (int i = 0; i < NUM_LEDS; i++) reg_output_init(LEDS[i]);
  reg_input_pullup_init(BTN);
  print_pin_regs("by reg", LEDS[0]);
  print_pin_regs("by reg", BTN);
  // 對照組：用 pinMode 設定另一支沒用到的腳，看兩者寫進去的值是否一樣
  pinMode(22, OUTPUT);
  pinMode(23, INPUT_PULLUP);
  print_pin_regs("pinMode", 22);
  print_pin_regs("pinMode", 23);
#else
  for (int i = 0; i < NUM_LEDS; i++) pinMode(LEDS[i], OUTPUT);
  pinMode(BTN, INPUT_PULLUP);
#endif

  debounce_init(&db, 20);
  all_leds(false);
}

#if EXERCISE == 0
void loop() {
  static bool last = false;
  static uint32_t count = 0;
  bool now = button_raw_pressed();
  if (now && !last) {                    // 偵測到「沒按 → 按下」的邊緣
    count++;
    Serial.printf("按下次數（沒有去彈跳）：%lu\n", (unsigned long)count);
  }
  last = now;
}

#elif EXERCISE == 1
void loop() {
  static uint32_t count = 0;
  if (debounce_update(&db, button_raw_pressed(), millis()) == DB_EVT_PRESS) {
    count++;
    Serial.printf("按下次數（有去彈跳）：%lu\n", (unsigned long)count);
  }
}

#elif EXERCISE == 2 || EXERCISE == 3
enum Mode { MODE_OFF, MODE_CHASE, MODE_BLINK_ALL, MODE_COUNT };

void loop() {
  static Mode mode = MODE_OFF;
  static uint32_t last_tick = 0;
  static int step = 0;

  if (debounce_update(&db, button_raw_pressed(), millis()) == DB_EVT_PRESS) {
    mode = (Mode)((mode + 1) % MODE_COUNT);
    step = 0;
    last_tick = millis();
    all_leds(false);
    Serial.printf("模式 = %d\n", mode);
  }

  // 不用 delay()：用時間差決定要不要更新，按鈕才能隨時被讀到
  if (millis() - last_tick < 250) return;
  last_tick = millis();

  switch (mode) {
    case MODE_OFF:
      break;
    case MODE_CHASE:                     // 跑馬燈：一次亮一顆
      all_leds(false);
      led_on(LEDS[step % NUM_LEDS]);
      step++;
      break;
    case MODE_BLINK_ALL:                 // 三顆一起 2 Hz 閃爍
      all_leds(step % 2 == 0);
      step++;
      break;
    default:
      break;
  }
}
#endif
