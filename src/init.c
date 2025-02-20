// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file init.c
 * @brief FastRPC Initialization and Cleanup Implementation
 *
 * Provides:
 * - One-time initialization of core FastRPC components
 * - Registration of DSP callbacks
 * - Memory management initialization
 * - Error handling and logging
 *
 * Thread Safety:
 * - Uses pthread_once for thread-safe initialization
 * - Safe for concurrent access from multiple threads
 *
 */

#include "fastrpc.h"
#include "error.h"
#include "log.h"
#include "remote_internal.h"
#include "rpcmem.h"

/* Initialization control */
static pthread_once_t fastrpc_core_init_once = PTHREAD_ONCE_INIT;
static pthread_once_t fastrpc_other_init_once = PTHREAD_ONCE_INIT;
static int g_initialized = 0;

/**
 * @brief Initialize other FastRPC components
 *
 * Registers callbacks for memory management and remote operations
 */
void fastrpc_other_init(void) {
    LOG_INF("Initializing FastRPC components");

    register_dsp_callback(CALLBACK_TYPE_INIT, rpcmem_dsp_callback);
    register_dsp_callback(CALLBACK_TYPE_DEINIT, rpcmem_dsp_callback);
    register_dsp_callback(CALLBACK_TYPE_INIT, remote_dsp_callback);
    register_dsp_callback(CALLBACK_TYPE_DEINIT, remote_dsp_callback);

    LOG_INF("FastRPC components initialized successfully");
}

/**
 * @brief Initialize FastRPC subsystem
 * @return AEE_SUCCESS on success, error code otherwise
 */
int fastrpc_init(void) {
    int ret;   
    LOG_INF("Initializing FastRPC");

    if (g_initialized) {
        LOG_WARN("FastRPC already initialized");
        return AEE_SUCCESS;
    }

    ret = pthread_once(&fastrpc_core_init_once, fastrpc_core_init);
    if (ret != 0) {
        LOG_ERR("Core initialization failed: %s", strerror(ret));
        return AEE_EFAILED;
    }

    ret = pthread_once(&fastrpc_other_init_once, fastrpc_other_init);
    if (ret != 0) {
        LOG_ERR("Component initialization failed: %s", strerror(ret));
        return AEE_EFAILED;
    }

    g_initialized = 1;
    LOG_INF("FastRPC initialized successfully");
    return AEE_SUCCESS;
}

/**
 * @brief Cleanup FastRPC subsystem
 * @return AEE_SUCCESS on success, error code otherwise
 */
int fastrpc_deinit(void) {
    LOG_INF("Deinitializing FastRPC");

    if (!g_initialized) {
        LOG_WARN("FastRPC not initialized");
        return AEE_SUCCESS;
    }

    // Unregister callbacks in reverse order
    unregister_dsp_callback(CALLBACK_TYPE_DEINIT, remote_dsp_callback);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, remote_dsp_callback);
    unregister_dsp_callback(CALLBACK_TYPE_DEINIT, rpcmem_dsp_callback);
    unregister_dsp_callback(CALLBACK_TYPE_INIT, rpcmem_dsp_callback);

    g_initialized = 0;
    LOG_INF("FastRPC deinitialized successfully");
    return AEE_SUCCESS;
}