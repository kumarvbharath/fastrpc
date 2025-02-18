// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "fastrpc.h"

/**
 * @file fastrpc.c
 * @brief FastRPC DSP and Session Management
 *
 * This file implements the FastRPC framework, which facilitates the management
 * of DSP (Digital Signal Processor) sessions and callbacks. It includes functions
 * and structures for DSP initialization, session management, and callback registration.
 *
 * @details
 * The framework ensures thread safety using mutexes and spin locks, providing a
 * structured approach to managing DSPs and their sessions, along with callback
 * mechanisms for various operations.
 *
 * @section DSP and Session Overview
 * - The DSP data structure represents a logical abstraction of a physical DSP
 *   available in the System on Chip (SoC) where FastRPC is enabled. This allows
 *   clients to offload their algorithms to the DSP, opening a handle on any DSP
 *   and managing their algorithms efficiently.
 * - The DSP data structure indicates which DSP the interface/module is loaded
 *   and managed on.
 * - The session data structure represents a protection domain (PD) created on the DSP,
 *   akin to a process on a High-Level Operating System (HLOS). This file enables FastRPC
 *   to maintain the DSPs supported for an HLOS process and the multiple PDs created on any DSP.
 * - Clients can leverage the FastRPC framework to offload intensive processing tasks to the DSP,
 *   utilizing its internal resources, such as HVX, for more efficient computation and power usage
 *   compared to the CPU.
 *
 * @note
 * This file is part of the FastRPC framework and is intended for managing DSP sessions
 * and callbacks in a thread-safe manner.
 */

/* Global DSP List */
static QList dsp_list;
static pthread_mutex_t dsp_list_lock;

/* Global DSP Callbacks List */
static QList dsp_callbacks;
static pthread_mutex_t dsp_callbacks_lock;

static pthread_spinlock_t handle_lock;
static struct module *handles = NULL; // Handle hash table

/* Initialization control */
static pthread_once_t fastrpc_init_once = PTHREAD_ONCE_INIT;

/* Helper function to lock and unlock dsp_list_lock */
static void lock_dsp_list() {
    pthread_mutex_lock(&dsp_list_lock);
}

static void unlock_dsp_list() {
    pthread_mutex_unlock(&dsp_list_lock);
}

/* Helper function to lock and unlock dsp->dsp_lock */
static void lock_dsp(struct dsp *dsp) {
    pthread_mutex_lock(&dsp->dsp_lock);
}

static void unlock_dsp(struct dsp *dsp) {
    pthread_mutex_unlock(&dsp->dsp_lock);
}

/* Helper function to iterate over dsp_list */
static struct dsp *find_dsp(int dsp_id) {
    struct dsp *dsp = NULL;
    QNode *node;

    QLIST_FOR_ALL(&dsp_list, node) {
        dsp = STD_RECOVER_REC(struct dsp, n, node);
        if (dsp->dsp_id == dsp_id) {
            return dsp;
        }
    }
    return NULL;
}

/* Helper function to iterate over dsp->sessions */
static struct session *find_session(struct dsp *dsp, int session_id) {
    struct session *sess = NULL;
    QNode *node;

    QLIST_FOR_ALL(&dsp->sessions, node) {
        sess = STD_RECOVER_REC(struct session, n, node);
        if (sess->session_id == session_id) {
            return sess;
        }
    }
    return NULL;
}

/* Helper function to call dsp callbacks */
static int call_dsp_callbacks(int callback_type, void *data) {
    QNode *node, *nnode;
    int err = AEE_SUCCESS;

    if (QList_IsEmpty(&dsp_callbacks)) {
        LOG_ERR("No DSP callbacks registered");
        return AEE_EFAILED;
    }

    QLIST_NEXTSAFE_FOR_ALL(&dsp_callbacks, node, nnode) {
        struct dsp_callback_node *callback_node = STD_RECOVER_REC(struct dsp_callback_node, n, node);
        if (callback_node->type == callback_type) {
            callback_node->callback(callback_type, callback_node->context, data, &err);
            if (err) {
                return err;
            }
        }
    }
    return AEE_SUCCESS;
}

