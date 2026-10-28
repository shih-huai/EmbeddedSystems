/*
 * MPU6050 驅動的主機端單元測試：用「假匯流排」取代真的 I2C
 *
 * 假匯流排內部有一份 128 個暫存器的陣列，模擬晶片的行為（寫入、連續讀取、位址自動遞增），
 * 也可以設定成「位址沒有 ACK」來測錯誤處理。
 */
#include <math.h>
#include <string.h>
#include <unity.h>
#include "mpu6050.h"

typedef struct {
  uint8_t addr;          // 晶片的 I2C 位址
  uint8_t regs[128];
  int writes;            // 被寫了幾次
  int fail;              // 1 = 所有傳輸都失敗
} fake_chip_t;

static int fake_write(void *ctx, uint8_t addr, const uint8_t *data, size_t len) {
  fake_chip_t *c = ctx;
  if (c->fail || addr != c->addr || len < 1) return -1;   // 位址不對 = NACK
  for (size_t i = 1; i < len; i++) c->regs[(data[0] + i - 1) & 0x7F] = data[i];
  c->writes++;
  return 0;
}

static int fake_write_read(void *ctx, uint8_t addr, const uint8_t *tx, size_t txlen,
                           uint8_t *rx, size_t rxlen) {
  fake_chip_t *c = ctx;
  if (c->fail || addr != c->addr || txlen != 1) return -1;
  for (size_t i = 0; i < rxlen; i++) rx[i] = c->regs[(tx[0] + i) & 0x7F];
  return 0;
}

static fake_chip_t chip;
static i2c_bus_t bus = {fake_write, fake_write_read, &chip};
static mpu6050_t dev;

void setUp(void) {
  memset(&chip, 0, sizeof(chip));
  chip.addr = 0x68;
  chip.regs[MPU6050_REG_WHO_AM_I] = 0x68;
  chip.regs[MPU6050_REG_PWR_MGMT_1] = 0x40;   // 上電預設：SLEEP = 1
}
void tearDown(void) {}

void test_init_wakes_up_chip(void) {
  TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&dev, &bus, 0x68));
  TEST_ASSERT_EQUAL_HEX8(0x00, chip.regs[MPU6050_REG_PWR_MGMT_1]);
  TEST_ASSERT_EQUAL_HEX8(0x00, chip.regs[MPU6050_REG_ACCEL_CONFIG]);
}

void test_init_wrong_address_is_bus_error(void) {
  TEST_ASSERT_EQUAL_INT(MPU6050_ERR_BUS, mpu6050_init(&dev, &bus, 0x69));
  TEST_ASSERT_EQUAL_INT(0, chip.writes);
}

void test_init_wrong_chip_id(void) {
  chip.regs[MPU6050_REG_WHO_AM_I] = 0x70;     // 例如 MPU6500
  TEST_ASSERT_EQUAL_INT(MPU6050_ERR_ID, mpu6050_init(&dev, &bus, 0x68));
  TEST_ASSERT_EQUAL_HEX8(0x40, chip.regs[MPU6050_REG_PWR_MGMT_1]);   // 認錯晶片就不要亂寫
}

void test_read_raw_big_endian_and_negative(void) {
  TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&dev, &bus, 0x68));
  const uint8_t sample[14] = {
    0x40, 0x00,   // ax = 16384 -> 1 g
    0xC0, 0x00,   // ay = -16384 -> -1 g
    0x00, 0x01,   // az = 1
    0xFF, 0x2C,   // temp = -212
    0x00, 0x83,   // gx = 131 -> 1 dps
    0xFF, 0x7D,   // gy = -131
    0x80, 0x00,   // gz = -32768（最小值）
  };
  memcpy(&chip.regs[MPU6050_REG_ACCEL_XOUT_H], sample, sizeof(sample));
  mpu6050_raw_t raw;
  TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_read_raw(&dev, &raw));
  TEST_ASSERT_EQUAL_INT16(16384, raw.accel[0]);
  TEST_ASSERT_EQUAL_INT16(-16384, raw.accel[1]);
  TEST_ASSERT_EQUAL_INT16(1, raw.accel[2]);
  TEST_ASSERT_EQUAL_INT16(-212, raw.temp);
  TEST_ASSERT_EQUAL_INT16(131, raw.gyro[0]);
  TEST_ASSERT_EQUAL_INT16(-131, raw.gyro[1]);
  TEST_ASSERT_EQUAL_INT16(-32768, raw.gyro[2]);

  mpu6050_data_t d;
  mpu6050_convert(&raw, &d);
  TEST_ASSERT_TRUE(fabsf(d.accel_g[0] - 1.0f) < 1e-4f);
  TEST_ASSERT_TRUE(fabsf(d.accel_g[1] + 1.0f) < 1e-4f);
  TEST_ASSERT_TRUE(fabsf(d.gyro_dps[0] - 1.0f) < 1e-4f);
  TEST_ASSERT_TRUE(fabsf(d.temp_c - 35.906f) < 1e-2f);   // -212/340 + 36.53
}

void test_read_raw_propagates_bus_error(void) {
  TEST_ASSERT_EQUAL_INT(MPU6050_OK, mpu6050_init(&dev, &bus, 0x68));
  chip.fail = 1;
  mpu6050_raw_t raw;
  TEST_ASSERT_EQUAL_INT(MPU6050_ERR_BUS, mpu6050_read_raw(&dev, &raw));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_init_wakes_up_chip);
  RUN_TEST(test_init_wrong_address_is_bus_error);
  RUN_TEST(test_init_wrong_chip_id);
  RUN_TEST(test_read_raw_big_endian_and_negative);
  RUN_TEST(test_read_raw_propagates_bus_error);
  return UNITY_END();
}
