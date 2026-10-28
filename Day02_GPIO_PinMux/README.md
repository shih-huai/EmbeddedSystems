# Day 02：GPIO 與 pin mux（VS Code + Wokwi）

一支腳要先經過 IO_MUX、再經過 GPIO matrix，才輪到第 1 課寫的 GPIO_OUT。這一課用按鈕切換 3 顆 LED 的模式，順便把按鈕去彈跳寫成可以在電腦上測試的狀態機。

## 需要的擴充套件
- **PlatformIO IDE**（負責編譯）
- **Wokwi Simulator**（負責模擬）

## 執行步驟
1. VS Code 選「檔案 → 開啟資料夾」，開啟 `Day02_GPIO_PinMux` 這個資料夾本身（不是上一層）。
2. 編譯：點左下角狀態列的 ✓（PlatformIO: Build），或按 `Ctrl+Alt+B`。看到 `SUCCESS` 即可。
3. 開啟 `diagram.json`，按 `F1` → `Wokwi: Start Simulator` 開始模擬。
4. 打開 Wokwi 的 Serial Monitor，按藍色按鈕看輸出。

修改程式後要**重新編譯**，Wokwi 才會載入新的韌體。

## 電路
| 元件 | 腳位 |
| --- | --- |
| 紅 LED（串 220Ω） | GPIO18 |
| 黃 LED（串 220Ω） | GPIO19 |
| 綠 LED（串 220Ω） | GPIO21 |
| 按鈕（另一端接 GND） | GPIO4，內部上拉 |

不再用 GPIO2，因為它是 strapping pin（開機時的電位會影響開機模式）。燒到實體板時照這張表接線即可。

## 檔案說明
| 檔案 | 用途 |
| --- | --- |
| `src/main.cpp` | 程式碼，用 `EXERCISE` 巨集切換練習 |
| `lib/debounce/` | 去彈跳狀態機（純 C，不碰硬體） |
| `test/test_debounce/` | 去彈跳的單元測試（Unity） |
| `platformio.ini` | `esp32dev`：燒錄／模擬用；`native`：在電腦上跑測試 |
| `diagram.json` | Wokwi 電路 |
| `wokwi.toml` | 告訴 Wokwi 韌體檔在 `.pio/build/esp32dev/` |

## 練習（改 `main.cpp` 第一段的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | 課堂實驗：沒有去彈跳，按一次按鈕看計數跳了幾次 |
| 1 | 用 `lib/debounce` 狀態機，一次按下只算一次（參考解） |
| 2 | 按鈕切換 3 種模式：全滅 → 跑馬燈 → 三顆一起 2 Hz 閃（參考解） |
| 3 | 不呼叫 `pinMode`，自己寫 IO_MUX 與 GPIO matrix 暫存器，並印出和 `pinMode` 設定的結果對照（參考解） |

建議先自己寫練習 1～3，再看參考解。

### 練習 0 會看到什麼
Wokwi 的按鈕有模擬彈跳（`diagram.json` 裡的 `"bounce": "1"`）。按一次，Serial Monitor 的計數可能一次跳好幾個。改成練習 1 之後，每按一次只加 1。

### 練習 3 會看到什麼
Serial Monitor 會印出每支腳的 `MCU_SEL`（2 = GPIO 功能）、`IE`（輸入致能）、`WPU`／`WPD`（上拉／下拉）、`DRV`（驅動強度）和 `OUT_SEL`（256 = 由 GPIO_OUT 暫存器驅動）。比較「reset」「by reg」「pinMode」三組數字，就知道 `pinMode` 底下做了什麼。

## 在電腦上跑單元測試
去彈跳邏輯不碰硬體，所以可以在電腦上測，不用開模擬器。

- 方法一（PlatformIO）：終端機執行 `pio test -e native`。Windows 需要先裝 gcc（例如 MSYS2 的 MinGW-w64）。
- 方法二（直接用 gcc）：下載 [Unity](https://github.com/ThrowTheSwitch/Unity) 的 `src/` 資料夾，然後：
  ```
  gcc -I<unity>/src -Ilib/debounce <unity>/src/unity.c lib/debounce/debounce.c test/test_debounce/test_main.c -o test_debounce
  ./test_debounce
  ```

應該看到 `5 Tests 0 Failures`。試著故意改壞 `debounce.c`（例如把 `>=` 改成 `>`），看哪個測試抓到。

## 常見問題
- **Wokwi 說找不到 firmware**：還沒編譯，或開錯資料夾層級（要開到有 `wokwi.toml` 的這一層）。
- **`pio test` 找不到 gcc**：方法一需要電腦上有 gcc；沒有的話先用方法二，或只跑 Wokwi。
