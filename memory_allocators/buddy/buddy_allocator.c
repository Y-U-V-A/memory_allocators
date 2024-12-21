#include "buddy_allocator.h"
#include "zmemory.h"
#include "logger.h"
#include "zmutex.h"

////////////////////////////////////////////////////////
//  __                        __        __            //
// /  |                      /  |      /  |           //
// $$ |____   __    __   ____$$ |  ____$$ | __    __  //
// $$      \ /  |  /  | /    $$ | /    $$ |/  |  /  | //
// $$$$$$$  |$$ |  $$ |/$$$$$$$ |/$$$$$$$ |$$ |  $$ | //
// $$ |  $$ |$$ |  $$ |$$ |  $$ |$$ |  $$ |$$ |  $$ | //
// $$ |__$$ |$$ \__$$ |$$ \__$$ |$$ \__$$ |$$ \__$$ | //
// $$    $$/ $$    $$/ $$    $$ |$$    $$ |$$    $$ | //
// $$$$$$$/   $$$$$$/   $$$$$$$/  $$$$$$$/  $$$$$$$ | //
//                                         /  \__$$ | //
//                                         $$    $$/  //
//                                          $$$$$$/   //
//                                                    //
////////////////////////////////////////////////////////

#define IS_POWER_OF_TWO(val) (((val) != 0) && (((val) & ((val) - 1)) == 0))
#define BUDDY_HEADER_SIZE sizeof(buddy_header)

typedef struct buddy_header {
    u64 size;
    struct buddy_header* next;
    struct buddy_header* prev;
    u64 unique; // 0xF7B3D591E6A4C208
} buddy_header;

typedef struct buddy_allocator {
    void* block;
    u64 size;
    u64 used;
    buddy_header** freelist;
    u64 freelist_size;
    zmutex mutex;
} buddy_allocator;

u32 get_which_power_of_two(u64 val);
u32 get_nearest_power_of_two(u64 val);
buddy_header* get_buddy(buddy_allocator* allocator, u64 size);
bool free_buddy(buddy_allocator* allocator, buddy_header* buddy);
buddy_header* check_its_buddy(buddy_allocator* allocator, buddy_header* buddy);

buddy_allocator* buddy_allocator_create(u64 size) {
    if (size == 0 || size <= sizeof(buddy_header) || IS_POWER_OF_TWO(size) == 0) {
        LOGE("buddy_allocator_create : invalid params");
        return 0;
    }

    u32 power = get_which_power_of_two(size);

    buddy_allocator* allocator = zmemory_allocate(sizeof(buddy_allocator));
    allocator->block = zmemory_allocate(size);
    allocator->size = size;
    allocator->used = 0;
    allocator->freelist = zmemory_allocate(power * sizeof(buddy_header*));
    allocator->freelist_size = power;
    u32 index = power - 1;
    allocator->freelist[index] = (buddy_header*)allocator->block;
    allocator->freelist[index]->next = 0;
    allocator->freelist[index]->prev = 0;
    allocator->freelist[index]->size = size;
    allocator->freelist[index]->unique = 0xF7B3D591E6A4C208;

    if (!zmutex_create(&allocator->mutex)) {
        LOGE("buddy_allocator_create : failed to create zmutex");
        return 0;
    }

    LOGT("buddy_allocator_create");
    return allocator;
}

void buddy_allocator_destroy(buddy_allocator* allocator) {
    if (allocator == 0) {
        LOGE("buddy_allocator_destroy : invalid params");
        return;
    }
    zmutex_destroy(&allocator->mutex);
    zmemory_free(allocator->freelist, allocator->freelist_size * sizeof(buddy_header*));
    zmemory_free(allocator->block, allocator->size);
    zmemory_free(allocator, sizeof(buddy_allocator));

    LOGT("buddy_allocator_destroy");
}

void* buddy_allocator_allocate(buddy_allocator* allocator, u64 size) {
    if (allocator == 0 || size == 0 || size >= allocator->size) {
        LOGE("buddy_allocator_allocate : invalid params");
        return 0;
    }

    zmutex_lock(&allocator->mutex);
    buddy_header* block = get_buddy(allocator, size);
    zmutex_unlock(&allocator->mutex);

    if (block == 0) {
        LOGW("buddy_allocator_allocate : no free space");
        return 0;
    }

    return (void*)((u8*)block + BUDDY_HEADER_SIZE);
}

void buddy_allocator_free(buddy_allocator* allocator, void* block) {
    if (allocator == 0 || block == 0) {
        LOGE("buddy_allocator_free : invalid params");
        return;
    }

    zmutex_lock(&allocator->mutex);
    buddy_header* buddy = (buddy_header*)((u8*)block - BUDDY_HEADER_SIZE);
    if (((u64)buddy < (u64)allocator->block) ||
        ((u64)buddy >= ((u64)allocator->block + allocator->size)) ||
        buddy->unique != 0xF7B3D591E6A4C208) {
        LOGE("buddy_allocator_free : invalid memory address");
        zmutex_unlock(&allocator->mutex);
        return;
    }
    if (!free_buddy(allocator, buddy)) {
        LOGE("buddy_allocator_free : failed to free block");
    }
    zmutex_unlock(&allocator->mutex);
}

