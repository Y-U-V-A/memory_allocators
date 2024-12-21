#ifndef POOL_ALLOCATOR__H
#define POOL_ALLOCATOR__H

#include "defines.h"

typedef struct pool_allocator pool_allocator;

pool_allocator* pool_allocator_create(u64 size, u64 chunk_size);

void pool_allocator_destroy(pool_allocator* allocator);

void* pool_allocator_allocate(pool_allocator* allocator);

void pool_allocator_free(pool_allocator* allocator, void* block_addr);

void pool_allocator_reset(pool_allocator* allocator);

u64 pool_allocator_header_size();

#endif