# Embedded Systems 教材

ESP32 韌體與驅動面試速成課程的教材存放處。所有資料夾與檔名都用英文，避免 PlatformIO 在 Windows 上遇到中文路徑編譯失敗。

- `lessons/`：每堂課的講義，Markdown（.md）與 HTML（.html，可直接用瀏覽器開啟）兩種格式
- `DayXX_.../`：每堂課的 VS Code + Wokwi 程式碼專案（PlatformIO），用 VS Code「開啟資料夾」開到這一層，步驟見各資料夾的 README

| 課 | 講義 | 程式碼 | 內容 | 更新日期 |
| --- | --- | --- | --- | --- |
| 1 | `lessons/Lesson01_Plan_Registers_Volatile` | `Day01_Registers_Volatile/` | 學習計畫、12 堂課大綱、記憶體映射暫存器與 volatile、測試與模擬方法 | 2026-10-23 |
| 2 | `lessons/Lesson02_GPIO_PinMux` | `Day02_GPIO_PinMux/` | IO_MUX 與 GPIO matrix、push-pull／open-drain、上下拉、strapping pin、去彈跳狀態機與單元測試 | 2026-10-24 |
| 3 | `lessons/Lesson03_Interrupts_ISR` | `Day03_Interrupts_ISR/` | 中斷流程與延遲、ISR 規則、IRAM_ATTR、競態條件與 critical section、邊緣／水平觸發 | 2026-10-24 |
| 4 | `lessons/Lesson04_Timers_PWM_Watchdog` | `Day04_Timers_PWM_Watchdog/` | 硬體計時器、LEDC PWM 呼吸燈、Task Watchdog | 2026-10-24 |
| 5 | `lessons/Lesson05_UART_RingBuffer` | `Day05_UART_RingBuffer/` | UART 框格格式與 baud 誤差、lock-free ring buffer、簡單指令列介面 | 2026-10-24 |
| 6 | `lessons/Lesson06_I2C_MPU6050` | `Day06_I2C_MPU6050/` | I2C 協定（START/STOP/ACK、上拉電阻、clock stretching、匯流排卡死）、自寫 MPU6050 驅動 | 2026-10-24 |
| 7 | `lessons/Lesson07_SPI_ILI9341` | `Day07_SPI_ILI9341/` | SPI 四種模式（CPOL/CPHA）、與 I2C 的取捨、自寫 ILI9341 螢幕驅動 | 2026-10-24 |
| 8 | `lessons/Lesson08_DMA_Cache_Alignment` | `Day08_DMA_Cache_Alignment/` | DMA、cache coherency、DMA 緩衝區對齊、double buffering | 2026-10-24 |
| 9 | `lessons/Lesson09_FreeRTOS_Tasks` | `Day09_FreeRTOS_Tasks/` | FreeRTOS task、搶佔式排程、堆疊大小與溢位偵測、雙核心 affinity | 2026-10-24 |
| 10 | `lessons/Lesson10_FreeRTOS_Sync` | `Day10_FreeRTOS_Sync/` | queue、semaphore、mutex、priority inversion 與 priority inheritance、FromISR API | 2026-10-24 |
| 11 | `lessons/Lesson11_Boot_Linker_OTA` | `Day11_Boot_Linker_OTA/` | 開機流程、linker script 與記憶體區段（.data/.bss）、bootloader、partition table、OTA 與回滾 | 2026-10-24 |
| 12 | `lessons/Lesson12_LowPower_HAL_Capstone` | `Day12_LowPower_HAL_Capstone/` | 睡眠模式與喚醒、驅動分層（HAL）、可測試驅動設計、總複習與模擬面試 | 2026-10-24 |

線上原始版本：https://claude.ai/artifact/WeuhwMe6HzwwifNdv3QZby
