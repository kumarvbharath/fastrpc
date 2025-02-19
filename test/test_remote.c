// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include "test_framework.h"
#include "fastrpc.h"
#include "remote.h"
#include "error.h"

static int callback_invoked = 0;

void *test_callback(int type, void *ctx, void *handle, int *err)
{
    callback_invoked++;
    *err = AEE_SUCCESS;
    return NULL;
}

int test_remote_handle_open(void)
{
    remote_handle64 handle;
    int result = remote_handle64_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    ASSERT_NOT_NULL(handle);
    remote_handle64_close(handle);
    return 0;
}

int test_remote_handle_open_invalid(void)
{
    remote_handle64 handle;
    int result = remote_handle64_open("invalid_uri", &handle);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

int test_remote_handle_invoke(void)
{
    remote_handle64 handle;
    int result = remote_handle64_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    ASSERT_NOT_NULL(handle);

    int fds[2];
    void *addrs[2];
    for (int i = 0; i < 2; i++) {
        fds[i] = open("/dev/null", O_RDWR);
        ASSERT_NOT_EQUAL(fds[i], -1);
        addrs[0] = 0x80000000;
        addrs[1] = 0x80002000;
        result = fastrpc_mmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 0, 4096, FASTRPC_MAP_STATIC);
        ASSERT_EQUAL(result, AEE_SUCCESS);
    }

    remote_arg args[2] = {0};
    for (int i = 0; i < 2; i++) {
        args[i].buf.pv = addrs[i];
        args[i].buf.nLen = 4096;
    }

    result = remote_handle64_invoke(handle, REMOTE_SCALARS_MAKE(2, 2, 0), args);
    ASSERT_EQUAL(result, AEE_SUCCESS);

    for (int i = 0; i < 2; i++) {
        result = fastrpc_munmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 4096);
        ASSERT_EQUAL(result, AEE_SUCCESS);
        close(fds[i]);
    }
    remote_handle64_close(handle);
    return 0;
}

int test_remote_handle_invoke_invalid(void)
{
    remote_arg args[1] = {0};
    int result = remote_handle64_invoke(0, REMOTE_SCALARS_MAKE(1, 1, 0), args);
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

int test_remote_handle_close_invalid(void)
{
    int result = remote_handle64_close(0); // Invalid handle
    ASSERT_EQUAL(result, AEE_EINVALIDPARAM);
    return 0;
}

void *thread_func_remote(void *arg)
{
    remote_handle64 handle;
    int result = remote_handle64_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    ASSERT_NOT_NULL(handle);

    int fds[2];
    void *addrs[2];
    for (int i = 0; i < 2; i++) {
        fds[i] = open("/dev/null", O_RDWR);
        ASSERT_NOT_EQUAL(fds[i], -1);
        addrs[0] = 0x80000000;
        addrs[1] = 0x80002000;
        result = fastrpc_mmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 0, 4096, FASTRPC_MAP_STATIC);
        ASSERT_EQUAL(result, AEE_SUCCESS);
    }

    remote_arg args[2] = {0};
    for (int i = 0; i < 2; i++) {
        args[i].buf.pv = addrs[i];
        args[i].buf.nLen = 4096;
    }

    result = remote_handle64_invoke(handle, REMOTE_SCALARS_MAKE(2, 2, 0), args);
    ASSERT_EQUAL(result, AEE_SUCCESS);

    for (int i = 0; i < 2; i++) {
        result = fastrpc_munmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 4096);
        ASSERT_EQUAL(result, AEE_SUCCESS);
        close(fds[i]);
    }
    remote_handle64_close(handle);
    return NULL;
}

int test_multithreading_remote(void)
{
    pthread_t thread1, thread2;

    pthread_create(&thread1, NULL, thread_func_remote, NULL);
    pthread_create(&thread2, NULL, thread_func_remote, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}

void *thread_func_concurrent(void *arg)
{
    remote_handle64 handle;
    int result = remote_handle64_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    ASSERT_NOT_NULL(handle);

    int fds[2];
    void *addrs[2];
    for (int i = 0; i < 2; i++) {
        fds[i] = open("/dev/null", O_RDWR);
        ASSERT_NOT_EQUAL(fds[i], -1);
        addrs[0] = 0x80000000;
        addrs[1] = 0x80002000;
        result = fastrpc_mmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 0, 4096, FASTRPC_MAP_STATIC);
        ASSERT_EQUAL(result, AEE_SUCCESS);
    }

    remote_arg args[2] = {0};
    for (int i = 0; i < 2; i++) {
        args[i].buf.pv = addrs[i];
        args[i].buf.nLen = 4096;
    }

    for (int i = 0; i < 10; i++) {
        result = remote_handle64_invoke(handle, REMOTE_SCALARS_MAKE(2, 2, 0), args);
        ASSERT_EQUAL(result, AEE_SUCCESS);
    }

    for (int i = 0; i < 2; i++) {
        result = fastrpc_munmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 4096);
        ASSERT_EQUAL(result, AEE_SUCCESS);
        close(fds[i]);
    }
    remote_handle64_close(handle);
    return NULL;
}

int test_concurrency_remote(void)
{
    pthread_t threads[10];

    for (int i = 0; i < 10; i++) {
        pthread_create(&threads[i], NULL, thread_func_concurrent, NULL);
    }

    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

int test_remote_handle_invoke_many_args(void)
{
    remote_handle64 handle;
    int result = remote_handle64_open("file:///test_skel.so?test_skel_handle_invoke&_modver=1.0&_dom=adsp&_session=1", &handle);
    ASSERT_EQUAL(result, AEE_SUCCESS);
    ASSERT_NOT_NULL(handle);

    int fds[100];
    void *addrs[100];
    for (int i = 0; i < 100; i++) {
        fds[i] = open("/dev/null", O_RDWR);
        ASSERT_NOT_EQUAL(fds[i], -1);
        addrs[i] = (0x80000000 + (i * 0x1000));
        result = fastrpc_mmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 0, 4096, FASTRPC_MAP_STATIC);
        ASSERT_EQUAL(result, AEE_SUCCESS);
    }

    remote_arg args[100] = {0};
    for (int i = 0; i < 100; i++) {
        args[i].buf.pv = fds[i];
        args[i].buf.nLen = 4096;
    }

    result = remote_handle64_invoke(handle, REMOTE_SCALARS_MAKE(100, 100, 0), args);
    ASSERT_EQUAL(result, AEE_SUCCESS);

    for (int i = 0; i < 100; i++) {
        result = fastrpc_munmap(MAKE_EXTENDED_DOMAIN_ID(ADSP_DOMAIN_ID, 1), fds[i], addrs[i], 4096);
        ASSERT_EQUAL(result, AEE_SUCCESS);
        close(fds[i]);
    }
    remote_handle64_close(handle);
    return 0;
}

int main()
{
    fastrpc_init();
    RUN_TEST(test_remote_handle_open);
    RUN_TEST(test_remote_handle_open_invalid);
    RUN_TEST(test_remote_handle_invoke);
    RUN_TEST(test_remote_handle_invoke_invalid);
    RUN_TEST(test_remote_handle_close_invalid);
    RUN_TEST(test_multithreading_remote);
    RUN_TEST(test_concurrency_remote);
    RUN_TEST(test_remote_handle_invoke_many_args);

    printf("All remote tests passed.\n");
    return 0;
}