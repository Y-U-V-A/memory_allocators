#include "freelist_allocator.h"
#include "logger.h"
#include "zmemory.h"
#include "zmutex.h"
//////////////////////////////////////////////////////////////////////////////
//   ______                                     __  __              __      //
//  /      \                                   /  |/  |            /  |     //
// /$$$$$$  |______    ______    ______        $$ |$$/   _______  _$$ |_    //
// $$ |_ $$//      \  /      \  /      \       $$ |/  | /       |/ $$   |   //
// $$   |  /$$$$$$  |/$$$$$$  |/$$$$$$  |      $$ |$$ |/$$$$$$$/ $$$$$$/    //
// $$$$/   $$ |  $$/ $$    $$ |$$    $$ |      $$ |$$ |$$      \   $$ | __  //
// $$ |    $$ |      $$$$$$$$/ $$$$$$$$/       $$ |$$ | $$$$$$  |  $$ |/  | //
// $$ |    $$ |      $$       |$$       |      $$ |$$ |/     $$/   $$  $$/  //
// $$/     $$/        $$$$$$$/  $$$$$$$/       $$/ $$/ $$$$$$$/     $$$$/   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#define ALIGN_UP(val, alignment) (((val) + ((alignment) - 1)) & (~((alignment) - 1)))
#define FREE_LIST_HEADER_SIZE sizeof(freelist_header)

typedef struct freelist_header {
    u64 size;
    struct freelist_header* next;
    u64 unique; // 0xF7B3D591E6A4C208;
} freelist_header;

typedef struct freelist_allocator {
    void* block;
    u64 size;
    u64 used;
    freelist_header* head;
    zmutex mutex;
} freelist_allocator;

freelist_header* is_connected(freelist_allocator* allocator, u64 addr);

freelist_allocator* freelist_allocator_create(u64 size) {
    if (size == 0 || size <= FREE_LIST_HEADER_SIZE) {
        LOGE("freelist_create : invalid params");
        return 0;
    }
    // initial memory addr returned by malloc will be 8 byte aligned
    freelist_allocator* allocator = zmemory_allocate(sizeof(freelist_allocator));
    allocator->block = zmemory_allocate(size);
    allocator->size = size;
    allocator->used = 0;
    allocator->head = allocator->block;
    allocator->head->size = size;
    allocator->head->next = 0;
    allocator->head->unique = 0xF7B3D591E6A4C208;
    if (!zmutex_create(&allocator->mutex)) {
        LOGE("freelist_create : failed to create mutex");
        return 0;
    }

    LOGT("freelist_allocator_create");

    return allocator;
}

void freelist_allocator_destroy(freelist_allocator* allocator) {
    if (allocator == 0) {
        LOGE("freelist_allocator_destroy : invalid params");
        return;
    }
    zmutex_destroy(&allocator->mutex);
    zmemory_free(allocator->block, allocator->size);
    zmemory_free(allocator, sizeof(freelist_allocator));

    LOGT("freelist_allocator_destroy");
}

void* freelist_allocator_allocate(freelist_allocator* allocator, u64 size) {
    if (allocator == 0 || size == 0 || size >= allocator->size) {
        LOGE("freelist_allocator_allocate : invalid params");
        return 0;
    }

    zmutex_lock(&allocator->mutex);

    // by default using best fit allocation

    freelist_header* block = allocator->head;
    freelist_header* block_prev = block;
    freelist_header* curr_best = 0;
    freelist_header* curr_best_prev = 0;
    size += FREE_LIST_HEADER_SIZE;

    i32 min_extra = 100000000;
    while (block) {
        i32 diff = block->size - size;
        if (diff >= 0 && diff <= min_extra) {
            min_extra = diff;
            curr_best_prev = block_prev;
            curr_best = block;
        }
        block_prev = block;
        block = block->next;
    }
    if (curr_best == 0) {
        LOGW("freelist_allocator_allocate : no free space");
        zmutex_unlock(&allocator->mutex);

        return 0;
    }

    u64 addr = (u64)curr_best;
    u64 end = addr + size;
    u64 aligned_addr = ALIGN_UP(end, 8); // default 8 byte alignment
    u64 used_size = aligned_addr - addr; // including padding
    u64 unused_size = curr_best->size - used_size;

    if (unused_size > FREE_LIST_HEADER_SIZE) {
        freelist_header* split_block = (void*)aligned_addr;
        split_block->size = unused_size;
        split_block->next = curr_best->next;
        split_block->unique = 0xF7B3D591E6A4C208;

        curr_best->size = used_size;

        if (curr_best == curr_best_prev) {
            allocator->head = split_block;
        } else {
            curr_best_prev->next = split_block;
        }

    } else {
        curr_best_prev->next = curr_best->next;
    }

    curr_best->next = 0;
    allocator->used += size;

    zmutex_unlock(&allocator->mutex);

    return (void*)(addr + FREE_LIST_HEADER_SIZE);
}