/* Helper function to call session callbacks */
static int call_session_callbacks(struct dsp *dsp, int callback_type, void *data) {
    QNode *node, *nnode;
    int err = AEE_SUCCESS;

    if(QList_IsEmpty(&dsp->session_callbacks)) {
        LOG_ERR("No session callbacks registered");
        return AEE_EFAILED;
    }

    QLIST_NEXTSAFE_FOR_ALL(&dsp->session_callbacks, node, nnode) {
        struct session_callback_node *callback_node = STD_RECOVER_REC(struct session_callback_node, n, node);
        if (callback_node->type == callback_type) {
            callback_node->callback(callback_type, callback_node->context, data, &err);
            if (err) {
                return err;
            }
        }
    }
    return AEE_SUCCESS;
}

/* Register DSP callback */
void register_dsp_callback(int type, fastrpc_callback_t callback)
{
    struct dsp_callback_node *node = malloc(sizeof(*node));
    if (!node) {
        LOG_ERR("Failed to allocate memory for DSP callback node");
        return;
    }
    
    QNode_CtorZ(&node->n);
    node->callback = callback;
    node->type = type;

    pthread_mutex_lock(&dsp_callbacks_lock);
    QList_AppendNode(&dsp_callbacks, &node->n);
    pthread_mutex_unlock(&dsp_callbacks_lock);

    LOG_INF("Registered DSP callback");
}

/* Register session callback to DSP */
void register_session_callback(struct dsp *dsp, int type, fastrpc_callback_t callback)
{
    struct session_callback_node *node = malloc(sizeof(*node));
    if (!node) {
        LOG_ERR("Failed to allocate memory for session callback node");
        return;
    }

    QNode_CtorZ(&node->n);
    node->callback = callback;
    node->type = type;

    lock_dsp(dsp);
    QList_AppendNode(&dsp->session_callbacks, &node->n);
    unlock_dsp(dsp);

    LOG_INF("Registered session callback for DSP ID %d", dsp->dsp_id);
}

/* Check if Session is in Use */
static int session_is_in_use(struct session *sess)
{
    int in_use;

    pthread_spin_lock(&sess->lock);
    in_use = (sess->active_users > 0);
    pthread_spin_unlock(&sess->lock);

    return in_use;
}

/* Validate Session */
static int validate_session(struct session *sess)
{
    struct dsp *dsp = NULL;
    QNode *node, *snode;
    bool session_found = false;

    if (!sess) {
        LOG_ERR("Invalid session pointer!!");
        return AEE_EINVALIDPARAM;
    }

    lock_dsp_list();

    QLIST_FOR_ALL(&dsp_list, node) {
        dsp = STD_RECOVER_REC(struct dsp, n, node);

        // Check if the session is valid by looking it up in the session list
        lock_dsp(dsp);
    
        if(QList_IsEmpty(&dsp->sessions)) {
            unlock_dsp(dsp);
            continue;
        }

        QLIST_FOR_ALL(&dsp->sessions, snode) {
            struct session *sess_node = STD_RECOVER_REC(struct session, n, snode);
            if (sess_node == sess) {
                if(sess_node->dsp == dsp) {
                    session_found = true;
                    break;
                }
            }
        }

        unlock_dsp(dsp);

        if(session_found) {
            break;
        }
    }

    if (!session_found)
        LOG_ERR("Session ID %d is not valid for DSP ID %d", sess->session_id, dsp->dsp_id);

    unlock_dsp_list();

    if (!session_found) {
        return AEE_EINVALIDPARAM;
    }

//    LOG_INF("Session ID %d is valid for DSP ID %d", sess->session_id, dsp->dsp_id);
    return AEE_SUCCESS;
}

