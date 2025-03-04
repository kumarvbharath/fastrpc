#include "test_framework.h"
#include "fastrpc.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static int callback_invoked = 0;
static int test_callback_session_ref = 0;

int test_callback_session(int type, void **context, void *data, void **ret) {
    (void)context;
    (void)data;
    (void)ret;
    if(type == CALLBACK_TYPE_INIT) {
        test_callback_session_ref++;
    } else if(type == CALLBACK_TYPE_DEINIT) {
        test_callback_session_ref--;
    }
    return AEE_SUCCESS;
}

int test_callback(int type, void **context, void *data, void **ret) {
    (void)context;
    (void)ret;
    callback_invoked++;
    if(type == CALLBACK_TYPE_MAP) {
        struct rpcmem *mem = (struct rpcmem*)data;
        mem->ptr = (void*)1;
    }
    return AEE_SUCCESS;
}

int test_callback_fail(int type, void **context, void *data, void **ret) {
    (void)context;
    (void)type;
    (void)data;
    (void)ret;
    return AEE_EFAILED;
}

void *thread_func_session_invoke(void *arg) {
    struct session *sess = (struct session *)arg;
    remote_handle64 handle = session_add_module(sess, "test_module");
    remote_arg params[1] = {0};
    int result = session_invoke(sess, handle, REMOTE_SCALARS_MAKE(1, 1, 0), params);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    session_remove_module(sess, handle);
    return NULL;
}

void *thread_func_session_map_memory(void *arg) {
    struct session *sess = (struct session *)arg;
    struct rpcmem mem = {0};
    void *ptr = session_map_memory(sess, &mem);
    ASSERT_NOT_NULL(ptr);
    session_unmap_memory(sess, &mem);
    return NULL;
}

int test_dsp_nocallback() {
    struct dsp *dsp = dsp_init(1);
    ASSERT_NULL(dsp);
    return 0;
}

int test_dsp_invalid() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(-1);
    ASSERT_NULL(dsp);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_dsp_init_secondtime() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    dsp = dsp_init(1);
    ASSERT_NOT_NULL(dsp);
    dsp_deinit(1);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_dsp_deinit_secondtime() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    ASSERT_NOT_NULL(dsp);
    dsp_deinit(1);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_dsp_deinit_with_active_session() {
    register_dsp_callback(CALLBACK_TYPE_INIT | CALLBACK_TYPE_DEINIT, test_callback_session);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback);
    struct session *sess = session_init(dsp, 1);
    ASSERT_NOT_NULL(sess);
    get_session(dsp, 1);
    session_deinit(sess);
    ASSERT_EQUAL(test_callback_session_ref, 1);
    put_session(sess);
    session_deinit(sess);
    dsp_deinit(1);
    ASSERT_EQUAL(test_callback_session_ref, 0);
    unregister_dsp_callback(CALLBACK_TYPE_INIT | CALLBACK_TYPE_DEINIT, test_callback_session);
    return 0;
}

int test_dsp_init_callback_fail() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback_fail);
    struct dsp *dsp = dsp_init(1);
    ASSERT_NULL(dsp);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback_fail);
    return 0;
}

int test_session_invalid() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback);
    struct session *sess = session_init(dsp, -1);
    ASSERT_NULL(sess);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_session_init_secondtime() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback);
    struct session *sess = session_init(dsp, 1);
    sess = session_init(dsp, 1);
    ASSERT_NOT_NULL(sess);
    session_deinit(sess);
    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_session_deinit_with_reference() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_DEINIT, test_callback_session);
    struct session *sess = session_init(dsp, 1);
    get_session(dsp, 1);
    ASSERT_NOT_NULL(sess);
    session_deinit(sess);
    ASSERT_EQUAL(test_callback_session_ref, 1);
    put_session(sess);
    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_DEINIT, test_callback_session);
    ASSERT_EQUAL(test_callback_session_ref, 0);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_session_deinit_secondtime() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback);
    struct session *sess = session_init(dsp, 1);
    ASSERT_NOT_NULL(sess);
    session_deinit(sess);
    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_session_init_callback_fail() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback_fail);
    struct session *sess = session_init(dsp, 1);
    ASSERT_NULL(sess);
    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT, test_callback_fail);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_register_unregister_dsp_callback() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_register_unregister_session_callback() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_get_session_from_handle() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    struct session *sess = session_init(dsp, 1);

    remote_handle64 handle = session_add_module(sess, "test_module");
    remote_handle64 handle1 = session_add_module(sess, "test_module1");
    remote_handle64 handle2 = session_add_module(sess, "test_module2");
    remote_handle64 handle3 = session_add_module(sess, "test_module3");
    remote_handle64 handle4 = session_add_module(sess, "test_module4");
    struct session *retrieved_sess = get_session_from_handle(handle);
    ASSERT_NOT_NULL(retrieved_sess);
    ASSERT_EQUAL(retrieved_sess->session_id, sess->session_id);

    session_remove_module(sess, handle);
    session_remove_module(sess, handle1);
    session_remove_module(sess, handle2);
    session_remove_module(sess, handle3);
    session_remove_module(sess, handle4);
    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_get_session_from_handle_invalid() {
    struct session *sess = get_session_from_handle(INVALID_HANDLE);
    ASSERT_NULL(sess);
    return 0;
}

