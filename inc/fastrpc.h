// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef FASTRPC_H
#define FASTRPC_H

#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "AEEQList.h"
#include "AEEstd.h"
#include "error.h"
#include "log.h"
#include "rpcmem.h"
#include "config.h"
#include "remote.h"
#include "uthash.h"

/**
 * FastRPC Callback Types 
 * These callbacks are triggered at different stages of the FastRPC session lifecycle
 */
 enum {
    /** Called during session initialization to setup session resources */
    CALLBACK_TYPE_INIT,
    /** Called during session deinitialization for cleanup */
    CALLBACK_TYPE_DEINIT,
    /** Called when first module is opened in a session */
    CALLBACK_TYPE_OPEN,
    /** Called when last module is closed in a session */
    CALLBACK_TYPE_CLOSE,
    /** Called for each remote_handle_open operation */
    CALLBACK_TYPE_LOAD,
    /** Called for each remote_handle_close operation */
    CALLBACK_TYPE_UNLOAD,
    /** Called when remote_session_control/config modifies session parameters */
    CALLBACK_TYPE_CONFIGURE,
    /** Called for each remote_handle_invoke RPC call */
    CALLBACK_TYPE_INVOKE,
    /** Called when fastrpc_mmap maps memory for RPC use */
    CALLBACK_TYPE_MAP,
    /** Called when fastrpc_munmap unmaps previously mapped memory */
    CALLBACK_TYPE_UNMAP,
};

typedef void* (*fastrpc_callback_t)(int type, void *ctx, void *data, int *retVal);

struct dsp_callback_node {
    int type;
    void *context;
    fastrpc_callback_t callback;
    QNode n;
};

struct session_callback_node {
    int type;
    void *context;
    fastrpc_callback_t callback;
    QNode n;
};

struct module {
    remote_handle64 handle;
    char name[32];
    struct session *sess;
    UT_hash_handle hh;
};

struct dsp;

struct session {
    int session_id;
    int active_users;
    struct dsp *dsp;
    config_store_t *config;
    pthread_spinlock_t lock;
    QList modules;
    QList maps;
    QNode n;
};

struct dsp {
    int dsp_id;
    int active_dsp; // Add this line
    QList sessions;
    QList session_callbacks;
    config_store_t *config;
    pthread_mutex_t dsp_lock;
    QNode n;
};

struct invoke_params {
    remote_handle64 handle;
    uint32_t sc;
    remote_arg64 *args;
};

