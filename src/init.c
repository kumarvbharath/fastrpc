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
#include "init.h"


/* Initialization control */
static pthread_once_t fastrpc_core_init_once = PTHREAD_ONCE_INIT;
static pthread_once_t fastrpc_other_init_once = PTHREAD_ONCE_INIT;
static int g_initialized = 0;

void fastrpc_default_configurations() {
    int attr = 0x0, flags = 0x4;
    // Set default configurations here
    struct remote_rpc_pd_initmem_size initmem = {-1, 0x400000};
    struct remote_process_type pdtype = {-1, 1};
    struct remote_rpc_control_pd_dump pddump = {-1, 1}; 
    struct remote_rpc_thread_params thread_params = {-1, 0xC0, 0x4000};

    global_configure("fastrpc.pd.type", &pdtype, sizeof(struct remote_process_type));
    global_configure("fastrpc.pd.dump", &pddump, sizeof(struct remote_rpc_control_pd_dump));
    global_configure("fastrpc.pd.attr", &attr, sizeof(int));
    global_configure("fastrpc.pd.initmem.size", &initmem, sizeof(struct remote_rpc_pd_initmem_size));
    global_configure("fastrpc.thread.priority", &thread_params, sizeof(struct remote_rpc_thread_params));
    global_configure("fastrpc.thread.stacksize", &thread_params, sizeof(struct remote_rpc_thread_params));
    global_configure("fastrpc.init.flags", &flags, sizeof(flags));
    LOG_INF("store default configurations : flags %p, attr %p", &flags, &attr);
}

/**
 * @brief Initialize other FastRPC components
 *
 * Registers callbacks for memory management and remote operations
 */
void fastrpc_other_init(void) {
    LOG_INF("Initializing FastRPC components");

    rpcmem_init();
    fastrpc_default_configurations();

    register_dsp_callback(CALLBACK_TYPE_INIT | CALLBACK_TYPE_DEINIT, rpcmem_dsp_callback);
    register_dsp_callback(CALLBACK_TYPE_INIT | CALLBACK_TYPE_DEINIT, remote_dsp_callback);
    register_dsp_callback(CALLBACK_TYPE_INIT | CALLBACK_TYPE_DEINIT, remotectl_dsp_callback);
    
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
        LOG_INF("FastRPC already initialized");
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
        LOG_INF("FastRPC not initialized");
        return AEE_SUCCESS;
    }

    // Unregister callbacks in reverse order
    unregister_dsp_callback(CALLBACK_TYPE_DEINIT | CALLBACK_TYPE_INIT, remotectl_dsp_callback);
    unregister_dsp_callback(CALLBACK_TYPE_DEINIT | CALLBACK_TYPE_INIT, remote_dsp_callback);
    unregister_dsp_callback(CALLBACK_TYPE_DEINIT | CALLBACK_TYPE_INIT, rpcmem_dsp_callback);

    global_configure("fastrpc.pd.type", NULL, 0);
    global_configure("fastrpc.pd.dump", NULL, 0);
    global_configure("fastrpc.pd.attr", NULL, 0);
    global_configure("fastrpc.pd.initmem.size", NULL, 0);
    global_configure("fastrpc.thread.priority", NULL, 0);
    global_configure("fastrpc.thread.stacksize", NULL, 0);

    rpcmem_deinit();
    g_initialized = 0;
    LOG_INF("FastRPC deinitialized successfully");
    return AEE_SUCCESS;
}