/* Validate Params */
static int validate_params(struct session *sess, uint32_t sc, remote_arg64 *params)
{
    QNode *node;

    // Check if the fds in params are already mapped on the session
    for (size_t i = 0; i < REMOTE_SCALARS_LENGTH(sc); i++) {
        bool found = false;
        QLIST_FOR_ALL(&sess->maps, node) {
            struct rpcmem *mem_node = STD_RECOVER_REC(struct rpcmem, n, node);
            if (mem_node->fd == params[i].dma.fd) {
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_ERR("FD %d in params is not mapped on session ID %d", params[i].dma.fd, sess->session_id);
            return AEE_EINVALIDPARAM;
        }
    }

    return AEE_SUCCESS;
}

/* Get Session from Handle */
struct session *get_session_from_handle(remote_handle64 handle)
{
    struct module *entry = NULL;

    pthread_spin_lock(&handle_lock);
    HASH_FIND_INT(handles, (void*)handle, entry);
    pthread_spin_unlock(&handle_lock);

    if (entry) {
        return entry->sess;
    } else {
        LOG_ERR("Handle %p not found in hash table", (void *)handle);
        return NULL;
    }
}

/* Initialize FastRPC */
void fastrpc_init_impl(void)
{
    pthread_mutexattr_t attr;

    QList_Ctor(&dsp_list);
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&dsp_list_lock, &attr);

    QList_Ctor(&dsp_callbacks);
    pthread_spin_init(&handle_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init(&dsp_callbacks_lock, &attr);

    pthread_mutexattr_destroy(&attr);
}

void fastrpc_init(void)
{
    pthread_once(&fastrpc_init_once, fastrpc_init_impl);
}

/* Initialize DSP */
struct dsp *dsp_init(int dsp_id)
{
    pthread_mutexattr_t attr;
    struct dsp *dsp = NULL;

    lock_dsp_list();

    /* Check if DSP is already initialized */
    dsp = find_dsp(dsp_id);
    if (dsp) {
        dsp->active_dsp++;
        LOG_INF("DSP ID %d already initialized, %p, count %d", dsp_id, dsp, dsp->active_dsp);
        unlock_dsp_list();
        return dsp;
    }

    dsp = malloc(sizeof(*dsp));
    if (!dsp) {
        unlock_dsp_list();
        LOG_DSP_INIT_ERR("Failed to allocate memory for DSP ID %d", dsp_id);
        return NULL;
    }

    dsp->active_dsp = 1;
    dsp->dsp_id = dsp_id;
    QList_Ctor(&dsp->sessions);
    QList_Ctor(&dsp->session_callbacks);
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&dsp->dsp_lock, &attr);
    QNode_CtorZ(&dsp->n);

    QList_AppendNode(&dsp_list, &dsp->n);

    /* Call registered DSP callbacks for init */
    int err = call_dsp_callbacks(CALLBACK_TYPE_INIT, dsp);
    if (err) {
        unlock_dsp_list();
        LOG_DSP_INIT_ERR("DSP callback failed during init DSP ID %d", dsp_id);
        return NULL;
    }

    LOG_DSP_INIT_INF("Initialized DSP ID %d, %p", dsp_id, dsp, dsp->active_dsp);
    unlock_dsp_list();

    return dsp;
}

