#ifndef STACK_ALLOCATOR__H
#define STACK_ALLOCATOR__H

#include "defines.h"

typedef enum stack_allocator_memory_alignment {
    ALIGNMENT_BYTE_8 = 8,
    ALIGNMENT_BYTE_16 = 16,
    ALIGNMENT_BYTE_32 = 32,
    ALIGNMENT_BYTE_64 = 64,
} stack_allocator_memory_alignment;

typedef struct stack_allocator stack_allocator;

#define stack_allocator_allocate(allocator, size) stack_allocator_allocate_aligned(allocator, size, ALIGNMENT_BYTE_8)
#define stack_allocator_allocate_aligned_8(allocator, size) stack_allocator_allocate_aligned(allocator, size, ALIGNMENT_BYTE_8)
#define stack_allocator_allocate_aligned_16(allocator, size) stack_allocator_allocate_aligned(allocator, size, ALIGNMENT_BYTE_16)
#define stack_allocator_allocate_aligned_32(allocator, size) stack_allocator_allocate_aligned(allocator, size, ALIGNMENT_BYTE_32)
#define stack_allocator_allocate_aligned_64(allocator, size) stack_allocator_allocate_aligned(allocator, size, ALIGNMENT_BYTE_64)

stack_allocator* stack_allocator_create(u64 size);

void stack_allocator_destroy(stack_allocator* allocator);

void* stack_allocator_allocate_aligned(stack_allocator* allocator, u64 size, stack_allocator_memory_alignment memory_alignment);

void stack_allocator_free(stack_allocator* allocator);

void stack_allocator_reset(stack_allocator* allocator);

u64 stack_allocator_used_memory(stack_allocator* allocator);

u64 stack_allocator_unused_memory(stack_allocator* allocator);

#endif