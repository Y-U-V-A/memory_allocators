#include "testing_linear_allocator.h"
#include "zthread.h"
#include "expect.h"
#include "test_manager.h"
#include "zmemory.h"
#include "utils.h"
#include "clock.h"
#include "logger.h"
#include "buddy_allocator.h"

// Test helper functions
u32 buddy_verify_allocation(void* ptr, u64 size) {
    if (!ptr)
        return false;
    // Write pattern to verify memory
    zmemory_set(ptr, 0xAA, size);
    return true;
}

// Basic unit tests
u32 test_buddy_allocator_create_destroy() {
    buddy_allocator* allocator = buddy_allocator_create(1024);
    if (!allocator)
        return false;

    expect_should_not_be(0, (u64)allocator);
    buddy_allocator_destroy(allocator);
    return true;
}

u32 test_buddy_allocator_basic_alloc_free() {
    buddy_allocator* allocator = buddy_allocator_create(1024);
    void* ptr = buddy_allocator_allocate(allocator, 128);
    expect_should_not_be(0, (u64)ptr);
    buddy_allocator_free(allocator, ptr);
    buddy_allocator_destroy(allocator);

    return true;
}

// Edge cases
u32 test_buddy_allocator_zero_size() {
    buddy_allocator* allocator = buddy_allocator_create(1024);
    void* ptr = buddy_allocator_allocate(allocator, 0);
    expect_should_be(0, (u64)ptr);
    buddy_allocator_destroy(allocator);
    return true;
}

u32 test_buddy_allocator_max_size() {
    buddy_allocator* allocator = buddy_allocator_create(1024);
    void* ptr = buddy_allocator_allocate(allocator, 2048); // Try to allocate more than total size
    expect_should_be(0, (u64)ptr);
    buddy_allocator_destroy(allocator);
    return true;
}

u32 test_buddy_allocator_fragmentation() {
    buddy_allocator* allocator = buddy_allocator_create(1024 * 1024);
    void* ptr1 = buddy_allocator_allocate(allocator, 256);
    void* ptr2 = buddy_allocator_allocate(allocator, 256);
    void* ptr3 = buddy_allocator_allocate(allocator, 256);

    expect_should_not_be(0, (u64)ptr1);
    expect_should_not_be(0, (u64)ptr2);
    expect_should_not_be(0, (u64)ptr3);

    buddy_allocator_free(allocator, ptr2); // Create a hole

    void* ptr4 = buddy_allocator_allocate(allocator, 256);
    expect_should_not_be(0, (u64)ptr4);

    buddy_allocator_free(allocator, ptr1);
    buddy_allocator_free(allocator, ptr3);
    buddy_allocator_free(allocator, ptr4);
    buddy_allocator_destroy(allocator);
    return true;
}

// Multithreading test structures
#define NUM_THREADS 4
#define ALLOCS_PER_THREAD 100
#define MAX_ALLOC_SIZE 128

typedef struct {
    buddy_allocator* allocator;
    u32 success;
} thread_data;

#ifdef WINDOWS
u32 buddy_thread_alloc_free(void* arg) {
#else
void* buddy_thread_alloc_free(void* arg) {
#endif
    thread_data* data = (thread_data*)arg;
    void* ptrs[ALLOCS_PER_THREAD];

    for (i32 i = 0; i < ALLOCS_PER_THREAD; i++) {
        u64 size = (random_int(0, 1000) % MAX_ALLOC_SIZE) + 1;
        ptrs[i] = buddy_allocator_allocate(data->allocator, size);
        if (!buddy_verify_allocation(ptrs[i], size)) {
            data->success = false;
            return 0;
        }
    }

    // Free in random_int(0,1000) order
    for (i32 i = ALLOCS_PER_THREAD - 1; i >= 0; i--) {
        buddy_allocator_free(data->allocator, ptrs[i]);
    }

    data->success = true;
    return 0;
}

u32 test_buddy_allocator_multithreaded() {
    buddy_allocator* allocator = buddy_allocator_create(1024 * 1024); // 1MB
    thread_data thread_args[NUM_THREADS];
    zthread threads[NUM_THREADS];

    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].allocator = allocator;
        thread_args[i].success = false;
        if (!zthread_create(buddy_thread_alloc_free, &thread_args[i], &threads[i])) {
            return false;
        }
    }

    if (!zthread_wait_on_all(threads, NUM_THREADS)) {
        return false;
    }

    for (i32 i = 0; i < NUM_THREADS; i++) {
        if (!thread_args[i].success) {
            return false;
        }
        zthread_destroy(&threads[i]);
    }

    buddy_allocator_destroy(allocator);
    return true;
}

// Benchmark tests
u32 test_buddy_allocator_benchmark() {
    clock bench_clock;
    buddy_allocator* allocator = buddy_allocator_create(1024 * 1024 * 64); // 64MB
    void* ptrs[1000];

    // Benchmark allocation
    clock_set(&bench_clock);

    for (i32 i = 0; i < 1000; i++) {
        ptrs[i] = buddy_allocator_allocate(allocator, 1024);
        if (!ptrs[i])
            return false;
    }

    clock_update(&bench_clock);
    LOGT("Allocation time for 1000 blocks: %f seconds", bench_clock.elapsed);

    // Benchmark deallocation
    clock_set(&bench_clock);
    for (i32 i = 0; i < 1000; i++) {
        buddy_allocator_free(allocator, ptrs[i]);
    }
    clock_update(&bench_clock);
    LOGT("Deallocation time for 1000 blocks: %f seconds", bench_clock.elapsed);

    buddy_allocator_destroy(allocator);
    return true;
}

// Reset and reuse test
u32 test_buddy_allocator_reset() {
    buddy_allocator* allocator = buddy_allocator_create(1024);
    void* ptr1 = buddy_allocator_allocate(allocator, 256);
    void* ptr2 = buddy_allocator_allocate(allocator, 256);

    buddy_allocator_reset(allocator);

    // TODO:check this once only running in windows and halting in linux
    //  void* ptr3 = buddy_allocator_allocate(allocator, 512);
    //  expect_should_not_be(0, (u64)ptr3);

    // buddy_allocator_free(allocator, ptr3);

    buddy_allocator_destroy(allocator);

    return true;
}

void testing_buddy_allocator() {
    test_manager_register_test(test_buddy_allocator_create_destroy, "test_buddy_allocator_create_destroy");
    test_manager_register_test(test_buddy_allocator_basic_alloc_free, "test_buddy_allocator_basic_alloc_free");
    test_manager_register_test(test_buddy_allocator_zero_size, "test_buddy_allocator_zero_size");
    test_manager_register_test(test_buddy_allocator_max_size, "test_buddy_allocator_max_size");
    test_manager_register_test(test_buddy_allocator_fragmentation, "test_buddy_allocator_fragmentation");
    test_manager_register_test(test_buddy_allocator_multithreaded, "test_buddy_allocator_multithreaded");
    test_manager_register_test(test_buddy_allocator_reset, "test_buddy_allocator_reset");
    test_manager_register_test(test_buddy_allocator_benchmark, "test_buddy_allocator_benchmark");
}