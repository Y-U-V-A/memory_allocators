#include "testing_stack_allocator.h"
#include "zthread.h"
#include "expect.h"
#include "test_manager.h"
#include "zmemory.h"
#include "utils.h"
#include "clock.h"
#include "stack_allocator.h"

// Unit Tests
u32 stack_test_create_destroy() {
    stack_allocator* allocator = stack_allocator_create(1024);
    if (!allocator)
        return false;

    expect_should_be(0, stack_allocator_used_memory(allocator));
    expect_should_be(1024, stack_allocator_unused_memory(allocator));

    stack_allocator_destroy(allocator);
    return true;
}

u32 stack_test_basic_allocation() {
    stack_allocator* allocator = stack_allocator_create(1024);

    void* block1 = stack_allocator_allocate(allocator, 256);
    expect_should_not_be(0, block1);
    u64 used_after_first = stack_allocator_used_memory(allocator);

    void* block2 = stack_allocator_allocate(allocator, 256);
    expect_should_not_be(0, block2);
    u64 used_after_second = stack_allocator_used_memory(allocator);

    // Test LIFO order - must free block2 before block1
    stack_allocator_free(allocator); // Frees block2
    expect_should_be(used_after_first, stack_allocator_used_memory(allocator));

    stack_allocator_free(allocator); // Frees block1
    expect_should_be(0, stack_allocator_used_memory(allocator));

    stack_allocator_destroy(allocator);
    return true;
}

// Alignment Tests
u32 stack_test_alignments() {
    stack_allocator* allocator = stack_allocator_create(1024);

    // Test 8-byte alignment
    void* block8 = stack_allocator_allocate_aligned_8(allocator, 10);
    expect_should_be(0, ((u64)block8 & 7));

    // Test 16-byte alignment
    void* block16 = stack_allocator_allocate_aligned_16(allocator, 10);
    expect_should_be(0, ((u64)block16 & 15));

    // Test 32-byte alignment
    void* block32 = stack_allocator_allocate_aligned_32(allocator, 10);
    expect_should_be(0, ((u64)block32 & 31));

    // Test 64-byte alignment
    void* block64 = stack_allocator_allocate_aligned_64(allocator, 10);
    expect_should_be(0, ((u64)block64 & 63));

    // Must free in reverse order
    stack_allocator_free(allocator); // block64
    stack_allocator_free(allocator); // block32
    stack_allocator_free(allocator); // block16
    stack_allocator_free(allocator); // block8

    stack_allocator_destroy(allocator);
    return true;
}

// Stack Order Tests
u32 stack_test_stack_order() {
    stack_allocator* allocator = stack_allocator_create(1024);

    // Allocate blocks of different sizes to test stack behavior
    void* block1 = stack_allocator_allocate(allocator, 64);
    u64 used1 = stack_allocator_used_memory(allocator);

    void* block2 = stack_allocator_allocate(allocator, 128);
    u64 used2 = stack_allocator_used_memory(allocator);

    void* block3 = stack_allocator_allocate(allocator, 256);
    u64 used3 = stack_allocator_used_memory(allocator);

    // Free in reverse order
    stack_allocator_free(allocator); // block3
    expect_should_be(used2, stack_allocator_used_memory(allocator));

    stack_allocator_free(allocator); // block2
    expect_should_be(used1, stack_allocator_used_memory(allocator));

    stack_allocator_free(allocator); // block1
    expect_should_be(0, stack_allocator_used_memory(allocator));

    stack_allocator_destroy(allocator);
    return true;
}

