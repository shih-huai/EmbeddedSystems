# Day 05：UART 與 ring buffer（VS Code + Wokwi）

UART1 的 TX（GPIO27）接到 UART2 的 RX（GPIO16），讓晶片自己跟自己通訊，用來觀察鮑率不一致和緩衝區溢位。再用自己寫的 ring buffer 和指令列，在 Serial Monitor 打字控制 LED。

## 執行步驟
1. VS Code「檔案 → 開啟資料夾」，開到 `Day05_UART_RingBuffer` 這一層。
2. 編譯：狀態列的 ✓（PlatformIO: Build）或 `Ctrl+Alt+B`。
3. 開啟 `diagram.json`，`F1` → `Wokwi: Start Simulator`，打開 Serial Monitor。

## 電路
| 元件 | 腳位 |
| --- | --- |
| UART1 TX → UART2 RX（一條線） | GPIO27 → GPIO16 |
| 紅 LED（串 220Ω） | GPIO18 |

燒到實體板時，用杜邦線把 GPIO27 接到 GPIO16。

## 練習（改 `src/main.cpp` 的 `#define EXERCISE`）
| 值 | 內容 |
| --- | --- |
| 0 | UART1 每秒送一行字，UART2 收到後印出來 |
| 1 | 鮑率不一致（送 115200、收 9600），看收到的位元組和 framing error 次數 |
| 2 | 在 Serial Monitor 打 `help`、`led on`、`led off`、`blink 200`、`stats` 控制 LED（參考解） |
| 3 | ring buffer 只有 64 bytes、消費者很慢，看「丟掉」的數字一直增加。試著加大緩衝區或讓消費者變快 |

## 程式庫
| 資料夾 | 內容 |
| --- | --- |
| `lib/ringbuf` | 單一生產者、單一消費者的無鎖 ring buffer，容量是 2 的次方 |
| `lib/cli` | 把字元組成一行（支援 Backspace、超長整行丟棄），再切成 argv |

## 在電腦上跑單元測試
- `pio test -e native`（Windows 需要先裝 gcc）
- 或直接用 gcc，例如：
  ```
  gcc -I<unity>/src -Ilib/ringbuf <unity>/src/unity.c lib/ringbuf/ringbuf.c test/test_ringbuf/test_main.c -o test_ringbuf
  gcc -I<unity>/src -Ilib/cli <unity>/src/unity.c lib/cli/cli.c test/test_cli/test_main.c -o test_cli
  ```
