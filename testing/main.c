#include "test_manager.h"
#include "zmemory.h"

#include "testing_linear_allocator.h"
#include "testing_stack_allocator.h"
#include "testing_pool_allocator.h"
#include "testing_freelist_allocator.h"
#include "testing_buddy_allocator.h"

i32 main() {
    zmemory_init();
    zmemory_log();

    test_manager_init();

    // register tests
    testing_linear_allocator();
    testing_stack_allocator();
    testing_pool_allocator();
    testing_freelist_allocator();
    testing_buddy_allocator();

    // run tests
    test_manager_run();

    test_manager_destroy();

    zmemory_log();
    zmemory_destroy();
}