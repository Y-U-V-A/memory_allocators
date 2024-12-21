#ifndef LOGGER__H
#define LOGGER__H

#include "defines.h"

#define LOGE(msg_fmt, ...) log_stdout("\033[31m" msg_fmt "\033[0m\n", ##__VA_ARGS__)
#define LOGW(msg_fmt, ...) log_stdout("\033[33m" msg_fmt "\033[0m\n", ##__VA_ARGS__)
#define LOGI(msg_fmt, ...) log_stdout("\033[37m" msg_fmt "\033[0m\n", ##__VA_ARGS__)
#define LOGD(msg_fmt, ...) log_stdout("\033[34m" msg_fmt "\033[0m\n", ##__VA_ARGS__)
#define LOGT(msg_fmt, ...) log_stdout("\033[32m" msg_fmt "\033[0m\n", ##__VA_ARGS__)

void log_stdout(const char* msg_fmt, ...);

u32 log_buffer(void* buffer, u64 buffer_size, const char* msg_fmt, ...);

#endif