// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "test_framework.h"
#include "fastrpc.h"
#include <pthread.h>

static int callback_invoked = 0;
static int callback_invoked_invoke = 0;
static int callback_invoked_configure = 0;

void *test_callback(int type, void *ctx, void *dsp, int *err)
{
    callback_invoked++;
    *err = AEE_SUCCESS;
    return NULL;
}

void *test_session_callback(int type, void *ctx, void *sess, int *err)
{
    callback_invoked++;
    *err = AEE_SUCCESS;
    return NULL;
}

void *test_session_callback_invoke(int type, void *ctx, void *invoke, int *err)
{
    if (type == CALLBACK_TYPE_INVOKE) {
        callback_invoked_invoke++;
        *err = AEE_SUCCESS;
    }
    return NULL;
}

void *test_session_callback_configure(int type, void *ctx, void *data, int *err)
{
    if (type == CALLBACK_TYPE_CONFIGURE) {
        config_store_t *store = (config_store_t *)data;
        // Test getting a config value
        void *value = config_store_get(store, "test_key");
        if (value) {
            callback_invoked_configure++;
            *err = AEE_SUCCESS;
        } else {
            *err = AEE_EFAILED;
        }
    }
}

int test_dsp_init(void)
{
    struct dsp *dsp = dsp_init(1);
    ASSERT_NOT_NULL(dsp);
    ASSERT_EQUAL(dsp->dsp_id, 1);
    dsp_deinit(1);
    return 0;
}

int test_dsp_init_null(void)
{
    struct dsp *dsp = dsp_init(0);
    ASSERT_NOT_NULL(dsp);
    ASSERT_EQUAL(dsp->dsp_id, 0);
    dsp_deinit(0);
    return 0;
}

int test_dsp_deinit_invalid(void)
{
    int result = dsp_deinit(999); // Invalid DSP ID
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

int test_session_init(void)
{
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);
    struct session *sess = session_init(dsp, 1);
    ASSERT_NOT_NULL(sess);
    ASSERT_EQUAL(sess->session_id, 1);
    session_deinit(sess);
    dsp_deinit(1);
    return 0;
}

int test_session_init_null(void)
{
    struct session *sess = session_init(NULL, 1);
    ASSERT_NULL(sess);
    return 0;
}

int test_session_deinit_invalid(void)
{
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);
    struct session *sess = session_init(dsp, 1);
    int result = session_deinit(NULL); // Invalid session
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    session_deinit(sess);
    dsp_deinit(1);
    return 0;
}

int test_session_configure(void)
{
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);
    register_session_callback(dsp, CALLBACK_TYPE_CONFIGURE, test_session_callback_configure);

    void *value = malloc(sizeof(int));
    struct session *sess = session_init(dsp, 1);
    int result = session_configure(sess, "test_key", value);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    ASSERT_TRUE(callback_invoked_configure > 0);
    session_deinit(sess);
    dsp_deinit(1);
    return 0;
}

int test_session_configure_null(void)
{
    int result = session_configure(NULL, "test_key", NULL);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

int test_session_invoke(void)
{
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);
    register_session_callback(dsp, CALLBACK_TYPE_INVOKE, test_session_callback_invoke);

    struct session *sess = session_init(dsp, 1);
    remote_arg params[1] = {0}; // Example params
    int result = session_invoke(sess, 1, 0, params);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    ASSERT_TRUE(callback_invoked_invoke > 0);
    session_deinit(sess);
    dsp_deinit(1);
    return 0;
}

int test_session_invoke_null(void)
{
    remote_arg params[1] = {0}; // Example params
    int result = session_invoke(NULL, 1, 0, params);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

int test_session_invoke2(void)
{
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);
    register_session_callback(dsp, CALLBACK_TYPE_INVOKE, test_session_callback_invoke);

    struct session *sess = session_init(dsp, 1);
    remote_arg params[1] = {0}; // Example params with invalid fd
    params[0].buf.pv = 0; // Invalid pointer, should fail from kernel
    int result = session_invoke(sess, 1, REMOTE_SCALARS_MAKE(1, 1, 0), params);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    session_deinit(sess);
    dsp_deinit(1);
    return 0;
}

int test_callbacks(void)
{
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);

    struct session *sess = session_init(dsp, 1);
    session_deinit(sess);
    dsp_deinit(1);

    ASSERT_TRUE(callback_invoked > 0);
    return 0;
}

void *thread_func(void *arg)
{
    struct dsp *dsp = (struct dsp *)arg;
    struct session *sess = session_init(dsp, 1);
    LOG_INF("Session ID %d", sess->session_id);
    session_deinit(sess);
    LOG_INF("Session deinitialized");
    return NULL;
}

int test_multithreading(void)
{
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);

    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, thread_func, (void *)dsp);
    pthread_create(&thread2, NULL, thread_func, (void *)dsp);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    dsp_deinit(1);
    return 0;
}

void *thread_func_init_deinit(void *arg)
{
    int dsp_id = *(int *)arg;
    struct dsp *dsp = dsp_init(dsp_id);
    ASSERT_NOT_NULL(dsp);
    ASSERT_EQUAL(dsp->dsp_id, dsp_id);
    int result = dsp_deinit(dsp_id);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    return NULL;
}