/* Deinitialize DSP */
int dsp_deinit(int dsp_id)
{
    struct dsp *dsp = NULL;
    QNode *node, *next_node;
    QNode *ncallbacks, *nncallbacks;
    int ret = AEE_SUCCESS;

    lock_dsp_list();

    if(QList_IsEmpty(&dsp_list)) {
        unlock_dsp_list();
        LOG_DSP_DEINIT_ERR("No DSPs initialized");
        return AEE_EINVALIDPARAM;
    }

    /* Find the DSP */
    QLIST_NEXTSAFE_FOR_ALL(&dsp_list, node, next_node) {
        dsp = STD_RECOVER_REC(struct dsp, n, node);
        if (dsp->dsp_id == dsp_id) {
            lock_dsp(dsp);

            // Decrement active_dsp reference count
            dsp->active_dsp--;
            if (dsp->active_dsp > 0) {
                LOG_DSP_DEINIT_INF("DSP ID %d still has active references %d", dsp_id, dsp->active_dsp);
                unlock_dsp(dsp);
                unlock_dsp_list();
                return AEE_SUCCESS;
            }

            // Check and deinit sessions
            if (!QList_IsEmpty(&dsp->sessions)) {
                LOG_DSP_DEINIT_INF("DSP ID %d has active sessions", dsp_id);
                unlock_dsp(dsp);
                unlock_dsp_list();
                return AEE_EACTIVESESSIONS;
            }

            // Call deinit callbacks
            ret = call_dsp_callbacks(CALLBACK_TYPE_DEINIT, dsp);
            if (ret != AEE_SUCCESS) {
                LOG_DSP_DEINIT_ERR("DSP callback failed during deinit DSP ID %d", dsp_id);
                unlock_dsp(dsp);
                unlock_dsp_list();
                return ret;
            }

            QLIST_NEXTSAFE_FOR_ALL(&dsp->session_callbacks, ncallbacks, nncallbacks) {
                struct session_callback_node *callback_node = STD_RECOVER_REC(struct session_callback_node, n, ncallbacks);
                QNode_Dequeue(&callback_node->n);
                free(callback_node);
            }
            // Cleanup DSP
            QNode_Dequeue(&dsp->n);
            unlock_dsp(dsp);
            pthread_mutex_destroy(&dsp->dsp_lock);
            free(dsp);
            unlock_dsp_list();
            
            LOG_DSP_DEINIT_INF("Deinitialized DSP ID %d", dsp_id);
            return AEE_SUCCESS;
        }
    }

    unlock_dsp_list();
    LOG_DSP_DEINIT_ERR("DSP deinit failed for DSP ID %d not found", dsp_id);
    return AEE_EINVALIDPARAM;
}

/* Get Session Reference */
struct session *get_session(struct dsp *dsp, int session_id)
{
    struct session *sess = NULL;

    if (!dsp) {
        LOG_ERR("Invalid DSP pointer");
        return NULL;
    }

    lock_dsp(dsp);
    sess = find_session(dsp, session_id);
    if (sess) {
        pthread_spin_lock(&sess->lock);
        sess->active_users++;
        pthread_spin_unlock(&sess->lock);
    }
    unlock_dsp(dsp);

    if (!sess) {
        LOG_ERR("Session ID %d not found in DSP ID %d", session_id, dsp->dsp_id);
        return NULL;
    }

    return sess;
}

/* Put Session Reference */
void put_session(struct session *sess)
{
    if (!sess) {
        LOG_ERR("Invalid session pointer");
        return;
    }

    pthread_spin_lock(&sess->lock);
    if (sess->active_users > 0) {
        sess->active_users--;
    }
    pthread_spin_unlock(&sess->lock);
}

/* Initialize Session */
struct session *session_init(struct dsp *dsp, int session_id)
{
    struct session *sess = NULL;

    if (!dsp) {
        LOG_SESS_INIT_ERR("Invalid DSP pointer");
        return NULL;
    }

    /* Lock DSP for thread safety during session operations */
    lock_dsp(dsp);

    /* Check if session already exists - avoid duplicates */
    sess = find_session(dsp, session_id);
    if (sess) {
        pthread_spin_lock(&sess->lock);
        sess->active_users++;
        pthread_spin_unlock(&sess->lock);
        LOG_SESS_INIT_INF("Session ID %d already exists for DSP ID %d, active users %d", session_id, dsp->dsp_id, sess->active_users);
        unlock_dsp(dsp);
        return sess;
    }

    /* Allocate new session with error checking */
    sess = calloc(1, sizeof(*sess));
    if (!sess) {
        unlock_dsp(dsp);
        LOG_SESS_INIT_ERR("Failed to allocate memory for session ID %d", session_id);
        return NULL;
    }

    /* Initialize session fields atomically */
    sess->session_id = session_id;
    sess->active_users = 0;
    sess->dsp = dsp;
    sess->config = config_store_create();
    if (!sess->config) {
        unlock_dsp(dsp);
        free(sess);
        LOG_SESS_INIT_ERR("Failed to create config store for session ID %d", session_id);
        return NULL;
    }
    QList_Ctor(&sess->maps);

    /* Initialize session lock */
    if (pthread_spin_init(&sess->lock, PTHREAD_PROCESS_PRIVATE) != 0) {
        unlock_dsp(dsp);
        free(sess);
        LOG_SESS_INIT_ERR("Failed to initialize session lock for ID %d", session_id);
        return NULL; 
    }
    