void buddy_allocator_reset(buddy_allocator* allocator) {
    if (allocator == 0) {
        LOGE("buddy_allocator_reset: invalid params");
        return;
    }
    zmutex_lock(&allocator->mutex);

    allocator->used = 0;
    for (u32 i = 0; i < allocator->freelist_size; ++i) {
        allocator->freelist[i] = 0;
    }
    u32 index = allocator->freelist_size - 1;
    allocator->freelist[index] = (buddy_header*)allocator->block;
    allocator->freelist[index]->next = 0;
    allocator->freelist[index]->prev = 0;
    allocator->freelist[index]->size = allocator->size;
    allocator->freelist[index]->unique = 0xF7B3D591E6A4C208;

    zmutex_unlock(&allocator->mutex);

    LOGT("buddy_allocator_reset");
}

u64 buddy_allocator_unused_memory(buddy_allocator* allocator) {
    return allocator->size - allocator->used;
}

u64 buddy_allocator_used_memory(buddy_allocator* allocator) {
    return allocator->used;
}

u64 buddy_allocator_header_size() {
    return BUDDY_HEADER_SIZE;
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

u32 get_which_power_of_two(u64 val) {
    u64 power = 0;
    while (val) {
        val = val >> 1;
        ++power;
    }
    return power;
}

u32 get_nearest_power_of_two(u64 val) {
    if (IS_POWER_OF_TWO(val)) {
        return val;
    }
    u64 power = 1;
    while (power < val) {
        power <<= 1;
    }
    return power;
}

buddy_header* get_buddy(buddy_allocator* allocator, u64 size) {

    u32 actual_size = get_nearest_power_of_two(size + BUDDY_HEADER_SIZE);
    u32 index = get_which_power_of_two(actual_size) - 1;

    while (index < allocator->freelist_size) {
        buddy_header* block = allocator->freelist[index];
        if (block) {
            u64 block_size = (allocator->freelist[index]->size);
            u64 half_block_size = (block_size >> 1);
            u32 i = 1;
            while (half_block_size >= actual_size) {
                buddy_header* half_block = (buddy_header*)((u8*)block + half_block_size);
                half_block->size = half_block_size;
                half_block->next = 0;
                half_block->prev = 0;
                half_block->unique = 0xF7B3D591E6A4C208;

                buddy_header* node = allocator->freelist[index - i];
                while (node && node->next) {
                    node = node->next;
                }
                if (node == 0) {
                    allocator->freelist[index - i] = half_block;
                } else {
                    node->next = half_block;
                    half_block->prev = node;
                }

                block_size = half_block_size;
                half_block_size = (block_size >> 1);
                i += 1;
            }
            allocator->freelist[index] = block->next;
            if (allocator->freelist[index]) {
                allocator->freelist[index]->prev = 0;
            }
            block->next = 0;
            block->prev = 0;
            block->size = block_size;

            allocator->used += block_size;
            return block;
        }

        index += 1;
    }
    return 0;
}

bool free_buddy(buddy_allocator* allocator, buddy_header* buddy) {

    allocator->used -= buddy->size;

    if (buddy->size == allocator->size) {
        buddy_allocator_reset(allocator);
        return true;
    }
    buddy_header* its_buddy;

    while ((its_buddy = check_its_buddy(allocator, buddy)) != 0) {
        buddy = (buddy < its_buddy ? buddy : its_buddy);
        buddy->size = (buddy->size << 1);
        buddy->next = 0;
        buddy->prev = 0;
        buddy->unique = 0xF7B3D591E6A4C208;
    }

    u32 index = get_which_power_of_two(buddy->size) - 1;
    buddy_header* node = allocator->freelist[index];
    while (node && node->next) {
        node = node->next;
    }
    if (node == 0) {
        allocator->freelist[index] = buddy;
    } else {
        node->next = buddy;
        buddy->prev = node;
    }

    return true;
}

buddy_header* check_its_buddy(buddy_allocator* allocator, buddy_header* buddy) {

    u32 index = get_which_power_of_two(buddy->size) - 1;
    buddy_header* node = allocator->freelist[index];
    buddy_header* its_front_buddy = (buddy_header*)((u64)buddy - buddy->size);
    buddy_header* its_back_buddy = (buddy_header*)((u64)buddy + buddy->size);

    if ((u64)its_front_buddy < (u64)allocator->block) {
        its_front_buddy = 0;
    }
    if ((u64)its_back_buddy >= (u64)allocator->block + allocator->size) {
        its_back_buddy = 0;
    }

    while (node) {
        if (node == its_front_buddy) {
            if (node->prev) {
                node->prev->next = 0;
            } else {
                allocator->freelist[index] = node->next;
                if (node->next) {
                    node->next->prev = 0;
                }
            }
            node->unique = 0;
            return its_front_buddy;
        }
        if (node == its_back_buddy) {
            if (node->prev) {
                node->prev->next = 0;
            } else {
                allocator->freelist[index] = node->next;
                if (node->next) {
                    node->next->prev = 0;
                }
            }
            node->unique = 0;
            return its_back_buddy;
        }
        node = node->next;
    }

    return 0;
}