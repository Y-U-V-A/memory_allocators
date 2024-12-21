#ifndef BUDDY_ALLOCATOR__H
#define BUDDY_ALLOCATOR__H

#include "defines.h"

typedef struct buddy_allocator buddy_allocator;

buddy_allocator* buddy_allocator_create(u64 size);

void buddy_allocator_destroy(buddy_allocator* allocator);

void* buddy_allocator_allocate(buddy_allocator* allocator, u64 size);

void buddy_allocator_free(buddy_allocator* allocator, void* block);

void buddy_allocator_reset(buddy_allocator* allocator);

#endif