    QNode_CtorZ(&sess->n);

    /* Process callbacks under DSP lock */
    int err = call_session_callbacks(dsp, CALLBACK_TYPE_INIT, sess);
    if (err) {
        pthread_spin_destroy(&sess->lock);
        unlock_dsp(dsp);
        free(sess);
        LOG_SESS_INIT_ERR("Callback failed for session ID %d", session_id);
        return NULL;
    }

    /* Add session to DSP's session list */
    QList_AppendNode(&dsp->sessions, &sess->n);
    LOG_SESS_INIT_INF("Initialized session %p ID %d for DSP ID %d, active session %d", sess, session_id, dsp->dsp_id, sess->active_users);
    unlock_dsp(dsp);

    return sess;
}

/* Deinitialize Session */
int session_deinit(struct session *sess)
{
    struct dsp *dsp;
    QNode *node, *nnode;
    int session_id;

    if (!sess) {
        LOG_SESS_DEINIT_ERR("Invalid session pointer");
        return AEE_EINVALIDPARAM;
    }

    session_id = sess->session_id;

    lock_dsp_list();
    if (QList_IsEmpty(&dsp_list)) {
        unlock_dsp_list();
        LOG_SESS_DEINIT_ERR("No DSPs initialized");
        return AEE_EINVALIDPARAM;
    }

    QLIST_NEXTSAFE_FOR_ALL(&dsp_list, node, nnode) {
        dsp = STD_RECOVER_REC(struct dsp, n, node);
        lock_dsp(dsp);
        if (QList_IsEmpty(&dsp->sessions)) {
            LOG_INF("No sessions found in DSP ID %d, moving to the next dsp", dsp->dsp_id);
            unlock_dsp(dsp);
            continue;
        }

        // Find matching session
        struct session *found_sess = find_session(dsp, session_id);
        if (!found_sess || found_sess != sess) {
            LOG_SESS_DEINIT_INF("Session ID %d found is not same as the session ptr", session_id);
            unlock_dsp(dsp);
            continue;
        }

        pthread_spin_lock(&sess->lock);
        sess->active_users--;
        pthread_spin_unlock(&sess->lock);

        // Check active users
        if (session_is_in_use(sess)) {
            LOG_SESS_DEINIT_INF("Cannot deinit session ID %d with active users %d", session_id, sess->active_users);
            unlock_dsp(dsp);
            unlock_dsp_list();
            return AEE_EACTIVESESSIONS;
        }

        pthread_spin_lock(&sess->lock);

        // Clean up memory maps
        QNode *map_node, *map_next;
        QLIST_NEXTSAFE_FOR_ALL(&sess->maps, map_node, map_next) {
            struct rpcmem *mem = STD_RECOVER_REC(struct rpcmem, n, map_node);
            QNode_Dequeue(&mem->n);
            free(mem);
        }
        pthread_spin_unlock(&sess->lock);

        // Process deinit callbacks
        int err = call_session_callbacks(dsp, CALLBACK_TYPE_DEINIT, sess);
        if (err) {
            unlock_dsp(dsp);
            unlock_dsp_list();
            LOG_SESS_DEINIT_ERR("Callback failed during session %d deinit", session_id);
            return AEE_EFAILED;
        }

        config_store_destroy(sess->config);
        // Cleanup session
        QNode_Dequeue(&sess->n);
        pthread_spin_destroy(&sess->lock);
        free(sess);
        LOG_SESS_DEINIT_INF("Successfully deinitialized session ID %d", session_id);
        unlock_dsp(dsp);
        unlock_dsp_list();
        return AEE_SUCCESS;
    }

    unlock_dsp_list();
    LOG_SESS_DEINIT_ERR("Session ID %d not found in any DSP", session_id);
    return AEE_EINVALIDPARAM;
}

