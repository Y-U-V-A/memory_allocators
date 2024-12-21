#include "pool_allocator.h"
#include "zmemory.h"
#include "logger.h"
#include "zmutex.h"

#define IS_POWER_OF_TWO(val) (((val) != 0) && (((val) & ((val) - 1)) == 0))
#define POOL_HEADER_SIZE sizeof(pool_header)

typedef struct pool_header {
    struct pool_header* next;
    u64 unique; // 0xF7B3D591E6A4C208
} pool_header;

typedef struct pool_allocator {
    void* block;
    u64 size;
    u64 block_size;
    pool_header* head;
    zmutex mutex;
} pool_allocator;

pool_allocator* pool_allocator_create(u64 size, u64 chunk_size) {

    if (size == 0 || chunk_size == 0 || IS_POWER_OF_TWO(chunk_size) == 0 ||
        chunk_size <= POOL_HEADER_SIZE || size <= POOL_HEADER_SIZE) {
        LOGE("pool_allocator_create : invalid params");
        return 0;
    }

    pool_allocator* allocator = zmemory_allocate(sizeof(pool_allocator));
    allocator->block = zmemory_allocate(size);
    allocator->size = size;
    allocator->block_size = chunk_size;
    allocator->head = allocator->block;
    allocator->head->next = 0;
    if (!zmutex_create(&allocator->mutex)) {
        LOGE("pool_allocator_create : failed to create zmutex");
        return 0;
    }

    // blocks will be aligned 8 byte by default
    u64 block = (u64)allocator->head;
    u64 last_addr = (u64)allocator->block + size;
    while ((last_addr - block) > chunk_size) {
        pool_header* addr = (pool_header*)block;
        block += chunk_size;
        addr->next = (pool_header*)block;
        addr->unique = 0xF7B3D591E6A4C208;
    }
    pool_header* addr = (pool_header*)block;
    addr->next = 0;
    addr->unique = 0xF7B3D591E6A4C208;

    LOGT("pool_allocator_create");
    return allocator;
}

void pool_allocator_destroy(pool_allocator* allocator) {
    if (allocator == 0) {
        LOGE("pool_allocator_destroy : invalid params");
        return;
    }
    zmutex_destroy(&allocator->mutex);
    zmemory_free(allocator->block, allocator->size);
    zmemory_free(allocator, sizeof(pool_allocator));
}

void* pool_allocator_allocate(pool_allocator* allocator) {
    if (allocator == 0) {
        LOGE("pool_allocator_allocate : invalid params");
        return 0;
    }

    zmutex_lock(&allocator->mutex);

    pool_header* head = allocator->head;

    if (head == 0) {
        LOGW("pool_allocator_allocate : no free space");
        zmutex_unlock(&allocator->mutex);
        return 0;
    }

    allocator->head = head->next;
    head->next = 0;

    zmutex_unlock(&allocator->mutex);

    return ((u8*)head + POOL_HEADER_SIZE); // adding 8 bytes to header
}

void pool_allocator_free(pool_allocator* allocator, void* block) {
    if (allocator == 0 || block == 0) {
        LOGE("pool_allocator_free : invalid params");
        return;
    }

    zmutex_lock(&allocator->mutex);

    pool_header* remove_block = (pool_header*)((u8*)block - POOL_HEADER_SIZE);

    if (((u64)remove_block < (u64)allocator->block) ||
        ((u64)remove_block >= ((u64)allocator->block + allocator->size)) ||
        remove_block->unique != 0xF7B3D591E6A4C208) {
        LOGE("pool_allocator_free : invalid memory address");
        zmutex_unlock(&allocator->mutex);
        return;
    }

    pool_header* head = allocator->head;
    allocator->head = remove_block;
    allocator->head->next = head;

    zmutex_unlock(&allocator->mutex);
}

void pool_allocator_reset(pool_allocator* allocator) {
    if (allocator == 0) {
        LOGE("pool_allocator_reset : invalid params");
        return;
    }
    zmutex_lock(&allocator->mutex);

    allocator->head = allocator->block;
    allocator->head->next = 0;

    u64 block = (u64)allocator->head;
    u64 last_addr = (u64)allocator->block + allocator->size;
    while ((last_addr - block) > allocator->block_size) {
        pool_header* addr = (pool_header*)block;
        block += allocator->block_size;
        addr->next = (pool_header*)block;
        addr->unique = 0xF7B3D591E6A4C208;
    }
    pool_header* addr = (pool_header*)block;
    addr->next = 0;
    addr->unique = 0xF7B3D591E6A4C208;

    zmutex_unlock(&allocator->mutex);
}

u64 pool_allocator_header_size() {
    return POOL_HEADER_SIZE;
}
