/**
 * @file test_procbuf.c
 * @brief Unit Tests for Process Buffer Implementation
 *
 * Tests:
 * - Basic parameter addition
 * - Multi-threaded access
 * - Error handling
 * - Buffer limits
 * - Session management
 * - Extended domain handling
 */

#include <pthread.h>
#include <string.h>
#include "test_framework.h"
#include "error.h"
#include "log.h"
#include "procbuf.h"

#define NUM_THREADS 4
#define NUM_PARAMS  10
#define TEST_DOMAIN 1
#define TEST_SESSION_ID 100

static struct dsp mock_dsp = {
    .dsp_id = TEST_DOMAIN
};

static struct session mock_session = {
    .dsp = &mock_dsp,
    .session_id = TEST_SESSION_ID
};

static pthread_barrier_t thread_barrier;

static void setup(void) {
    pthread_barrier_init(&thread_barrier, NULL, NUM_THREADS);
}

static void teardown(void) {
    pthread_barrier_destroy(&thread_barrier);
}

// Test basic parameter addition
static int test_add_param_basic(void) {
    uint32_t test_data = 0x12345678;
    int fd;
    void *addr;
    uint32_t size;
    int ret_val;

    ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, &mock_session, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_SUCCESS);

    ASSERT_EQUAL(procbuf_add_param(&mock_session, 1, &test_data, sizeof(test_data)), AEE_SUCCESS);
    ASSERT_EQUAL(procbuf_get_info(&mock_session, &fd, &addr, &size), AEE_SUCCESS);

    ASSERT_TRUE(fd > 0);
    ASSERT_NOT_NULL(addr);

    // Verify parameter layout
    param_header_t *hdr = get_param_header(addr, 0);
    ASSERT_EQUAL(hdr->id, 1);
    ASSERT_EQUAL(hdr->size, sizeof(test_data));
    ASSERT_EQUAL(*(uint32_t*)get_param_data(hdr), test_data);
    return 0;
}

// Test invalid parameters
static int test_invalid_params(void) {
    uint32_t test_data = 0x12345678;

    ASSERT_EQUAL(AEE_EINVALIDPARAM, procbuf_add_param(NULL, 1, 
                                                  &test_data, sizeof(test_data)));
    ASSERT_EQUAL(AEE_EINVALIDPARAM, procbuf_add_param(&mock_session, 1, 
                                                  NULL, sizeof(test_data)));
    ASSERT_EQUAL(AEE_EINVALIDPARAM, procbuf_add_param(&mock_session, 1, 
                                                  &test_data, 0));
    return 0;
}

// Test buffer limits
static int test_buffer_limits(void) {
    char large_data[PROC_BUF_SIZE];
    int ret_val;
    memset(large_data, 0x55, sizeof(large_data));

    ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, &mock_session, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_SUCCESS);
    ASSERT_EQUAL(procbuf_add_param(&mock_session, 1, 
                large_data, sizeof(large_data)), AEE_ENOMEM);
    return 0;
}

// Test multiple parameters
static int test_multiple_params(void) {
    uint32_t values[NUM_PARAMS];
    int fd;
    void *addr;
    uint32_t size;
    int ret_val;

    ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, &mock_session, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_SUCCESS);

    // Add parameters
    for (int i = 0; i < NUM_PARAMS; i++) {
        values[i] = i;
        ASSERT_EQUAL(procbuf_add_param(&mock_session, i, &values[i], sizeof(values[i])), AEE_SUCCESS);
    }

    // Verify all parameters
    ASSERT_EQUAL(procbuf_get_info(&mock_session, &fd, &addr, &size), AEE_SUCCESS);
    
    for (int i = 0; i < NUM_PARAMS; i++) {
        param_header_t *hdr = get_param_header(addr, i);
        LOG_INF("Param %d: id=%u size=%u data=%u", i, hdr->id, hdr->size, *(uint32_t*)get_param_data(hdr));
        ASSERT_EQUAL(hdr->id, i);
        ASSERT_EQUAL(hdr->size, sizeof(values[i]));
        ASSERT_EQUAL(*(uint32_t*)get_param_data(hdr), values[i]);
    }

    return 0;
}

// Thread function for concurrent access test
static void* thread_func(void* arg) {
    int thread_id = *(int*)arg;
    uint32_t value = thread_id;

    // Wait for all threads
    pthread_barrier_wait(&thread_barrier);

    // Add parameters concurrently
    for (int i = 0; i < 6; i++) {
        if (procbuf_add_param(&mock_session, thread_id * 100 + i, 
                             &value, sizeof(value)) != AEE_SUCCESS) {
            return (void*)1;
        }
    }
    return NULL;
}

// Test concurrent access
static int test_concurrent_access(void) {
    pthread_t threads[NUM_THREADS];
    void *status;
    int ret_val;

    ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, &mock_session, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_SUCCESS);

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_func, &i);
    }

    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], &status);
        ASSERT_NULL(status);
    }

    // Verify total parameters
    int fd;
    void *addr;
    uint32_t size;
    ASSERT_EQUAL(procbuf_get_info(&mock_session, &fd, &addr, &size), AEE_SUCCESS);
    
    uint32_t *param_count = (uint32_t*)addr;
    ASSERT_EQUAL(*param_count, NUM_THREADS * 6);
    return 0;
}

