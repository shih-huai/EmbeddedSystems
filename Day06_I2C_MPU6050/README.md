# Day 06：I2C 與 MPU6050 驅動（VS Code + Wokwi）

不用現成函式庫，自己寫 MPU6050 六軸感測器的驅動。驅動只依賴一組 I2C 函式指標（`i2c_bus_t`），所以可以在電腦上用「假匯流排」做單元測試。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day06_I2C_MPU6050` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。
4. 模擬時點一下 MPU6050，可以拖拉滑桿改變加速度和角速度。

## 電路
| MPU6050 | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

實體模組（常見的 GY-521）板上已有上拉電阻，AD0 腳空接或接 GND 時位址是 0x68。

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 掃描 I2C 匯流排，列出有 ACK 的位址（應該看到 0x68） |
| 1 | 直接用 `Wire` 讀 WHO_AM_I：寫暫存器位址、repeated START、讀 1 個位元組（參考解） |
| 2 | 用 `lib/mpu6050` 驅動，每 500 ms 印出加速度、角速度、溫度（參考解） |
| 3 | 錯誤處理：錯的位址會得到 NACK；示範匯流排卡死時打 9 個時脈的復原程序（參考解） |

## 程式庫
| 檔案 | 內容 |
| --- | --- |
| `lib/mpu6050/i2c_bus.h` | I2C 抽象介面：`write`、`write_read` 兩個函式指標 |
| `lib/mpu6050/mpu6050.c` | 驅動：檢查 WHO_AM_I、喚醒、burst read 14 bytes、大端序轉換、單位換算 |
| `test/test_mpu6050` | 5 個單元測試，用假匯流排模擬晶片的暫存器，也測 NACK 和認錯晶片 |

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或直接用 gcc：
  ```
  gcc -I<unity>/src -Ilib/mpu6050 <unity>/src/unity.c lib/mpu6050/mpu6050.c test/test_mpu6050/test_main.c -o test_mpu6050 -lm
  ```
