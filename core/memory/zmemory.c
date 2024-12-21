#include "zmemory.h"
#include "logger.h"
#include "zmutex.h"
#include <string.h>
#include <stdlib.h>

typedef struct zmemory_state {
    u64 allocated_memory;
    zmutex mutex;
} zmemory_state;

static zmemory_state state;

bool zmemory_init() {
    state.allocated_memory = 0;
    if (zmutex_create(&state.mutex)) {
        LOGT("zmemory_init");
        return true;
    }
    return false;
}

void zmemory_destroy() {
    zmutex_destroy(&state.mutex);
    state.allocated_memory = 0;
    LOGT("zmemory_destroy");
}

void* zmemory_allocate(u64 size) {
    void* temp = malloc(size);
    if (temp) {
        zmutex_lock(&state.mutex);
        state.allocated_memory += size;
        zmutex_unlock(&state.mutex);
        memset(temp, 0, size);
    }
    return temp;
}

void zmemory_free(void* block, u64 size) {
    if (block) {
        free(block);
        zmutex_lock(&state.mutex);
        state.allocated_memory -= size;
        zmutex_unlock(&state.mutex);
    }
}

void* zmemory_set(void* block, i32 value, u64 size) {
    return memset(block, value, size);
}

void* zmemory_set_zero(void* block, u64 size) {
    return memset(block, 0, size);
}

void* zmemory_copy(void* dest, const void* src, u64 size) {
    return memcpy(dest, src, size);
}

void* zmemory_move(void* dest, const void* src, u64 size) {
    return memmove(dest, src, size);
}

i32 zmemory_compare(const void* block1, const void* block2, u64 size) {
    return memcmp(block1, block2, size);
}

void zmemory_log() {
    LOGD("current heap allocated memory = %llu", state.allocated_memory);
}