/**
 * @brief Get a session object from a module handle
 * @param handle Module handle obtained from session_add_module
 * @return Session pointer if found, NULL otherwise
 * @thread_safety Thread-safe using handle_lock spinlock
 */
 struct session *get_session_from_handle(remote_handle64 handle);

 /**
  * @brief Register a callback for DSP-wide events
  * @param type Callback type from CALLBACK_TYPE_* enum
  * @param callback Function pointer to handle the callback
  * @thread_safety Thread-safe using dsp_callbacks_lock mutex
  * @note Does not validate if callback is already registered
  */
 void register_dsp_callback(int type, fastrpc_callback_t callback);
 
 /**
  * @brief Register a callback for session-specific events on a DSP
  * @param dsp DSP handle obtained from dsp_init
  * @param type Callback type from CALLBACK_TYPE_* enum
  * @param callback Function pointer to handle the callback
  * @thread_safety Thread-safe using dsp->dsp_lock mutex
  * @note Does not validate if callback is already registered
  */
 void register_session_callback(struct dsp *dsp, int type, fastrpc_callback_t callback);
 
 /**
  * @brief Initialize a DSP instance
  * @param dsp_id Unique identifier for the DSP
  * @return DSP handle on success, NULL on failure
  * @retval NULL if memory allocation fails or DSP callbacks fail
  * @thread_safety Thread-safe using dsp_list_lock mutex
  * @note Multiple calls with same dsp_id return same handle with increased reference count
  */
 struct dsp *dsp_init(int dsp_id);
 
 /**
  * @brief Deinitialize a DSP instance
  * @param dsp_id Identifier of DSP to deinitialize
  * @return AEE_SUCCESS on success, error code otherwise
  * @retval AEE_EACTIVESESSIONS if DSP has active sessions
  * @retval AEE_EINVALIDPARAM if DSP not found
  * @thread_safety Thread-safe using dsp_list_lock and dsp->dsp_lock mutexes
  * @note Only deinitialized when reference count reaches zero
  */
 int dsp_deinit(int dsp_id);
 
 /**
  * @brief Initialize a new session on a DSP
  * @param dsp DSP handle obtained from dsp_init
  * @param session_id Unique identifier for the session
  * @return Session handle on success, NULL on failure
  * @retval NULL if memory allocation fails or session callbacks fail
  * @thread_safety Thread-safe using dsp->dsp_lock mutex
  * @note Multiple calls with same session_id return same handle with increased reference count
  */
 struct session *session_init(struct dsp *dsp, int session_id);
 
 /**
  * @brief Deinitialize a session
  * @param sess Session handle to deinitialize
  * @return AEE_SUCCESS on success, error code otherwise
  * @retval AEE_EACTIVESESSIONS if session has active users
  * @retval AEE_EINVALIDPARAM if session not found
  * @thread_safety Thread-safe using session->lock spinlock and dsp->dsp_lock mutex
  */
 int session_deinit(struct session *sess);
 
 /**
  * @brief Get a reference to an existing session
  * @param dsp DSP handle containing the session
  * @param session_id Session identifier
  * @return Session handle if found, NULL otherwise
  * @thread_safety Thread-safe using dsp->dsp_lock mutex and session->lock spinlock
  * @note Increases session reference count
  */
 struct session *get_session(struct dsp *dsp, int session_id);
 
 /**
  * @brief Release a session reference
  * @param sess Session handle to release
  * @thread_safety Thread-safe using session->lock spinlock
  * @note Decrements session reference count
  */
 void put_session(struct session *sess);
 
 /**
  * @brief Add a module to a session
  * @param sess Session handle
  * @param name Module name (max 31 chars)
  * @return Module handle on success, 0 on failure
  * @thread_safety Thread-safe using session->lock spinlock and handle_lock spinlock
  * @note Increases session reference count
  */
 uintptr_t session_add_module(struct session *sess, const char *name);
 
 /**
  * @brief Remove a module from a session
  * @param sess Session handle
  * @param handle Module handle to remove
  * @thread_safety Thread-safe using session->lock spinlock and handle_lock spinlock
  * @note Decrements session reference count
  */
 void session_remove_module(struct session *sess, remote_handle64 handle);
 
 /**
  * @brief Configure session parameters
  * @param sess Session handle
  * @param config_key Configuration key
  * @param config_value Configuration value
  * @return AEE_SUCCESS on success, error code otherwise
  * @retval AEE_EINVALIDPARAM if session is invalid
  * @thread_safety Thread-safe through session validation
  */
 int session_configure(struct session *sess, char* config_key, void *config_value);
 
 /**
  * @brief Invoke a remote procedure call on a session
  * @param sess Session handle
  * @param handle Remote handle for the call
  * @param sc Scalar describing the RPC
  * @param params RPC parameters
  * @return AEE_SUCCESS on success, error code otherwise
  * @retval AEE_EINVALIDPARAM if session or params are invalid
  * @thread_safety Thread-safe through session validation
  * @note All memory referenced in params must be pre-mapped using session_map_memory
  */
 int session_invoke(struct session *sess, remote_handle64 handle, uint32_t sc, remote_arg64 *params);
 
 /**
  * @brief Map memory for use with RPC calls
  * @param sess Session handle
  * @param mem Memory descriptor to map
  * @return Mapped memory pointer on success, NULL on failure
  * @thread_safety Thread-safe using session->lock spinlock
  * @note Increases session reference count
  */
 void *session_map_memory(struct session *sess, struct rpcmem *mem);
 
 /**
  * @brief Unmap previously mapped memory
  * @param sess Session handle
  * @param mem Memory descriptor to unmap
  * @thread_safety Thread-safe using session->lock spinlock
  * @note Decrements session reference count
  */
 void session_unmap_memory(struct session *sess, struct rpcmem *mem);

#endif // FASTRPC_H