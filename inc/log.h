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
#define ENABLE_INF_LOGGING 1

// Define function-specific logging control
#define LOG_DSP_INIT 1
#define LOG_DSP_DEINIT 1
#define LOG_SESS_INIT 1
#define LOG_SESS_DEINIT 1
#define LOG_SESS_INVOKE 1
#define LOG_SESS_MAP 1
#define LOG_SESS_UNMAP 1
#define LOG_SESS_ADDMOD 1
#define LOG_SESS_RMMOD 1
#define LOG_SESS_CFG 1

// Define logging macros
#if ENABLE_ERR_LOGGING
    #define ERR(fmt, ...) fprintf(stderr, "ERROR [%s][%s:%d][Thread %lu] : " fmt "\n", __func__, __FILE__, __LINE__, pthread_self(), ##__VA_ARGS__);
    #define LOG_ERR ERR
#else
    #define ERR(fmt, ...)
    #define LOG_ERR ERR
#endif

#if ENABLE_WRN_LOGGING
    #define WRN(fmt, ...) fprintf(stdout, "WARNING [%s][%s:%d][Thread %lu] : " fmt "\n",  __func__, __FILE__, __LINE__, pthread_self(), ##__VA_ARGS__)
    #define LOG_WRN WRN
#else
    #define WRN(fmt, ...)
    #define LOG_WRN WRN
#endif

#if ENABLE_INF_LOGGING
    #define INF(fmt, ...) fprintf(stdout, "INFO [%s][%s:%d][Thread %lu] : " fmt "\n",  __func__, __FILE__, __LINE__, pthread_self(), ##__VA_ARGS__)
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

#if LOG_SESS_ADDMOD
    #define LOG_SESS_ADDMOD_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_ADDMOD_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_ADDMOD_INF(fmt, ...)
    #define LOG_SESS_ADDMOD_ERR(fmt, ...)
#endif

#if LOG_SESS_RMMOD
    #define LOG_SESS_RMMOD_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_RMMOD_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_RMMOD_INF(fmt, ...)
    #define LOG_SESS_RMMOD_ERR(fmt, ...)
#endif

#if LOG_SESS_CFG
    #define LOG_SESS_CFG_INF(fmt, ...) INF(fmt, ##__VA_ARGS__)
    #define LOG_SESS_CFG_ERR(fmt, ...) ERR(fmt, ##__VA_ARGS__)
#else
    #define LOG_SESS_CFG_INF(fmt, ...)
    #define LOG_SESS_CFG_ERR(fmt, ...)
#endif

#endif /* __LOG_H__ */