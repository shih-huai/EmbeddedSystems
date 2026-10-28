/*
 * MPU6050 六軸感測器驅動（加速度計 + 陀螺儀），只依賴 i2c_bus_t
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPU6050_ADDR_DEFAULT  0x68   // AD0 腳接 GND；接 VCC 時是 0x69

/* 會用到的暫存器（MPU-6000/6050 Register Map） */
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_ACCEL_XOUT_H  0x3B   // 從這裡連續 14 個位元組：加速度 XYZ、溫度、陀螺儀 XYZ
#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_WHO_AM_I      0x75
#define MPU6050_WHO_AM_I_VALUE    0x68

enum {
  MPU6050_OK = 0,
  MPU6050_ERR_BUS = -1,      // I2C 傳輸失敗（沒有 ACK、匯流排卡住）
  MPU6050_ERR_ID = -2,       // 有回應，但不是 MPU6050
  MPU6050_ERR_ARG = -3,
};

typedef struct {
  const i2c_bus_t *bus;
  uint8_t addr;
} mpu6050_t;

typedef struct {
  int16_t accel[3];   // X、Y、Z 原始值
  int16_t temp;
  int16_t gyro[3];
} mpu6050_raw_t;

typedef struct {
  float accel_g[3];   // 單位 g（±2g 檔位）
  float temp_c;
  float gyro_dps[3];  // 單位 度/秒（±250 dps 檔位）
} mpu6050_data_t;

/* 確認 WHO_AM_I、喚醒晶片（上電預設在睡眠模式）、設定 ±2g */
int mpu6050_init(mpu6050_t *dev, const i2c_bus_t *bus, uint8_t addr);

/* 一次 burst read 14 個位元組，保證 7 個值來自同一次取樣 */
int mpu6050_read_raw(mpu6050_t *dev, mpu6050_raw_t *out);

/* 原始值換成物理單位（純計算，不碰匯流排） */
void mpu6050_convert(const mpu6050_raw_t *raw, mpu6050_data_t *out);

#ifdef __cplusplus
}
#endif

#endif
