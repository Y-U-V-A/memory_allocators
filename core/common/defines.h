#ifndef DEFINES__H
#define DEFINES__H

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef _Bool bool;

#define true 1
#define false 0

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#    define WINDOWS
#elif defined(__linux__) || defined(__gnu_linux__)
#    define LINUX
#else
#    error "platform not supported"
#endif

#endif