int test_multithreading_init_deinit(void)
{
    pthread_t thread1, thread2;
    int dsp_id1 = 1, dsp_id2 = 2;

    pthread_create(&thread1, NULL, thread_func_init_deinit, &dsp_id1);
    pthread_create(&thread2, NULL, thread_func_init_deinit, &dsp_id2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    return 0;
}

void *thread_func_init_deinit_50(void *arg)
{
    int dsp_id = *(int *)arg;
    struct dsp *dsp = dsp_init(dsp_id);
    ASSERT_NOT_NULL(dsp);
    ASSERT_EQUAL(dsp->dsp_id, dsp_id);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);
    struct session *sess = session_init(dsp, 1);
    ASSERT_NOT_NULL(sess);
    ASSERT_EQUAL(sess->session_id, 1);
    int result = session_deinit(sess);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    result = dsp_deinit(dsp_id);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    return NULL;
}

int test_multithreading_init_deinit_50(void)
{
    pthread_t threads[50];
    int dsp_ids[50];
    for (int i = 0; i < 50; i++) {
        dsp_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, thread_func_init_deinit_50, &dsp_ids[i]);
    }

    for (int i = 0; i < 50; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}

int test_multiple_dsp_session_init_deinit(void)
{
    struct dsp *dsp1 = dsp_init(1);
    register_session_callback(dsp1, CALLBACK_TYPE_INIT, test_session_callback);
    struct dsp *dsp2 = dsp_init(2);
    register_session_callback(dsp2, CALLBACK_TYPE_INIT, test_session_callback);
    ASSERT_NOT_NULL(dsp1);
    ASSERT_NOT_NULL(dsp2);

    struct session *sess1 = session_init(dsp1, 1);
    struct session *sess2 = session_init(dsp2, 2);
    ASSERT_NOT_NULL(sess1);
    ASSERT_NOT_NULL(sess2);

    session_deinit(sess1);
    session_deinit(sess2);
    dsp_deinit(1);
    dsp_deinit(2);
    return 0;
}

void *thread_func_multiple_dsp_session(void *arg)
{
    int dsp_id = *(int *)arg;
    struct dsp *dsp = dsp_init(dsp_id);
    ASSERT_NOT_NULL(dsp);
    ASSERT_EQUAL(dsp->dsp_id, dsp_id);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);

    struct session *sess = session_init(dsp, 1);
    ASSERT_NOT_NULL(sess);
    ASSERT_EQUAL(sess->session_id, 1);

    session_deinit(sess);
    dsp_deinit(dsp_id);
    return NULL;
}

int test_multithreading_multiple_dsp_session(void)
{
    pthread_t threads[10];
    int dsp_ids[10];
    for (int i = 0; i < 10; i++) {
        dsp_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, thread_func_multiple_dsp_session, &dsp_ids[i]);
    }

    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }
    return 0;
}

static int test_session_config(void) {
    struct dsp *dsp = dsp_init(1);
    ASSERT_NOT_NULL(dsp);

    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_session_callback);
    struct session *sess = session_init(dsp, 1);
    ASSERT_NOT_NULL(sess);

    // Test configuration
    int *value = malloc(sizeof(int));
    *value = 42;
    ASSERT_EQUAL(session_configure(sess, "test_key", value), AEE_SUCCESS);

    // Verify config value
    void *retrieved = config_store_get(sess->config, "test_key");
    ASSERT_NOT_NULL(retrieved);
    ASSERT_EQUAL(*(int*)retrieved, 42);

    // Test invalid config
    ASSERT_NOT_EQUAL(session_configure(sess, NULL, value), AEE_SUCCESS);
    ASSERT_NOT_EQUAL(session_configure(sess, "test_key", NULL), AEE_SUCCESS);

    // Cleanup
    //value cleaned up in config_store_destroy
    ASSERT_EQUAL(session_deinit(sess), AEE_SUCCESS);
    ASSERT_EQUAL(dsp_deinit(1), AEE_SUCCESS);
    
    return 0;
}

int main()
{
    fastrpc_init();
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);

    RUN_TEST(test_dsp_init);
    RUN_TEST(test_dsp_init_null);
    RUN_TEST(test_session_init);
    RUN_TEST(test_session_init_null);
    RUN_TEST(test_session_configure);
    RUN_TEST(test_session_configure_null);
    RUN_TEST(test_session_invoke);
    RUN_TEST(test_session_invoke_null);
    RUN_TEST(test_session_invoke2);
    RUN_TEST(test_callbacks);
    RUN_TEST(test_multithreading);
    RUN_TEST(test_multithreading_init_deinit);
    RUN_TEST(test_dsp_deinit_invalid);
    RUN_TEST(test_session_deinit_invalid);
    RUN_TEST(test_multithreading_init_deinit_50);
    RUN_TEST(test_multiple_dsp_session_init_deinit);
    RUN_TEST(test_multithreading_multiple_dsp_session);
    RUN_TEST(test_session_config);

    printf("All tests passed.\n");
    return 0;
}