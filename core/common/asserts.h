#ifndef ASSERTS__H
#define ASSERTS__H

#include "defines.h"
#include "logger.h"

#define DEBUG_BREAK __builtin_trap()

#define ASSERT_GULE(a, b) a##b
#define _ASSERT_GULE(a, b) ASSERT_GULE(a, b)

#define ZSTATIC_ASSERT(exp)                                  \
    enum {                                                   \
        _ASSERT_GULE(assert_, __LINE__) = 1 / (i32)(!!(exp)) \
    }

ZSTATIC_ASSERT(sizeof(i8) == 1);
ZSTATIC_ASSERT(sizeof(i16) == 2);
ZSTATIC_ASSERT(sizeof(i32) == 4);
ZSTATIC_ASSERT(sizeof(i64) == 8);

ZSTATIC_ASSERT(sizeof(u8) == 1);
ZSTATIC_ASSERT(sizeof(u16) == 2);
ZSTATIC_ASSERT(sizeof(u32) == 4);
ZSTATIC_ASSERT(sizeof(u64) == 8);

ZSTATIC_ASSERT(sizeof(f32) == 4);
ZSTATIC_ASSERT(sizeof(f64) == 8);

#define ZASSERT(exp)                    \
    if (exp) {                          \
                                        \
    } else {                            \
        LOGE(#exp, __FILE__, __LINE__); \
        DEBUG_BREAK;                    \
    }

#endif