int test_session_map_memory() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    struct session *sess = session_init(dsp, 1);

    struct rpcmem mem = {0};
    void *ptr = session_map_memory(sess, &mem);
    ASSERT_NOT_NULL(ptr);

    session_unmap_memory(sess, &mem);
    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_session_map_memory_invalid() {
    struct session *sess = NULL;
    struct rpcmem mem = {0};
    void *ptr = session_map_memory(sess, &mem);
    ASSERT_NULL(ptr);
    return 0;
}

int test_session_unmap_memory_invalid() {
    struct session *sess = NULL;
    struct rpcmem mem = {0};
    int result = session_unmap_memory(sess, &mem);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

int test_session_invoke_invalid_handle() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    struct session *sess = session_init(dsp, 1);

    remote_arg params[1] = {0};
    int result = session_invoke(sess, INVALID_HANDLE, REMOTE_SCALARS_MAKE(1, 1, 0), params);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);

    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_session_invoke_invalid_params() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    struct session *sess = session_init(dsp, 1);

    remote_handle64 handle = session_add_module(sess, "test_module");
    int result = session_invoke(sess, handle, REMOTE_SCALARS_MAKE(1, 1, 0), NULL);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);

    session_remove_module(sess, handle);
    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_session_add_module_invalid() {
    struct session *sess = NULL;
    remote_handle64 handle = session_add_module(sess, "test_module");
    ASSERT_EQUAL(handle, INVALID_HANDLE);
    return 0;
}

int test_session_remove_module_invalid() {
    struct session *sess = NULL;
    int result = session_remove_module(sess, INVALID_HANDLE);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

int test_multithreading_session_invoke() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    struct session *sess = session_init(dsp, 1);

    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, thread_func_session_invoke, (void *)sess);
    pthread_create(&thread2, NULL, thread_func_session_invoke, (void *)sess);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int test_multithreading_session_map_memory() {
    register_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    struct dsp *dsp = dsp_init(1);
    register_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    struct session *sess = session_init(dsp, 1);

    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, thread_func_session_map_memory, (void *)sess);
    pthread_create(&thread2, NULL, thread_func_session_map_memory, (void *)sess);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    session_deinit(sess);
    unregister_session_callback(dsp, CALLBACK_TYPE_INIT | CALLBACK_TYPE_MAP | CALLBACK_TYPE_UNMAP | CALLBACK_TYPE_CONFIGURE, test_callback);
    dsp_deinit(1);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, test_callback);
    return 0;
}

int main() {
    fastrpc_core_init();
    RUN_TEST(test_register_unregister_dsp_callback);
    RUN_TEST(test_register_unregister_session_callback);
    RUN_TEST(test_get_session_from_handle);
    RUN_TEST(test_get_session_from_handle_invalid);
    RUN_TEST(test_dsp_invalid);
    RUN_TEST(test_dsp_init_secondtime);
    RUN_TEST(test_dsp_deinit_secondtime);
    RUN_TEST(test_dsp_init_callback_fail);
    RUN_TEST(test_dsp_nocallback);
    RUN_TEST(test_dsp_deinit_with_active_session);
    RUN_TEST(test_session_invalid);
    RUN_TEST(test_session_init_secondtime);
    RUN_TEST(test_session_deinit_secondtime);
    RUN_TEST(test_session_deinit_with_reference);
    RUN_TEST(test_session_init_callback_fail);
    RUN_TEST(test_session_map_memory);
    RUN_TEST(test_session_map_memory_invalid);
    RUN_TEST(test_session_unmap_memory_invalid);
    RUN_TEST(test_session_invoke_invalid_handle);
    RUN_TEST(test_session_invoke_invalid_params);
    RUN_TEST(test_session_add_module_invalid);
    RUN_TEST(test_session_remove_module_invalid);
    RUN_TEST(test_multithreading_session_invoke);
    RUN_TEST(test_multithreading_session_map_memory);

    return 0;
}