// Test extended domain parameter addition
static int test_extended_domain(void) {
    uint32_t test_data = 0xABCD1234;
    int ret_val;
    uint32_t ext_domain = MAKE_EXTENDED_DOMAIN_ID(TEST_DOMAIN, TEST_SESSION_ID);

    ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, &mock_session, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_SUCCESS);
    ASSERT_EQUAL(procbuf_add_param_ext(ext_domain, 1, 
                &test_data, sizeof(test_data)), AEE_SUCCESS);

    int fd;
    void *addr;
    uint32_t size;
    ASSERT_EQUAL(procbuf_get_info(&mock_session, &fd, &addr, &size), AEE_SUCCESS);

    // Verify parameter layout
    param_header_t *hdr = get_param_header(addr, 0);
    ASSERT_EQUAL(hdr->id, 1);
    ASSERT_EQUAL(hdr->size, sizeof(test_data));
    ASSERT_EQUAL(*(uint32_t*)get_param_data(hdr), test_data);
    return 0;
}

// Test callback errors
static int test_callback_errors(void) {
    int ret_val;
    
    // Test with invalid session
    ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, NULL, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_EINVALIDPARAM);
    
    // Test with invalid event type
    ASSERT_NULL(procbuf_session_callback(999, NULL, &mock_session, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_EINVALIDPARAM);
    return 0;
}

// Test hash table limits
static int test_hash_table_limits(void) {
    uint32_t test_data = 1000;
    int test_domain, test_session = 0;
    int ret_val;
    for (int i = 0; i < 1000; i++) {
        test_domain = i;
        struct dsp mock_dsp = {
            .dsp_id = test_domain
        };
        
        struct session mock_session = {
            .dsp = &mock_dsp,
            .session_id = test_session
        };
        uint32_t test_data = i;
        ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, &mock_session, &ret_val));
        ASSERT_EQUAL(ret_val, AEE_SUCCESS);
        ASSERT_EQUAL(procbuf_add_param(&mock_session, 1, 
                    &test_data, sizeof(test_data)), AEE_SUCCESS);
    }
    return 0;
}

// Test parameter layout
static int test_param_layout(void) {
    uint32_t data1 = 0x12345678;
    uint32_t data2 = 0xABCDEF00;
    char data3[7] = "Testing";  // Unaligned size
    int ret_val;
    void *addr;
    int fd;
    uint32_t size;

    ASSERT_NULL(procbuf_session_callback(CALLBACK_TYPE_INIT, NULL, &mock_session, &ret_val));
    ASSERT_EQUAL(ret_val, AEE_SUCCESS);
    
    // Add parameters with different sizes
    ASSERT_EQUAL(procbuf_add_param(&mock_session, 1, &data1, sizeof(data1)), AEE_SUCCESS);
    ASSERT_EQUAL(procbuf_add_param(&mock_session, 2, &data2, sizeof(data2)), AEE_SUCCESS);
    ASSERT_EQUAL(procbuf_add_param(&mock_session, 3, data3, sizeof(data3)), AEE_SUCCESS);
    
    // Verify layout and alignment
    ASSERT_EQUAL(procbuf_get_info(&mock_session, &fd, &addr, &size), AEE_SUCCESS);
    
    // Check first parameter
    param_header_t *hdr1 = get_param_header(addr, 0);
    ASSERT_EQUAL(hdr1->id, 1);
    ASSERT_EQUAL(hdr1->size, sizeof(data1));
    ASSERT_EQUAL(*(uint32_t*)get_param_data(hdr1), data1);
    
    // Check second parameter
    param_header_t *hdr2 = get_param_header(addr, 1);
    ASSERT_EQUAL(hdr2->id, 2);
    ASSERT_EQUAL(hdr2->size, sizeof(data2));
    ASSERT_EQUAL(*(uint32_t*)get_param_data(hdr2), data2);
    
    // Check third parameter (unaligned)
    param_header_t *hdr3 = get_param_header(addr, 2);
    ASSERT_EQUAL(hdr3->id, 3);
    ASSERT_EQUAL(hdr3->size, sizeof(data3));
    ASSERT_EQUAL(memcmp(get_param_data(hdr3), data3, sizeof(data3)), 0);
    
    // Verify alignment
    uintptr_t addr1 = (uintptr_t)get_param_data(hdr1);
    uintptr_t addr2 = (uintptr_t)get_param_data(hdr2);
    uintptr_t addr3 = (uintptr_t)get_param_data(hdr3);
    
    ASSERT_EQUAL(addr1 % PARAM_ALIGN, 0);
    ASSERT_EQUAL(addr2 % PARAM_ALIGN, 0);
    ASSERT_EQUAL(addr3 % PARAM_ALIGN, 0);
    
    return 0;
}

int main(void) {
    printf("Process Buffer Tests\n");
    setup();

    fastrpc_core_init();
    rpcmem_init();
    RUN_TEST(test_add_param_basic);
    RUN_TEST(test_invalid_params);
    RUN_TEST(test_buffer_limits);
    RUN_TEST(test_multiple_params);
    RUN_TEST(test_concurrent_access);
    RUN_TEST(test_extended_domain);
    RUN_TEST(test_callback_errors);
    RUN_TEST(test_hash_table_limits);
    RUN_TEST(test_param_layout);
    rpcmem_deinit();

    teardown();
    return 0;
}