/*
 * 簡單的指令列：把收到的字元組成一行，再把一行拆成 argv
 * 純 C、不碰硬體，可在電腦上做單元測試。
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CLI_LINE_MAX 64

typedef struct {
  char buf[CLI_LINE_MAX];
  size_t len;
  bool ready;       // 上一次 feed 已經交出一整行
  bool overflow;    // 這一行太長，會被整行丟掉
} line_reader_t;

void lr_init(line_reader_t *lr);

/* 餵一個字元。收到 '\n' 且這一行沒有超長時回傳 true，lr->buf 是以 NUL 結尾的一整行
 * （不含 \r\n），在下一次呼叫 lr_feed 之前都有效。支援 Backspace（0x08、0x7F）。 */
bool lr_feed(line_reader_t *lr, char c);

/* 把 line 原地以空白或 Tab 切開，最多 max 個參數；回傳參數個數。 */
int cli_split(char *line, char *argv[], int max);

#ifdef __cplusplus
}
#endif

#endif