// Edge Cases
u32 stack_test_edge_cases() {
    // Test invalid creation parameters
    expect_should_be(0, stack_allocator_create(0));

    stack_allocator* allocator = stack_allocator_create(1024);

    // Test null/invalid parameters
    expect_should_be(0, stack_allocator_allocate(0, 100));
    expect_should_be(0, stack_allocator_allocate(allocator, 0));

    // Test allocation larger than available space
    expect_should_be(0, stack_allocator_allocate(allocator, 2048));

    // Test freeing empty stack
    stack_allocator_free(allocator);
    expect_should_be(0, stack_allocator_used_memory(allocator));

    // Test allocation that exactly fits
    void* block = stack_allocator_allocate(allocator, 1024 - 8); // Account for header
    expect_should_not_be(0, block);

    stack_allocator_destroy(allocator);
    return true;
}

// Reset Test
u32 stack_test_reset() {
    stack_allocator* allocator = stack_allocator_create(1024);

    // Allocate several blocks
    void* block1 = stack_allocator_allocate(allocator, 64);
    void* block2 = stack_allocator_allocate(allocator, 128);
    void* block3 = stack_allocator_allocate(allocator, 256);

    u64 used_memory = stack_allocator_used_memory(allocator);
    expect_should_not_be(0, used_memory);

    // Reset allocator
    stack_allocator_reset(allocator);
    expect_should_be(0, stack_allocator_used_memory(allocator));

    // Try to allocate again
    void* new_block = stack_allocator_allocate(allocator, 64);
    expect_should_not_be(0, new_block);
    expect_should_be((u64)block1, (u64)new_block); // Should get same address as first block

    stack_allocator_destroy(allocator);
    return true;
}

// Benchmark test
u32 stack_test_benchmark() {
    const u64 TEST_SIZE = 1024 * 1024 * 64; // 64MB
    const u64 ITERATIONS = 10000;
    const u64 BLOCK_SIZE = 1024; // 1KB

    stack_allocator* allocator = stack_allocator_create(TEST_SIZE);
    clock benchmark_clock;

    // Benchmark stack push (allocation)
    clock_set(&benchmark_clock);

    for (u64 i = 0; i < ITERATIONS; i++) {
        void* block = stack_allocator_allocate(allocator, BLOCK_SIZE);
        if (!block) {
            if (i < ITERATIONS / 2) {
                LOGE("Benchmark allocation failed too early at iteration %llu", i);
                return false;
            }
            break;
        }
    }

    clock_update(&benchmark_clock);
    f64 push_time = benchmark_clock.elapsed;

    // Benchmark stack pop (free)
    clock_set(&benchmark_clock);

    u64 free_count = stack_allocator_used_memory(allocator) / (BLOCK_SIZE + 8);
    for (u64 i = 0; i < free_count; i++) {
        stack_allocator_free(allocator);
    }

    clock_update(&benchmark_clock);
    f64 pop_time = benchmark_clock.elapsed;

    // Benchmark push/pop cycle
    clock_set(&benchmark_clock);

    for (u64 i = 0; i < ITERATIONS; i++) {
        void* block = stack_allocator_allocate(allocator, 64); // Small blocks for more iterations
        if (block) {
            stack_allocator_free(allocator);
        }
    }

    clock_update(&benchmark_clock);
    f64 cycle_time = benchmark_clock.elapsed;

    LOGI("Benchmark Results:");
    LOGI("Push Time (Allocation): %f seconds", push_time);
    LOGI("Pop Time (Free): %f seconds", pop_time);
    LOGI("Push/Pop Cycle Time: %f seconds", cycle_time);
    LOGI("Push/Pop Operations per second: %f", ITERATIONS / cycle_time);

    stack_allocator_destroy(allocator);
    return true;
}

void testing_stack_allocator() {

    test_manager_register_test(stack_test_create_destroy, "stack_test_create_destroy");
    test_manager_register_test(stack_test_basic_allocation, "stack_test_basic_allocation");
    test_manager_register_test(stack_test_alignments, "stack_test_alignments");
    test_manager_register_test(stack_test_stack_order, "stack_test_stack_order");
    test_manager_register_test(stack_test_edge_cases, "stack_test_edge_cases");
    test_manager_register_test(stack_test_reset, "stack_test_reset");
    test_manager_register_test(stack_test_benchmark, "stack_test_benchmark");
}