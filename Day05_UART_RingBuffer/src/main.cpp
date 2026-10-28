/*
 * Day 05：UART 與 ring buffer
 *
 * UART0（Serial）：接 Wokwi 的 Serial Monitor。
 * UART1（Serial1）：TX 改到 GPIO27。UART2（Serial2）：RX 是 GPIO16。
 * 電路上 GPIO27 接到 GPIO16，所以 UART1 送出的東西會被 UART2 收到（自己跟自己通訊）。
 * LED：GPIO18（串 220Ω 到 GND）。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：UART1 每秒送一行字，UART2 收到後印到 Serial Monitor
 *   1 = 練習 1 實驗：兩邊鮑率不一致（115200 vs 9600），看收到什麼、出現幾次 framing error
 *   2 = 練習 2 參考解：在 Serial Monitor 打指令控制 LED（自己的 ring buffer + 指令列）
 *   3 = 練習 3 實驗：ring buffer 太小、消費者太慢，觀察資料被丟掉
 * 建議先自己寫練習 2，再對照參考解。
 */
#include <Arduino.h>
#include "ringbuf.h"
#include "cli.h"

#define EXERCISE 0

const int LED = 18;
const int U1_RX = 26, U1_TX = 27;   // UART1 預設腳位接著 flash，所以一定要改腳
const int U2_RX = 16, U2_TX = 17;

#if EXERCISE == 0 || EXERCISE == 1
static volatile uint32_t frame_errors = 0;

void setup() {
  Serial.begin(115200);
  const uint32_t rx_baud = (EXERCISE == 0) ? 115200 : 9600;
  Serial1.begin(115200, SERIAL_8N1, U1_RX, U1_TX);
  Serial2.begin(rx_baud, SERIAL_8N1, U2_RX, U2_TX);
  Serial2.onReceiveError([](hardwareSerial_error_t err) {
    if (err == UART_FRAME_ERROR) frame_errors++;   // 該是停止位元的地方讀到 0
  });
  Serial.printf("Day 05 EXERCISE %d：UART1 送 115200，UART2 收 %lu\n", EXERCISE, (unsigned long)rx_baud);
}

void loop() {
  static uint32_t n = 0, last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    Serial1.printf("hello #%lu\n", (unsigned long)n++);
  }
  if (Serial2.available()) {
    delay(20);                       // 等這一行收完，示範用
    Serial.print("UART2 收到：");
    while (Serial2.available()) {
      int c = Serial2.read();
      if (EXERCISE == 0) Serial.write(c);
      else Serial.printf("%02X ", c); // 鮑率不對時多半是亂碼，印十六進位比較好看
    }
    if (EXERCISE == 1) Serial.printf("\n  framing error 累計：%lu\n", (unsigned long)frame_errors);
  }
}

#elif EXERCISE == 2
// UART 驅動在背景收資料，onReceive 的回呼把位元組搬進我們的 ring buffer（生產者），
// loop() 從 ring buffer 拿出來組成指令（消費者）。這就是「ISR 只搬資料、主程式做事」的分工。
static uint8_t rx_storage[256];
static ringbuf_t rx;
static line_reader_t lr;
static uint32_t blink_ms = 0;

static void on_uart_rx() {
  while (Serial.available()) rb_put(&rx, (uint8_t)Serial.read());
}

static void run_command(int argc, char *argv[]) {
  if (argc == 0) return;
  if (!strcmp(argv[0], "help")) {
    Serial.println("指令：led on | led off | blink <ms> | stats | help");
  } else if (!strcmp(argv[0], "led") && argc == 2) {
    blink_ms = 0;
    digitalWrite(LED, !strcmp(argv[1], "on") ? HIGH : LOW);
    Serial.printf("LED %s\n", argv[1]);
  } else if (!strcmp(argv[0], "blink") && argc == 2) {
    blink_ms = strtoul(argv[1], nullptr, 10);
    Serial.printf("每 %lu ms 切換一次\n", (unsigned long)blink_ms);
  } else if (!strcmp(argv[0], "stats")) {
    Serial.printf("ring buffer：%lu/%lu，丟掉 %lu 個位元組\n", (unsigned long)rb_count(&rx),
                  (unsigned long)rb_capacity(&rx), (unsigned long)rx.dropped);
  } else {
    Serial.printf("不認得的指令：%s（打 help 看說明）\n", argv[0]);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  rb_init(&rx, rx_storage, sizeof(rx_storage));
  lr_init(&lr);
  Serial.onReceive(on_uart_rx);
  Serial.println("Day 05 EXERCISE 2：在 Serial Monitor 輸入指令，打 help 看說明");
}

void loop() {
  uint8_t c;
  while (rb_get(&rx, &c)) {
    if (lr_feed(&lr, (char)c)) {
      char *argv[4];
      int argc = cli_split(lr.buf, argv, 4);
      run_command(argc, argv);
    }
  }
  static uint32_t last = 0;
  if (blink_ms && millis() - last >= blink_ms) {
    last = millis();
    digitalWrite(LED, !digitalRead(LED));
  }
}

#elif EXERCISE == 3
static uint8_t rx_storage[64];      // 故意很小
static ringbuf_t rx;
static volatile uint32_t received = 0;

static void on_uart2_rx() {
  while (Serial2.available()) {
    rb_put(&rx, (uint8_t)Serial2.read());
    received++;
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, U1_RX, U1_TX);
  Serial2.begin(115200, SERIAL_8N1, U2_RX, U2_TX);
  rb_init(&rx, rx_storage, sizeof(rx_storage));
  Serial2.onReceive(on_uart2_rx);
  Serial.println("Day 05 EXERCISE 3：每 2 秒送 500 bytes，消費者每 10 ms 只拿 8 bytes");
}

void loop() {
  static uint32_t last_burst = 0, consumed = 0;
  if (millis() - last_burst >= 2000) {
    last_burst = millis();
    for (int i = 0; i < 500; i++) Serial1.write((uint8_t)i);
    Serial.printf("送出 500，UART2 收到 %lu，消費者拿走 %lu，ring buffer 丟掉 %lu\n",
                  (unsigned long)received, (unsigned long)consumed, (unsigned long)rx.dropped);
  }
  uint8_t c;
  for (int i = 0; i < 8 && rb_get(&rx, &c); i++) consumed++;   // 模擬很慢的處理
  delay(10);
}
#endif
