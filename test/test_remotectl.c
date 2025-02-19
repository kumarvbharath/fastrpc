#include "test_framework.h"
#include "remotectl.h"
#include "error.h"
#include <pthread.h>
#include <string.h>

#define NUM_THREADS 4
#define NUM_ITERATIONS 100
#define TEST_ERROR_BUF_SIZE 256

struct thread_ctx {
    int thread_id;
    int success;
    remote_handle64 handle;
    const char* module_name;
};

// Helper function to open and close modules repeatedly
static void* thread_open_close(void* arg) {
    struct thread_ctx* ctx = (struct thread_ctx*)arg;
    char error_buf[TEST_ERROR_BUF_SIZE];
    int error;
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        int module_handle;
        error = 0;

        // Test module open
        int ret = remotectl1_open1(ctx->handle, ctx->module_name, 
                                 &module_handle, error_buf, 
                                 TEST_ERROR_BUF_SIZE, &error);
        if (ret != AEE_SUCCESS) {
            LOG_ERR("Thread %d: Open failed: %s", ctx->thread_id, error_buf);
            ctx->success = 0;
            return NULL;
        }

        // Test module close
        error = 0;
        ret = remotectl1_close1(ctx->handle, module_handle, error_buf, 
                              TEST_ERROR_BUF_SIZE, &error);
        if (ret != AEE_SUCCESS) {
            LOG_ERR("Thread %d: Close failed: %s", ctx->thread_id, error_buf);
            ctx->success = 0;
            return NULL;
        }
    }
    
    ctx->success = 1;
    return NULL;
}

static int test_basic_operations(void) {
    remote_handle64 handle;
    char error_buf[TEST_ERROR_BUF_SIZE];
    int error;
    
    // Test open
    ASSERT_EQUAL(remotectl1_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle), AEE_SUCCESS);

    // Test module open
    int module_handle;
    ASSERT_EQUAL(remotectl1_open1(handle, "test_module", &module_handle, 
                                error_buf, TEST_ERROR_BUF_SIZE, &error), 
                AEE_SUCCESS);

    // Test parameter setting
    uint32_t params[] = {1, 2, 3, 4};
    ASSERT_EQUAL(remotectl1_set_param(handle, 1, params, 4), AEE_SUCCESS);

    // Test heap growth
    ASSERT_EQUAL(remotectl1_grow_heap(handle, 0x1000, 4096), AEE_SUCCESS);

    // Test module close
    ASSERT_EQUAL(remotectl1_close1(handle, module_handle, error_buf, 
                                 TEST_ERROR_BUF_SIZE, &error), 
                AEE_SUCCESS);

    // Test close
    ASSERT_EQUAL(remotectl1_close(handle), AEE_SUCCESS);

    return 0;
}

static int test_negative_cases(void) {
    remote_handle64 handle;
    char error_buf[TEST_ERROR_BUF_SIZE] = "no error";
    int error;
    
    // Test NULL parameters
    ASSERT_NOT_EQUAL(remotectl1_open(NULL, &handle), AEE_SUCCESS);
    ASSERT_NOT_EQUAL(remotectl1_open("test_uri", NULL), AEE_SUCCESS);

    // Setup valid handle for remaining tests
    ASSERT_EQUAL(remotectl1_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle), AEE_SUCCESS);

    // Test invalid module operations
    int module_handle;
    ASSERT_NOT_EQUAL(remotectl1_open1(handle, NULL, &module_handle, 
                                    error_buf, TEST_ERROR_BUF_SIZE, &error), 
                    AEE_SUCCESS);
    ASSERT_NOT_EQUAL(remotectl1_open1(handle, "test_module", NULL, 
                                    error_buf, TEST_ERROR_BUF_SIZE, &error), 
                    AEE_SUCCESS);
    LOG_INF("Error: %s", error_buf);
    // Test invalid parameter setting
    ASSERT_NOT_EQUAL(remotectl1_set_param(handle, 1, NULL, 4), AEE_SUCCESS);
    ASSERT_NOT_EQUAL(remotectl1_set_param(handle, 1, (uint32_t*)0x80000000, -1), 
                    AEE_SUCCESS);

    // Test invalid heap operations
    ASSERT_NOT_EQUAL(remotectl1_grow_heap(handle, 0, 0), AEE_SUCCESS);

    // Cleanup
    ASSERT_EQUAL(remotectl1_close(handle), AEE_SUCCESS);

    return 0;
}

static int test_concurrent_operations(void) {
    remote_handle64 handle;
    struct thread_ctx threads[NUM_THREADS];
    pthread_t thread_ids[NUM_THREADS];
    
    // Setup main handle
    ASSERT_EQUAL(remotectl1_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle), AEE_SUCCESS);

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads[i].thread_id = i;
        threads[i].handle = handle;
        threads[i].module_name = "test_module";
        threads[i].success = 0;
        
        ASSERT_EQUAL(pthread_create(&thread_ids[i], NULL, 
                                  thread_open_close, &threads[i]), 0);
    }

    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(thread_ids[i], NULL);
        ASSERT_TRUE(threads[i].success);
    }

    // Cleanup
    ASSERT_EQUAL(remotectl1_close(handle), AEE_SUCCESS);

    return 0;
}

static int test_stress_operations(void) {
    remote_handle64 handle;
    char error_buf[TEST_ERROR_BUF_SIZE];
    int error;
    
    ASSERT_EQUAL(remotectl1_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle), AEE_SUCCESS);

    // Rapid open/close sequence
    for (int i = 0; i < 1000; i++) {
        int module_handle;
        ASSERT_EQUAL(remotectl1_open1(handle, "test_module", &module_handle, 
                                    error_buf, TEST_ERROR_BUF_SIZE, &error), 
                    AEE_SUCCESS);
        ASSERT_EQUAL(remotectl1_close1(handle, module_handle, error_buf, 
                                     TEST_ERROR_BUF_SIZE, &error), 
                    AEE_SUCCESS);
    }

    ASSERT_EQUAL(remotectl1_close(handle), AEE_SUCCESS);
    return 0;
}

int main(void) {
    LOG_INF("Starting remotectl tests");

    RUN_TEST(test_basic_operations);
    RUN_TEST(test_negative_cases);
    RUN_TEST(test_concurrent_operations);
    RUN_TEST(test_stress_operations);

    LOG_INF("All remotectl tests completed");
    return 0;
}