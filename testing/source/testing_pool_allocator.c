#include "testing_pool_allocator.h"
#include "zthread.h"
#include "expect.h"
#include "test_manager.h"
#include "zmemory.h"
#include "utils.h"
#include "clock.h"
#include "pool_allocator.h"

// Test helper functions
u32 verify_pool_block(void* ptr, u64 size) {
    if (!ptr)
        return false;
    // Write pattern to verify memory
    zmemory_set(ptr, 0xAA, size);
    // Read back and verify
    unsigned char* bytes = (unsigned char*)ptr;
    for (u64 i = 0; i < size; i++) {
        if (bytes[i] != 0xAA)
            return false;
    }
    return true;
}

// Basic unit tests
u32 test_pool_allocator_create_destroy() {
    pool_allocator* allocator = pool_allocator_create(1024, 32);
    expect_should_not_be(0, (u64)allocator);
    pool_allocator_destroy(allocator);
    return true;
}

u32 test_pool_allocator_invalid_create() {
    // Test non-power-of-two chunk size
    pool_allocator* allocator1 = pool_allocator_create(1024, 30);
    expect_should_be(0, (u64)allocator1);

    // Test chunk size too small
    pool_allocator* allocator2 = pool_allocator_create(1024, 4);
    expect_should_be(0, (u64)allocator2);

    // Test total size too small
    pool_allocator* allocator3 = pool_allocator_create(4, 32);
    expect_should_be(0, (u64)allocator3);

    return true;
}

u32 test_pool_allocator_basic_alloc_free() {
    pool_allocator* allocator = pool_allocator_create(1024, 32);

    void* ptr = pool_allocator_allocate(allocator);
    expect_should_not_be(0, (u64)ptr);

    // Verify we can write to the full chunk size
    if (!verify_pool_block(ptr, 32 - pool_allocator_header_size())) {
        return false;
    }

    pool_allocator_free(allocator, ptr);
    pool_allocator_destroy(allocator);
    return true;
}

// Full allocation test
u32 test_pool_allocator_full_allocation() {
    const u64 total_size = 1024;
    const u64 chunk_size = 32;
    const u64 num_chunks = total_size / chunk_size;

    pool_allocator* allocator = pool_allocator_create(total_size, chunk_size);
    void* ptrs[100]; // Make sure this is larger than num_chunks
    i32 allocated = 0;

    // Allocate until full
    while (allocated < 100) {
        void* ptr = pool_allocator_allocate(allocator);
        if (!ptr)
            break;
        ptrs[allocated++] = ptr;
    }

    // Verify we got the expected number of chunks
    expect_should_be(num_chunks, allocated);

    // Try one more allocation - should fail
    void* ptr = pool_allocator_allocate(allocator);
    expect_should_be(0, (u64)ptr);

    // Free all chunks
    for (i32 i = 0; i < allocated; i++) {
        pool_allocator_free(allocator, ptrs[i]);
    }

    // Should be able to allocate again
    ptr = pool_allocator_allocate(allocator);
    expect_should_not_be(0, (u64)ptr);

    pool_allocator_destroy(allocator);
    return true;
}

// Edge cases
u32 test_pool_allocator_edge_cases() {
    pool_allocator* allocator = pool_allocator_create(1024, 32);

    // Test null allocator
    void* ptr = pool_allocator_allocate(0);
    expect_should_be(0, (u64)ptr);

    // Test null free
    pool_allocator_free(allocator, 0);

    // Test invalid address free
    char dummy;
    pool_allocator_free(allocator, &dummy);

    pool_allocator_destroy(allocator);
    return true;
}

// Reset functionality
u32 test_pool_allocator_reset() {
    pool_allocator* allocator = pool_allocator_create(1024, 32);
    void* ptrs[40];
    i32 count = 0;

    // Allocate several blocks
    while ((ptrs[count] = pool_allocator_allocate(allocator)) != 0) {
        count++;
    }

    // Reset the allocator
    pool_allocator_reset(allocator);

    // Should be able to allocate the same number of blocks again
    i32 new_count = 0;
    void* new_ptr;
    while ((new_ptr = pool_allocator_allocate(allocator)) != 0) {
        new_count++;
    }

    expect_should_be(count, new_count);

    pool_allocator_destroy(allocator);
    return true;
}

