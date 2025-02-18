#include "test_framework.h"
#include "rpcmem.h"
#include "error.h"
#include "log.h"
#include <pthread.h>
#include <limits.h>

#define NUM_THREADS 4
#define ALLOC_COUNT 100

struct thread_test_ctx {
    int thread_id;
    int success;
    void **ptrs;
    int num_allocs;
};

static void* concurrent_alloc_free(void *arg) {
    struct thread_test_ctx *ctx = (struct thread_test_ctx*)arg;
    ctx->success = 1;

    for (int i = 0; i < ctx->num_allocs; i++) {
        size_t size = (i % 4096) + 1; // Vary sizes from 1 to 4096 bytes
        void *ptr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, size);
        if (!ptr) {
            LOG_ERR("Thread %d: Failed to allocate %zu bytes", ctx->thread_id, size);
            ctx->success = 0;
            break;
        }
        ctx->ptrs[i] = ptr;
    }

    // Free every other allocation to test interleaved free
    for (int i = 0; i < ctx->num_allocs; i += 2) {
        if (ctx->ptrs[i]) {
            rpcmem_free(ctx->ptrs[i]);
            ctx->ptrs[i] = NULL;
        }
    }

    return NULL;
}

static int test_init_deinit(void) {
    LOG_INF("Testing init/deinit sequence");
    
    // Test double init
    rpcmem_init();
    // Try second init - should log error
    rpcmem_init();

    // Test deinit
    rpcmem_deinit();

    // Test double deinit - should log error
    rpcmem_deinit();

    return 0;
}

static int test_basic_alloc_free(void) {
    LOG_INF("Testing basic allocation and free");
    
    rpcmem_init();

    // Test single byte allocation
    void *ptr1 = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, 1);
    ASSERT_NOT_NULL(ptr1);

    // Test 4K allocation
    void *ptr2 = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, 4096);
    ASSERT_NOT_NULL(ptr2);

    // Free allocations
    rpcmem_free(ptr1);
    rpcmem_free(ptr2);

    rpcmem_deinit();
    return 0;
}

static int test_negative_cases(void) {
    LOG_INF("Testing negative cases");

    // Test allocation before init
    void *ptr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, 1024);
    ASSERT_NULL(ptr);

    rpcmem_init();

    // Test invalid sizes
    ptr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, 0);
    ASSERT_NULL(ptr);
    
    ptr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, -1);
    ASSERT_NULL(ptr);

    // Test huge allocation
    ptr = rpcmem_alloc2(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, SIZE_MAX);
    ASSERT_NULL(ptr);

    // Test NULL pointer free
    rpcmem_free(NULL);

    // Test invalid pointer free
    rpcmem_free((void*)0x12345678);

    rpcmem_deinit();
    return 0;
}

static int test_concurrent_operations(void) {
    LOG_INF("Testing concurrent operations");

    rpcmem_init();

    pthread_t threads[NUM_THREADS];
    struct thread_test_ctx contexts[NUM_THREADS];
    
    // Initialize thread contexts
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].thread_id = i;
        contexts[i].success = 0;
        contexts[i].num_allocs = ALLOC_COUNT;
        contexts[i].ptrs = calloc(ALLOC_COUNT, sizeof(void*));
        ASSERT_NOT_NULL(contexts[i].ptrs);
    }

    // Start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_EQUAL(pthread_create(&threads[i], NULL, concurrent_alloc_free, &contexts[i]), 0);
    }

    // Wait for threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        ASSERT_TRUE(contexts[i].success);

        // Free remaining allocations
        for (int j = 0; j < contexts[i].num_allocs; j++) {
            if (contexts[i].ptrs[j]) {
                rpcmem_free(contexts[i].ptrs[j]);
            }
        }
        free(contexts[i].ptrs);
    }

    rpcmem_deinit();
    return 0;
}

static int test_deinit_with_active_allocs(void) {
    LOG_INF("Testing deinit with active allocations");

    rpcmem_init();

    // Make some allocations
    void *ptr = rpcmem_alloc(RPCMEM_HEAP_ID_SYSTEM, RPCMEM_DEFAULT_FLAGS, 1024);
    ASSERT_NOT_NULL(ptr);

    // Clean up before deinit
    rpcmem_free(ptr);
    
    rpcmem_deinit();
    return 0;
}

int main(void) {
    LOG_INF("Starting RPCMEM tests");

    RUN_TEST(test_init_deinit);
    RUN_TEST(test_basic_alloc_free);
    RUN_TEST(test_negative_cases);
    RUN_TEST(test_concurrent_operations);
    RUN_TEST(test_deinit_with_active_allocs);

    LOG_INF("All RPCMEM tests completed");
    return 0;
}