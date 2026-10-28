/*
 * Day 01：記憶體映射暫存器與 volatile
 *
 * 不用 digitalWrite，直接寫 ESP32 GPIO 暫存器：按住按鈕 LED 亮，放開 LED 滅。
 * LED：GPIO2（串 220Ω 到 GND）；按鈕：GPIO4 到 GND（內部上拉）。
 * 位址來自 ESP32 TRM 的 GPIO 章節，適用原版 ESP32（S3、C3 位址不同）。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：按住亮、放開滅
 *   1 = 練習 1 參考解：按住時以 2 Hz 閃爍，只用 W1TS／W1TC，不用 ^=
 *   2 = 練習 2 實驗：拿掉 volatile 的忙等迴圈（觀察編譯器優化造成的死迴圈）
 *   3 = 練習 3 參考解：改用 ESP-IDF 的 soc/gpio_reg.h 與 REG_WRITE
 * 建議先自己寫練習 1、3，再對照參考解。
 */
#include <Arduino.h>

#define EXERCISE 0

#define GPIO_BASE            0x3FF44000u
#define REG32(off)           (*(volatile uint32_t *)(GPIO_BASE + (off)))
#define GPIO_OUT_W1TS        REG32(0x08)
#define GPIO_OUT_W1TC        REG32(0x0C)
#define GPIO_ENABLE_W1TS     REG32(0x24)
#define GPIO_IN              REG32(0x3C)

// 練習 2 用：同一個位址，但「忘了」volatile
#define GPIO_IN_NO_VOLATILE  (*(uint32_t *)(GPIO_BASE + 0x3C))

const int LED = 2, BTN = 4;

static inline bool button_pressed() {
  return !(GPIO_IN & (1u << BTN));   // 上拉，按下為 0
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);              // 先讓 IO_MUX 把腳位交給 GPIO（第 2 課會拆解）
  pinMode(BTN, INPUT_PULLUP);
  GPIO_ENABLE_W1TS = (1u << LED);
  Serial.printf("Day 01 start, EXERCISE = %d\n", EXERCISE);
}

#if EXERCISE == 0
void loop() {
  if (button_pressed()) GPIO_OUT_W1TS = (1u << LED);
  else                  GPIO_OUT_W1TC = (1u << LED);
}

#elif EXERCISE == 1
void loop() {
  static uint32_t last = 0;
  static bool on = false;
  if (!button_pressed()) {
    GPIO_OUT_W1TC = (1u << LED);
    on = false;
    return;
  }
  // 2 Hz 閃爍 = 每 250 ms 切換一次
  if (millis() - last >= 250) {
    last = millis();
    on = !on;
    if (on) GPIO_OUT_W1TS = (1u << LED);
    else    GPIO_OUT_W1TC = (1u << LED);
  }
}

#elif EXERCISE == 2
void loop() {
  Serial.println("按住按鈕，再放開...");
  while (!button_pressed()) { }      // 等待按下（有 volatile，正常）
  Serial.println("偵測到按下，等待放開（這裡沒有 volatile）");
  // 沒有 volatile，編譯器可以假設這個值不會變，只讀一次就好。可能出現兩種「錯」：
  //   a) 讀取被提到迴圈外 -> 放開按鈕也出不來（死迴圈）
  //   b) C++ 允許編譯器假設沒有副作用的迴圈終究會結束 -> 整個迴圈被刪掉，
  //      還沒放開就直接印出下一行
  // 哪一種取決於編譯器版本與優化等級。想看證據：用 xtensa-esp32-elf-objdump -d
  // 反組譯 .pio/build/esp32dev/firmware.elf，找 loop() 這段的組語。
  while (!(GPIO_IN_NO_VOLATILE & (1u << BTN))) { }
  Serial.println("離開等待迴圈（注意：此時你真的放開按鈕了嗎？）");
  delay(200);
}

#elif EXERCISE == 3
#include "soc/gpio_reg.h"
void loop() {
  if (!(REG_READ(GPIO_IN_REG) & (1u << BTN))) REG_WRITE(GPIO_OUT_W1TS_REG, 1u << LED);
  else                                        REG_WRITE(GPIO_OUT_W1TC_REG, 1u << LED);
}
static_assert(GPIO_OUT_W1TS_REG == GPIO_BASE + 0x08, "W1TS 位址應一致");
static_assert(GPIO_OUT_W1TC_REG == GPIO_BASE + 0x0C, "W1TC 位址應一致");
static_assert(GPIO_IN_REG       == GPIO_BASE + 0x3C, "GPIO_IN 位址應一致");
#endif
