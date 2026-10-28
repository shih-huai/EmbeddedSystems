#include "mpu6050.h"

static int write_reg(mpu6050_t *dev, uint8_t reg, uint8_t value) {
  uint8_t buf[2] = {reg, value};
  return dev->bus->write(dev->bus->ctx, dev->addr, buf, sizeof(buf)) == 0 ? MPU6050_OK : MPU6050_ERR_BUS;
}

static int read_regs(mpu6050_t *dev, uint8_t reg, uint8_t *out, size_t len) {
  // 先寫入要讀的暫存器位址，repeated START 後再讀；晶片會自動把位址往下加
  return dev->bus->write_read(dev->bus->ctx, dev->addr, &reg, 1, out, len) == 0 ? MPU6050_OK : MPU6050_ERR_BUS;
}

int mpu6050_init(mpu6050_t *dev, const i2c_bus_t *bus, uint8_t addr) {
  if (!dev || !bus || !bus->write || !bus->write_read) return MPU6050_ERR_ARG;
  dev->bus = bus;
  dev->addr = addr;

  uint8_t id = 0;
  int err = read_regs(dev, MPU6050_REG_WHO_AM_I, &id, 1);
  if (err) return err;
  if (id != MPU6050_WHO_AM_I_VALUE) return MPU6050_ERR_ID;

  err = write_reg(dev, MPU6050_REG_PWR_MGMT_1, 0x00);       // 清掉 SLEEP 位元，時脈用內部 8 MHz
  if (err) return err;
  return write_reg(dev, MPU6050_REG_ACCEL_CONFIG, 0x00);    // AFS_SEL = 0：±2g
}

static int16_t be16(const uint8_t *p) {
  // MPU6050 是大端序：高位元組在前。先組成 uint16_t 再轉 int16_t，負數才正確
  return (int16_t)(uint16_t)((p[0] << 8) | p[1]);
}

int mpu6050_read_raw(mpu6050_t *dev, mpu6050_raw_t *out) {
  if (!dev || !out) return MPU6050_ERR_ARG;
  uint8_t b[14];
  int err = read_regs(dev, MPU6050_REG_ACCEL_XOUT_H, b, sizeof(b));
  if (err) return err;
  for (int i = 0; i < 3; i++) out->accel[i] = be16(&b[i * 2]);
  out->temp = be16(&b[6]);
  for (int i = 0; i < 3; i++) out->gyro[i] = be16(&b[8 + i * 2]);
  return MPU6050_OK;
}

void mpu6050_convert(const mpu6050_raw_t *raw, mpu6050_data_t *out) {
  for (int i = 0; i < 3; i++) {
    out->accel_g[i] = raw->accel[i] / 16384.0f;   // ±2g：每 g 16384 LSB
    out->gyro_dps[i] = raw->gyro[i] / 131.0f;     // ±250 dps：每 dps 131 LSB
  }
  out->temp_c = raw->temp / 340.0f + 36.53f;      // 資料手冊的公式
}
