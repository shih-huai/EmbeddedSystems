/*
 * Day 06：I2C 與 MPU6050 驅動
 *
 * MPU6050：SDA 接 GPIO21、SCL 接 GPIO22、VCC 接 3.3V、GND 接 GND（I2C 位址 0x68）。
 * Wokwi 的模組已內建上拉電阻；實體模組（GY-521）通常也有。
 *
 * 改 EXERCISE 的值就能切換練習：
 *   0 = 課堂範例：掃描 I2C 匯流排，列出有回 ACK 的位址
 *   1 = 練習 1 參考解：不用驅動，直接用 Wire 讀 WHO_AM_I（寫暫存器位址 + repeated START + 讀）
 *   2 = 練習 2 參考解：用 lib/mpu6050 驅動，每 500 ms 印出加速度、陀螺儀、溫度
 *   3 = 練習 3 參考解：錯誤處理。故意用錯的位址 0x69 初始化，再示範匯流排卡死時的復原程序
 * 在 Wokwi 點一下 MPU6050 可以拖拉滑桿改變加速度與角速度。
 */
#include <Arduino.h>
#include <Wire.h>
#include "mpu6050.h"

#define EXERCISE 0

const int SDA_PIN = 21, SCL_PIN = 22;

// ---- 把 Arduino Wire 包成驅動要的 i2c_bus_t ----
static int wire_write(void *ctx, uint8_t addr, const uint8_t *data, size_t len) {
  TwoWire *w = static_cast<TwoWire *>(ctx);
  w->beginTransmission(addr);
  w->write(data, len);
  return w->endTransmission(true) == 0 ? 0 : -1;    // 0 = 成功；2 = 位址 NACK；3 = 資料 NACK
}

static int wire_write_read(void *ctx, uint8_t addr, const uint8_t *tx, size_t txlen,
                           uint8_t *rx, size_t rxlen) {
  TwoWire *w = static_cast<TwoWire *>(ctx);
  w->beginTransmission(addr);
  w->write(tx, txlen);
  if (w->endTransmission(false) != 0) return -1;     // false = 不送 STOP，接著 repeated START
  if (w->requestFrom(addr, (uint8_t)rxlen) != rxlen) return -1;
  for (size_t i = 0; i < rxlen; i++) rx[i] = (uint8_t)w->read();
  return 0;
}

static const i2c_bus_t wire_bus = {wire_write, wire_write_read, &Wire};

// 從機在傳輸中途被重置時，可能一直把 SDA 拉低，匯流排就卡死了。
// 標準解法：主機手動打最多 9 個 SCL 脈衝，讓從機把剩下的位元送完、放開 SDA，再送一個 STOP。
__attribute__((unused)) static bool i2c_bus_recover(int sda, int scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, OUTPUT_OPEN_DRAIN);
  digitalWrite(scl, HIGH);
  for (int i = 0; i < 9 && digitalRead(sda) == LOW; i++) {
    digitalWrite(scl, LOW);  delayMicroseconds(5);
    digitalWrite(scl, HIGH); delayMicroseconds(5);
  }
  // STOP：SCL 為高時，SDA 從低變高
  pinMode(sda, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda, LOW);  delayMicroseconds(5);
  digitalWrite(sda, HIGH); delayMicroseconds(5);
  pinMode(sda, INPUT_PULLUP);
  return digitalRead(sda) == HIGH;
}

#if EXERCISE == 0
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 100000);      // 100 kHz 標準模式
  Serial.println("Day 06 EXERCISE 0：掃描 I2C 匯流排");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {       // 只送位址，有 ACK 就代表有裝置
      Serial.printf("  0x%02X 有回應\n", addr);
      found++;
    }
  }
  Serial.printf("共找到 %d 個裝置\n", found);
}
void loop() { delay(1000); }

#elif EXERCISE == 1
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 400000);      // 400 kHz 快速模式
  Wire.beginTransmission(MPU6050_ADDR_DEFAULT);
  Wire.write(MPU6050_REG_WHO_AM_I);          // 先「寫」要讀的暫存器位址
  uint8_t err = Wire.endTransmission(false); // 不送 STOP
  uint8_t n = Wire.requestFrom((uint8_t)MPU6050_ADDR_DEFAULT, (uint8_t)1);  // repeated START + 讀
  Serial.printf("Day 06 EXERCISE 1：endTransmission=%u，讀到 %u 個位元組，WHO_AM_I=0x%02X（應為 0x68）\n",
                err, n, n ? Wire.read() : 0);
}
void loop() { delay(1000); }

#elif EXERCISE == 2
static mpu6050_t imu;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 400000);
  int err = mpu6050_init(&imu, &wire_bus, MPU6050_ADDR_DEFAULT);
  Serial.printf("Day 06 EXERCISE 2：mpu6050_init = %d\n", err);
}

void loop() {
  mpu6050_raw_t raw;
  mpu6050_data_t d;
  if (mpu6050_read_raw(&imu, &raw) == MPU6050_OK) {
    mpu6050_convert(&raw, &d);
    Serial.printf("加速度 g: %6.2f %6.2f %6.2f | 角速度 dps: %7.1f %7.1f %7.1f | %.1f°C\n",
                  d.accel_g[0], d.accel_g[1], d.accel_g[2],
                  d.gyro_dps[0], d.gyro_dps[1], d.gyro_dps[2], d.temp_c);
  } else {
    Serial.println("讀取失敗");
  }
  delay(500);
}

#elif EXERCISE == 3
void setup() {
  Serial.begin(115200);
  Serial.printf("Day 06 EXERCISE 3：匯流排復原 -> %s\n",
                i2c_bus_recover(SDA_PIN, SCL_PIN) ? "SDA 已放開" : "SDA 仍被拉低（檢查接線或電源）");
  Wire.begin(SDA_PIN, SCL_PIN, 400000);
  mpu6050_t imu;
  Serial.printf("用錯的位址 0x69 初始化：%d（-1 = 匯流排錯誤／NACK）\n", mpu6050_init(&imu, &wire_bus, 0x69));
  Serial.printf("用正確位址 0x68 初始化：%d（0 = 成功）\n", mpu6050_init(&imu, &wire_bus, 0x68));
}
void loop() { delay(1000); }
#endif