/* Add Module to Session */
remote_handle64 session_add_module(struct session *sess, const char *name)
{
    struct module *mod;
    struct dsp *dsp;

    if (!sess) {
        LOG_ERR("Invalid session pointer");
        return AEE_EINVALIDPARAM;
    }

    dsp = sess->dsp;

    mod = malloc(sizeof(*mod));
    if (!mod) {
        LOG_ERR("Failed to allocate memory for module %s", name);
        return AEE_ENOMEM;
    }

    strncpy(mod->name, name, sizeof(mod->name) - 1);

    /* Call registered session callbacks for load */
    int err = call_session_callbacks(dsp, CALLBACK_TYPE_LOAD, mod->name);
    if (err) {
        LOG_ERR("Failed to load module %s", mod->name);
        free(mod);
        return AEE_EFAILED;
    }

    mod->handle = (remote_handle64)mod;
    mod->sess = sess;
    pthread_spin_lock(&sess->lock);
    sess->active_users++;
    pthread_spin_unlock(&sess->lock);

    pthread_spin_lock(&handle_lock);
    HASH_ADD_INT(handles, handle, mod);
    pthread_spin_unlock(&handle_lock);

    LOG_INF("Added module %s to session ID %d", name, sess->session_id);
    return mod->handle;
}

/* Remove Module from Session */
void session_remove_module(struct session *sess, remote_handle64 handle)
{
    struct module *mod = NULL;
    struct dsp *dsp;

    if (!sess) {
        LOG_ERR("Invalid session pointer");
        return;
    }

    dsp = sess->dsp;

    pthread_spin_lock(&handle_lock);
    HASH_FIND_INT(handles, (void*)handle, mod);
    pthread_spin_unlock(&handle_lock);

    /* Call registered session callbacks for unload */
    if(mod) {
        int err = call_session_callbacks(dsp, CALLBACK_TYPE_UNLOAD, sess);
        if(err) {
            LOG_ERR("Failed to unload module ID %p", (void*)handle);
        }
        /* Remove module from hash table */
        pthread_spin_lock(&handle_lock);
        HASH_DEL(handles, mod);
        pthread_spin_unlock(&handle_lock);

        pthread_spin_lock(&sess->lock);
        sess->active_users--;
        pthread_spin_unlock(&sess->lock);

        LOG_INF("Removed module ID %p from session ID %d", (void*)handle, sess->session_id);
    } else {
        LOG_ERR("Module ID %p not found in session ID %d", (void*)handle, sess->session_id);
    }
}

/* Configure Session */
int session_configure(struct session *sess, char* config_key, void *config_value)
{
    struct dsp *dsp = NULL;

    if (!config_key || !config_value) {
        LOG_ERR("Invalid config key or value");
        return AEE_EINVALIDPARAM;
    }
    if (!sess) {
        LOG_ERR("Invalid session pointer");
        return AEE_EINVALIDPARAM;
    }

    if (validate_session(sess) != AEE_SUCCESS) {
        LOG_ERR("Invalid session pointer");
        return AEE_EINVALIDPARAM;
    }

    dsp = sess->dsp;

    // Check return value from config_store_set
    int err = config_store_set(sess->config, config_key, config_value);
    if (err != AEE_SUCCESS) {
        LOG_ERR("Failed to set config value for key %s", config_key);
        return err;
    }

    /* Call registered session callbacks for configure */
    err = call_session_callbacks(dsp, CALLBACK_TYPE_CONFIGURE, sess->config);
    if (err) {
        LOG_ERR("Failed to configure session ID %d with config key %s", sess->session_id, config_key);
        return AEE_EFAILED;
    }

    LOG_INF("Configured session ID %d with config key %s", sess->session_id, config_key);
    return AEE_SUCCESS;
}

/* Invoke Session */
int session_invoke(struct session *sess, remote_handle64 handle, uint32_t sc, remote_arg64 *params)
{
    struct dsp *dsp = NULL; // Assume you have a way to get the DSP from the session

    if (validate_session(sess) != AEE_SUCCESS) {
        return AEE_EINVALIDPARAM;
    }

    dsp = sess->dsp;

    if (validate_params(sess, sc, params) != AEE_SUCCESS) {
        LOG_SESS_INV_ERR("Invalid params for session ID %d handle %p", sess->session_id, (void *)handle);
        return AEE_EINVALIDPARAM;
    }

    struct invoke_params invoke_args;
    invoke_args.handle = handle;
    invoke_args.sc = sc;
    invoke_args.args = params;

    /* Call registered session callbacks for invoke */
    int err = call_session_callbacks(dsp, CALLBACK_TYPE_INVOKE, &invoke_args);
    if(err) {
        LOG_SESS_INV_ERR("Failed to invoke session ID %d with handle %p and sc %u", 
                 sess->session_id, (void *)handle, sc);
        return AEE_EFAILED;
    }

    // Add your invocation logic here
    LOG_SESS_INV_INF("Invoked session ID %d with handle %p and sc %u", sess->session_id, (void *)handle, sc);
    return AEE_SUCCESS;
}

