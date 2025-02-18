// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __LOG_H__
#define __LOG_H__

#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>

// Define global logging control
#define ENABLE_ERR_LOGGING 1
#define ENABLE_WRN_LOGGING 0
#define ENABLE_INF_LOGGING 0

// Define function-specific logging control
#define LOG_DSP_INIT 1
#define LOG_DSP_DEINIT 1
#define LOG_SESS_INIT 1
#define LOG_SESS_DEINIT 1
#define LOG_SESS_INVOKE 1
#define LOG_SESS_MAP 1
#define LOG_SESS_UNMAP 1
#define LOG_CONFIG_INIT 1
#define LOG_CONFIG_DEINIT 1
#define LOG_CONFIG_SET 1
#define LOG_CONFIG_GET 1

// Helper function to print backtrace
static void print_backtrace() {
    void *buffer[10];
    int nptrs = backtrace(buffer, 10);
    char **symbols = backtrace_symbols(buffer, nptrs);
    if (symbols) {
        fprintf(stderr, "Backtrace:\n");
        for (int i = 0; i < nptrs; i++) {
            fprintf(stderr, "%s\n", symbols[i]);
        }
        free(symbols);
    }
}

// Define logging macros
#if ENABLE_ERR_LOGGING
    #define ERR(fmt, ...) do { \
                                fprintf(stderr, "ERROR [Thread %lu] : " fmt "\n", pthread_self(), ##__VA_ARGS__); \
                            } while (0)
    #define LOG_ERR ERR
#else
    #define ERR(fmt, ...)
    #define LOG_ERR ERR
#endif

#if ENABLE_WRN_LOGGING
    #define WRN(fmt, ...) fprintf(stdout, "WARNING [Thread %lu] : " fmt "\n", pthread_self(), ##__VA_ARGS__)
    #define LOG_WRN WRN
#else
    #define WRN(fmt, ...)
    #define LOG_WRN WRN
#endif

#if ENABLE_INF_LOGGING
    #define INF(fmt, ...) fprintf(stdout, "INFO [Thread %lu] : " fmt "\n", pthread_self(), ##__VA_ARGS__)
    #define LOG_INF INF
#else
    #define INF(fmt, ...)
    #define LOG_INF INF
#endif

// Define function-specific logging macros
#if LOG_DSP_INIT
    #define LOG_DSP_INIT_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_DSP_INIT_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_DSP_INIT_INF(fmt, ...)
    #define LOG_DSP_INIT_ERR(fmt, ...)
#endif

#if LOG_DSP_DEINIT
    #define LOG_DSP_DEINIT_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_DSP_DEINIT_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_DSP_DEINIT_INF(fmt, ...)
    #define LOG_DSP_DEINIT_ERR(fmt, ...)
#endif

#if LOG_SESS_INIT
    #define LOG_SESS_INIT_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_INIT_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_INIT_INF(fmt, ...)
    #define LOG_SESS_INIT_ERR(fmt, ...)
#endif

#if LOG_SESS_DEINIT
    #define LOG_SESS_DEINIT_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_DEINIT_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_DEINIT_INF(fmt, ...)
    #define LOG_SESS_DEINIT_ERR(fmt, ...)
#endif

#if LOG_SESS_INVOKE
    #define LOG_SESS_INV_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_INV_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_INV_INF(fmt, ...)
    #define LOG_SESS_INV_ERR(fmt, ...)
#endif

#if LOG_SESS_MAP
    #define LOG_SESS_MAP_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_MAP_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_MAP_INF(fmt, ...)
    #define LOG_SESS_MAP_ERR(fmt, ...)
#endif

#if LOG_SESS_UNMAP
    #define LOG_SESS_UNMAP_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_UNMAP_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_UNMAP_INF(fmt, ...)
    #define LOG_SESS_UNMAP_ERR(fmt, ...)
#endif

// Define config-specific logging macros
#if LOG_CONFIG_INIT
    #define LOG_CFG_INIT_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_CFG_INIT_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_CFG_INIT_INF(fmt, ...)
    #define LOG_CFG_INIT_ERR(fmt, ...)
#endif

#if LOG_CONFIG_DEINIT
    #define LOG_CFG_DEINIT_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_CFG_DEINIT_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_CFG_DEINIT_INF(fmt, ...)
    #define LOG_CFG_DEINIT_ERR(fmt, ...)
#endif

#if LOG_CONFIG_SET
    #define LOG_CFG_SET_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_CFG_SET_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_CFG_SET_INF(fmt, ...)
    #define LOG_CFG_SET_ERR(fmt, ...)
#endif

#if LOG_CONFIG_GET
    #define LOG_CFG_GET_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_CFG_GET_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_CFG_GET_INF(fmt, ...)
    #define LOG_CFG_GET_ERR(fmt, ...)
#endif

#endif /* __LOG_H__ */