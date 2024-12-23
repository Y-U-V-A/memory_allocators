#include "stack_allocator.h"
#include "zmemory.h"
#include "logger.h"

#define ALIGN_UP(val, alignment) (((val) + ((alignment) - 1)) & (~((alignment) - 1)))
#define IS_POWER_OF_TWO(val) (((val) != 0) && (((val) & ((val) - 1)) == 0))
#define HEADER_SIZE 8

typedef struct stack_allocator {
    void* block;
    u64 size;
    u64 used;
} stack_allocator;

stack_allocator* stack_allocator_create(u64 size) {
    if (size == 0) {
        LOGE("stack_allocator_create : invalid params");
        return 0;
    }
    stack_allocator* allocator = zmemory_allocate(sizeof(stack_allocator));
    allocator->block = zmemory_allocate(size);
    if (allocator->block == 0) {
        LOGE("stack_allocator_create : failed to allocate memory ");
        zmemory_free(allocator, sizeof(stack_allocator));
        return 0;
    }
    allocator->size = size;
    allocator->used = 0;
    LOGT("stack_allocator_create");
    return allocator;
}

void stack_allocator_destroy(stack_allocator* allocator) {
    if (allocator == 0) {
        LOGE("stack_allocator_destroy : invalid params");
        return;
    }

    zmemory_free(allocator->block, allocator->size);
    zmemory_free(allocator, sizeof(stack_allocator));
    LOGT("stack_allocator_destroy");
    return;
}

void* stack_allocator_allocate_aligned(stack_allocator* allocator, u64 size, stack_allocator_memory_alignment memory_alignment) {
    if (allocator == 0 || size == 0 || IS_POWER_OF_TWO(memory_alignment) == 0 || size > allocator->size) {
        LOGE("stack_allocator_allocate : invalid params");
        return 0;
    }

    size += HEADER_SIZE; // to store the offset at last 8 bytes;

    u64 alignment = (u64)memory_alignment;
    u64 curr_addr = (u64)allocator->block + allocator->used;
    u64 aligned_addr = ALIGN_UP(curr_addr, alignment);
    u64 padding = (aligned_addr - curr_addr);

    if ((allocator->used + padding + size) > allocator->size) {
        LOGW("stack_allocator_allocate: no free space (requested %llu,padding %llu,alignment %llu,available %llu)",
             size, padding, alignment, allocator->size - allocator->used);
        return 0;
    }

    u64* header = (u64*)(aligned_addr + size - HEADER_SIZE);
    *header = allocator->used;

    allocator->used += padding;
    allocator->used += size;

    return (void*)aligned_addr;
}

void stack_allocator_free(stack_allocator* allocator) {
    if (allocator == 0) {
        LOGE("stack_allocator_free : invalid params");
        return;
    }

    if (allocator->used == 0) {
        return;
    }

    u64* header = (u64*)((u64)allocator->block + allocator->used - HEADER_SIZE);
    allocator->used = *header;
}

void stack_allocator_reset(stack_allocator* allocator) {
    if (allocator == 0) {
        LOGE("stack_allocator_reset : invalid params");
        return;
    }
    allocator->used = 0;
    LOGT("stack_allocator_reset");
}

u64 stack_allocator_used_memory(stack_allocator* allocator) {
    return allocator->used;
}

u64 stack_allocator_unused_memory(stack_allocator* allocator) {
    return allocator->size - allocator->used;
}