/* Map Memory to Session */
void *session_map_memory(struct session *sess, struct rpcmem *mem)
{
    struct dsp *dsp = NULL; // Assume you have a way to get the DSP from the session
    struct rpcmem *mem_copy;

    if (!mem) {
        LOG_SESS_MAP_ERR("Invalid memory pointer");
        return NULL;
    }

    if (validate_session(sess) != AEE_SUCCESS) {
        LOG_SESS_MAP_ERR("Invalid session pointer");
        return NULL;
    }

    dsp = sess->dsp;

    mem_copy = malloc(sizeof(*mem_copy));
    if (!mem_copy) {
        LOG_SESS_MAP_ERR("Failed to allocate memory for rpcmem copy");
        return NULL;
    }

    memcpy(mem_copy, mem, sizeof(*mem_copy));
    QNode_CtorZ(&mem_copy->n);

    /* Call registered session callbacks for map (before) */
    int err = call_session_callbacks(dsp, CALLBACK_TYPE_MAP, mem_copy);
    if (err) {
        LOG_SESS_MAP_ERR("Failed to map memory to session ID %d", sess->session_id);
        free(mem_copy);
        return NULL;
    }

    pthread_spin_lock(&sess->lock);
    QList_AppendNode(&sess->maps, &mem_copy->n);
    sess->active_users++;
    pthread_spin_unlock(&sess->lock);

    /* Call registered session callbacks for map (after) */
    err = call_session_callbacks(dsp, CALLBACK_TYPE_MAP, mem_copy);
    if (err) {
        LOG_SESS_MAP_ERR("Failed to map memory to session ID %d", sess->session_id);
        pthread_spin_lock(&sess->lock);
        QNode_Dequeue(&mem_copy->n);
        sess->active_users--;
        pthread_spin_unlock(&sess->lock);
        free(mem_copy);
        return NULL;
    }

    LOG_SESS_MAP_INF("Mapped memory to session ID %d", sess->session_id);
    return mem_copy->ptr;
}

/* Unmap Memory from Session */
void session_unmap_memory(struct session *sess, struct rpcmem *mem)
{
    struct dsp *dsp = NULL; // Assume you have a way to get the DSP from the session
    QNode *node;
    struct rpcmem *mem_node;

    if (!sess || !mem) {
        LOG_SESS_UNMAP_ERR("Invalid session or memory pointer");
        return;
    }

    if(validate_session(sess) != AEE_SUCCESS) {
        LOG_SESS_UNMAP_ERR("Invalid session pointer");
        return;
    }

    dsp = sess->dsp;

    pthread_spin_lock(&sess->lock);
    QLIST_FOR_ALL(&sess->maps, node) {
        mem_node = STD_RECOVER_REC(struct rpcmem, n, node);
        if (mem_node->fd == mem->fd && mem_node->ptr == mem->ptr) {
            QNode_Dequeue(&mem_node->n);
            LOG_SESS_UNMAP_INF("memory mapped fd %d to session ID %d", mem_node->fd, sess->session_id);
            free(mem_node);
            break;
        }
    }
    if (sess->active_users > 0)
        sess->active_users--;
    pthread_spin_unlock(&sess->lock);

    /* Call registered session callbacks for unmap (after) */
    int err = call_session_callbacks(dsp, CALLBACK_TYPE_UNMAP, mem);
    if (err) {
        LOG_SESS_UNMAP_ERR("Failed to unmap memory from session ID %d", sess->session_id);
    }
}