void freelist_allocator_free(freelist_allocator* allocator, void* block_addr) {
    if (allocator == 0 || block_addr == 0) {
        LOGE("freelist_allocator_free : invalid params");
        return;
    }

    zmutex_lock(&allocator->mutex);

    freelist_header* remove_block = (freelist_header*)((u64)block_addr - FREE_LIST_HEADER_SIZE);

    if (((u64)remove_block < (u64)allocator->block) ||
        ((u64)remove_block >= ((u64)allocator->block + allocator->size)) ||
        remove_block->unique != 0xF7B3D591E6A4C208) {
        LOGE("freelist_allocator_free : invalid memory address");
        zmutex_unlock(&allocator->mutex);
        return;
    }

    allocator->used -= remove_block->size;
    freelist_header* block = allocator->head;
    freelist_header* block_prev = block;

    while (block) {
        u64 addr = (u64)block;
        if (addr + block->size == (u64)remove_block) {
            block->size += remove_block->size;
            remove_block->unique = 0;
            freelist_header* node;
            while ((node = is_connected(allocator, (u64)block + block->size)) != 0) {
                block->size += node->size;
            }
            zmutex_unlock(&allocator->mutex);
            return;
        }
        if (addr - remove_block->size == (u64)remove_block) {
            remove_block->next = block->next;
            remove_block->size += block->size;
            remove_block->unique = 0xF7B3D591E6A4C208;
            block->unique = 0;
            if (block_prev == block) {
                allocator->head = remove_block;
            } else {
                block_prev->next = remove_block;
            }
            zmutex_unlock(&allocator->mutex);
            return;
        }

        block_prev = block;
        block = block->next;
    }

    block_prev->next = remove_block;
    remove_block->next = 0;
    remove_block->unique = 0xF7B3D591E6A4C208;

    zmutex_unlock(&allocator->mutex);
}

void freelist_allocator_reset(freelist_allocator* allocator) {
    if (allocator == 0) {
        LOGE("freelist_allocator_reset : invalid params");
        return;
    }
    zmutex_lock(&allocator->mutex);

    allocator->used = 0;
    allocator->head = allocator->block;
    allocator->head->size = allocator->size;
    allocator->head->next = 0;
    allocator->head->unique = 0xF7B3D591E6A4C208;

    zmutex_unlock(&allocator->mutex);

    LOGT("freelist_allocator_reset");
}

u64 freelist_allocator_used_memory(freelist_allocator* allocator) {
    return allocator->used;
}

u64 freelist_allocator_unused_memory(freelist_allocator* allocator) {
    return allocator->size - allocator->used;
}

u64 freelist_allocator_header_size() {
    return FREE_LIST_HEADER_SIZE;
}

//////////////////////////////////////////////////////////////////////
//  __                  __                                          //
// /  |                /  |                                         //
// $$ |____    ______  $$ |  ______    ______    ______    _______  //
// $$      \  /      \ $$ | /      \  /      \  /      \  /       | //
// $$$$$$$  |/$$$$$$  |$$ |/$$$$$$  |/$$$$$$  |/$$$$$$  |/$$$$$$$/  //
// $$ |  $$ |$$    $$ |$$ |$$ |  $$ |$$    $$ |$$ |  $$/ $$      \  //
// $$ |  $$ |$$$$$$$$/ $$ |$$ |__$$ |$$$$$$$$/ $$ |       $$$$$$  | //
// $$ |  $$ |$$       |$$ |$$    $$/ $$       |$$ |      /     $$/  //
// $$/   $$/  $$$$$$$/ $$/ $$$$$$$/   $$$$$$$/ $$/       $$$$$$$/   //
//                         $$ |                                     //
//                         $$ |                                     //
//                         $$/                                      //
//                                                                  //
//////////////////////////////////////////////////////////////////////

freelist_header* is_connected(freelist_allocator* allocator, u64 addr) {
    freelist_header* curr = allocator->head;
    freelist_header* prev = curr;

    while (curr) {
        if ((u64)curr == addr) {

            if (prev == curr) {
                allocator->head = curr->next;
                curr->next = 0;
            } else {
                prev->next = curr->next;
                curr->next = 0;
            }
            curr->unique = 0;
            return curr;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}