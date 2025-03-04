// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "test_framework.h"
#include "config.h"
#include <pthread.h>
#include <string.h>
#include <unistd.h>

// Thread test context
struct thread_test_ctx {
    config_store_t *store;
    int thread_id;
    int success;
};

static void* concurrent_different_keys(void *arg) {
    struct thread_test_ctx *ctx = (struct thread_test_ctx*)arg;
    char key[32];
    ctx->success = 1;

    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, i);
        int *value = malloc(sizeof(int));
        *value = i;
        
        if (config_store_set(ctx->store, key, value, sizeof(int)) != AEE_SUCCESS) {
            ctx->success = 0;
            break;
        }
        
        void *retrieved = config_store_get(ctx->store, key);
        if (!retrieved || *(int*)retrieved != i) {
            ctx->success = 0;
            break;
        }
    }
    return NULL;
}

static void* concurrent_same_key(void *arg) {
    struct thread_test_ctx *ctx = (struct thread_test_ctx*)arg;
    const char *key = "shared_key";
    ctx->success = 1;

    for (int i = 0; i < 100; i++) {
        int *value = malloc(sizeof(int));
        *value = ctx->thread_id * 1000 + i;
        
        if (config_store_set(ctx->store, key, value, sizeof(int)) != AEE_SUCCESS) {
            ctx->success = 0;
            break;
        }
        
        void *retrieved = config_store_get(ctx->store, key);
        if (!retrieved) {
            ctx->success = 0;
            break;
        }
    }
    return NULL;
}

static int test_config_basic(void) {
    printf("\nTesting basic config operations...\n");
    
    config_store_t *store = config_store_create();
    ASSERT_NOT_NULL(store);

    int *value = malloc(sizeof(int));
    *value = 42;
    ASSERT_EQUAL(config_store_set(store, "test_key", value, sizeof(int)), AEE_SUCCESS);

    void *retrieved = config_store_get(store, "test_key");
    ASSERT_NOT_NULL(retrieved);
    ASSERT_EQUAL(*(int*)retrieved, 42);

    config_store_destroy(store);
    return 0;
}

static int test_config_negative(void) {
    printf("\nTesting negative cases...\n");
    
    ASSERT_NULL(config_store_get(NULL, "key"));
    
    config_store_t *store = config_store_create();
    ASSERT_NOT_NULL(store);
    
    ASSERT_NULL(config_store_get(store, NULL));
    ASSERT_NULL(config_store_get(store, "nonexistent"));
    
    char long_key[64];
    memset(long_key, 'a', 63);
    long_key[63] = '\0';
    int *value = malloc(sizeof(int));
    *value = 42;
    ASSERT_EQUAL(config_store_set(store, long_key, value, sizeof(int)), AEE_SUCCESS);

    config_store_destroy(store);
    return 0;
}

static int test_config_concurrent(void) {
    printf("\nTesting concurrent operations...\n");
    
    config_store_t *store = config_store_create();
    ASSERT_NOT_NULL(store);

    #define NUM_THREADS 4
    pthread_t threads[NUM_THREADS];
    struct thread_test_ctx contexts[NUM_THREADS];

    // Test with different keys
    printf("Testing concurrent access with different keys...\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].store = store;
        contexts[i].thread_id = i;
        contexts[i].success = 0;
        ASSERT_EQUAL(pthread_create(&threads[i], NULL, concurrent_different_keys, &contexts[i]), 0);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        ASSERT_TRUE(contexts[i].success);
    }

    // Test with same key
    printf("Testing concurrent access with same key...\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].store = store;
        contexts[i].thread_id = i;
        contexts[i].success = 0;
        ASSERT_EQUAL(pthread_create(&threads[i], NULL, concurrent_same_key, &contexts[i]), 0);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        ASSERT_TRUE(contexts[i].success);
    }

    config_store_destroy(store);
    return 0;
}

int main(void) {
    printf("Starting config store tests...\n");
    
    RUN_TEST(test_config_basic);
    RUN_TEST(test_config_negative);
    RUN_TEST(test_config_concurrent);
    
    printf("\nAll config store tests completed successfully!\n");
    return 0;
}