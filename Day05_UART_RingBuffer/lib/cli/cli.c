#include "cli.h"

void lr_init(line_reader_t *lr) {
  lr->len = 0;
  lr->ready = false;
  lr->overflow = false;
}

bool lr_feed(line_reader_t *lr, char c) {
  if (lr->ready) {                 // 上一行已經交出去了，開始新的一行
    lr->len = 0;
    lr->ready = false;
  }
  if (c == '\r') return false;     // Windows 的 \r\n：忽略 \r
  if (c == '\n') {
    if (lr->overflow) {            // 超長的一行整行丟掉，不要交出被截斷的指令
      lr->overflow = false;
      lr->len = 0;
      return false;
    }
    lr->buf[lr->len] = '\0';
    lr->ready = true;
    return true;
  }
  if (c == 0x08 || c == 0x7F) {    // Backspace
    if (lr->len > 0) lr->len--;
    return false;
  }
  if (lr->len >= CLI_LINE_MAX - 1) {   // 留一格給 '\0'
    lr->overflow = true;
    return false;
  }
  lr->buf[lr->len++] = c;
  return false;
}

int cli_split(char *line, char *argv[], int max) {
  int argc = 0;
  char *p = line;
  while (*p && argc < max) {
    while (*p == ' ' || *p == '\t') p++;   // 跳過連續空白
    if (*p == '\0') break;
    argv[argc++] = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p) *p++ = '\0';                   // 原地切斷
  }
  return argc;
}
