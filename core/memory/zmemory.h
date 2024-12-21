#ifndef ZMEMORY__H
#define ZMEMORY__H

#include "defines.h"

bool zmemory_init();

void zmemory_destroy();

void* zmemory_allocate(u64 size);

void zmemory_free(void* block, u64 size);

void* zmemory_set(void* block, i32 value, u64 size);

void* zmemory_set_zero(void* block, u64 size);

void* zmemory_copy(void* dest, const void* src, u64 size);

void* zmemory_move(void* dest, const void* src, u64 size);

i32 zmemory_compare(const void* block1, const void* block2, u64 size);

void zmemory_log();

#endif