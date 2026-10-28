# Day 01：記憶體映射暫存器與 volatile（VS Code + Wokwi）

不用 `digitalWrite`，直接寫 ESP32 的 GPIO 暫存器：按住按鈕 LED 亮，放開就滅。

## 需要的擴充套件
- **PlatformIO IDE**（負責編譯）
- **Wokwi Simulator**（負責模擬，你已安裝）


## 執行步驟
1. VS Code 選「檔案 → 開啟資料夾」，開啟 `Day01_Registers_Volatile` 這個資料夾本身（不是上一層）。
2. 第一次開啟時 PlatformIO 會自動下載 ESP32 工具鏈，需要幾分鐘。
3. 編譯：點左下角狀態列的 ✓（PlatformIO: Build），或按 `Ctrl+Alt+B`。看到 `SUCCESS` 即可。
4. 第一次使用 Wokwi 要啟用授權：`F1` → `Wokwi: Request a New License`，依瀏覽器指示完成（免費）。
5. 開啟 `diagram.json` 可以看到電路圖；按 `F1` → `Wokwi: Start Simulator` 開始模擬。
6. 在模擬畫面中按住綠色按鈕，紅色 LED 應該會亮。

修改程式後要**重新編譯（步驟 3）**，Wokwi 才會載入新的韌體。

## 檔案說明
| 檔案 | 用途 |
| --- | --- |
| `src/main.cpp` | 程式碼，用 `EXERCISE` 巨集切換練習 |
| `platformio.ini` | PlatformIO 專案設定（esp32dev + Arduino 框架） |
| `diagram.json` | Wokwi 電路：LED 串 220Ω 接 GPIO2，按鈕接 GPIO4 與 GND |
| `wokwi.toml` | 告訴 Wokwi 韌體檔在 `.pio/build/esp32dev/` |

## 練習（改 `main.cpp` 第一段的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 課堂範例：按住亮、放開滅 |
| 1 | 按住時 LED 以 2 Hz 閃爍，只用 W1TS／W1TC，不用 `^=`（參考解） |
| 2 | 拿掉 `volatile` 的等待迴圈，觀察編譯器把讀取優化掉 |
| 3 | 改用 ESP-IDF 的 `soc/gpio_reg.h` 與 `REG_WRITE`，並用 `static_assert` 確認位址和自己寫的一致（參考解） |

建議先自己寫練習 1 和 3，再看參考解。

### 練習 2 會看到什麼
按住按鈕後放開，Serial Monitor 會印出訊息。沒有 `volatile` 時，編譯器可以假設這個值不會變，所以會出現兩種「錯」：等待迴圈被整個刪掉，還沒放開就直接印出下一行；或者讀取只做一次，放開後也卡在迴圈裡。用一般 GCC 以 `-Os` 編譯同樣的迴圈，有 `volatile` 的版本每圈都會重讀位址，沒有的版本整個迴圈直接變成 `ret`。這就是面試題「volatile 是什麼」最好的實證。

## 常見問題
- **Wokwi 說找不到 firmware**：還沒編譯，或開錯資料夾層級（要開到有 `wokwi.toml` 的這一層）。
- **LED 不亮**：確認 `diagram.json` 的板子是 ESP32 DevKit V1；位址只適用原版 ESP32，S3、C3 的位址不同。
