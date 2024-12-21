#ifndef EXPECT__H
#define EXPECT__H

#include "defines.h"
#include "logger.h"

#define expect_should_be(expected, actual)                                                \
    if (expected != actual) {                                                             \
        LOGE("expected %lld but got %lld , %s:%d", expected, actual, __FILE__, __LINE__); \
        return false;                                                                     \
    }

#define expect_should_not_be(expected, actual)                                                \
    if (expected == actual) {                                                                 \
        LOGE("not expected %lld but got %lld , %s:%d", expected, actual, __FILE__, __LINE__); \
        return false;                                                                         \
    }

#define expect_float_should_be(expected, actual, tolerance)                            \
    if (ABS(expected - actual) > tolerance) {                                          \
        LOGE("expected %lf but got %lf, %s:%d", expected, actual, __FILE__, __LINE__); \
        return false;                                                                  \
    }

#endif