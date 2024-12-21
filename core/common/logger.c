#include "logger.h"
#include <stdio.h>
#include <stdarg.h>

void log_stdout(const char* msg_fmt, ...) {
    va_list args;
    va_start(args, msg_fmt);
    vprintf(msg_fmt, args);
    va_end(args);
}

u32 log_buffer(void* buffer, u64 buffer_size, const char* msg_fmt, ...) {
    va_list args;
    va_start(args, msg_fmt);
    u32 written = (u32)vsnprintf(buffer, buffer_size, msg_fmt, args);
    va_end(args);
    return written;
}