// Multithreading test structures
#define NUM_THREADS 4
#define ALLOCS_PER_THREAD 100

typedef struct {
    pool_allocator* allocator;
    u32 success;
} thread_data;

#ifdef WINDOWS
u32 thread_pool_alloc_free(void* arg) {
#else
void* thread_pool_alloc_free(void* arg) {
#endif
    thread_data* data = (thread_data*)arg;
    void* ptrs[ALLOCS_PER_THREAD];
    i32 allocated = 0;

    // Perform allocations
    for (i32 i = 0; i < ALLOCS_PER_THREAD; i++) {
        void* ptr = pool_allocator_allocate(data->allocator);
        if (ptr) {
            ptrs[allocated++] = ptr;
            if (!verify_pool_block(ptr, 32 - pool_allocator_header_size())) {
                data->success = false;
#ifdef WINDOWS
                return 0;
#else
                return 0;
#endif
            }
        }
    }

    // Free in reverse order
    for (i32 i = allocated - 1; i >= 0; i--) {
        pool_allocator_free(data->allocator, ptrs[i]);
    }

    data->success = true;
#ifdef WINDOWS
    return 0;
#else
    return 0;
#endif
}

u32 test_pool_allocator_multithreaded() {
    pool_allocator* allocator = pool_allocator_create(1024 * 1024, 32); // 1MB pool with 32-byte chunks
    thread_data thread_args[NUM_THREADS];
    zthread threads[NUM_THREADS];

    for (i32 i = 0; i < NUM_THREADS; i++) {
        thread_args[i].allocator = allocator;
        thread_args[i].success = false;
        if (!zthread_create(thread_pool_alloc_free, &thread_args[i], &threads[i])) {
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

    pool_allocator_destroy(allocator);
    return true;
}

// Benchmark tests
u32 test_pool_allocator_benchmark() {
    clock bench_clock;
    const i32 num_allocs = 10000;
    void* ptrs[10000];

    // Test different chunk sizes
    const u64 chunk_sizes[] = {32, 64, 128, 256};

    for (u64 i = 0; i < sizeof(chunk_sizes) / sizeof(chunk_sizes[0]); i++) {
        pool_allocator* allocator = pool_allocator_create(chunk_sizes[i] * num_allocs, chunk_sizes[i]);

        // Benchmark allocation
        clock_set(&bench_clock);
        for (i32 j = 0; j < num_allocs; j++) {
            ptrs[j] = pool_allocator_allocate(allocator);
            if (!ptrs[j])
                return false;
        }
        clock_update(&bench_clock);
        LOGT("Allocation time for %d blocks of size %llu: %f seconds",
             num_allocs, chunk_sizes[i], bench_clock.elapsed);

        // Benchmark deallocation
        clock_set(&bench_clock);
        for (i32 j = 0; j < num_allocs; j++) {
            pool_allocator_free(allocator, ptrs[j]);
        }
        clock_update(&bench_clock);
        LOGT("Deallocation time for %d blocks of size %llu: %f seconds",
             num_allocs, chunk_sizes[i], bench_clock.elapsed);

        pool_allocator_destroy(allocator);
    }

    return true;
}

void testing_pool_allocator() {

    test_manager_register_test(test_pool_allocator_create_destroy, "test_pool_allocator_create_destroy");
    test_manager_register_test(test_pool_allocator_invalid_create, "test_pool_allocator_invalid_create");
    test_manager_register_test(test_pool_allocator_basic_alloc_free, "test_pool_allocator_basic_alloc_free");
    test_manager_register_test(test_pool_allocator_full_allocation, "test_pool_allocator_full_allocation");
    test_manager_register_test(test_pool_allocator_edge_cases, "test_pool_allocator_edge_cases");
    test_manager_register_test(test_pool_allocator_reset, "test_pool_allocator_reset");
    test_manager_register_test(test_pool_allocator_multithreaded, "test_pool_allocator_multithreaded");
    test_manager_register_test(test_pool_allocator_benchmark, "test_pool_allocator_benchmark");
}