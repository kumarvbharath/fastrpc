/**
 * @file procbuf.c
 * @brief Session-specific Process Buffer Management
 *
 * Provides:
 * - Session-specific shared buffer management
 * - Parameter packing with alignment
 * - Thread-safe operations
 * - Buffer tracking using hash tables
 * - Support for extended domain IDs
 * 
 * Buffer Layout:
 * +------------------+
 * | Parameter Count  | <- 4 bytes
 * +------------------+
 * | Param1 Header   | <- 4 bytes (8-bit ID, 24-bit size)
 * +------------------+
 * | Param1 Data     | <- Aligned size
 * +------------------+
 * | Param2 Header   |
 * +------------------+
 * | Param2 Data     |
 * +------------------+
 * |       ...       |
 * 
 * Thread Safety:
 * - Uses hash table level locking
 * - Uses per-buffer locking
 * - Safe for concurrent access
 *
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "procbuf.h"

static proc_buf_t *g_session_bufs = NULL;
static pthread_mutex_t g_hash_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Find or create buffer for session
 * 
 * @param session Session pointer
 * @param create Create if not found
 * @return Buffer pointer or NULL
 */
static proc_buf_t* get_session_buffer(void *session, int create) {
    proc_buf_t *pb = NULL;
    
    pthread_mutex_lock(&g_hash_lock);
    HASH_FIND_PTR(g_session_bufs, &session, pb);
    
    if (!pb && create) {
        pb = calloc(1, sizeof(proc_buf_t));
        if (pb) {
            pb->session = session;
            pthread_mutex_init(&pb->lock, NULL);
            HASH_ADD_PTR(g_session_bufs, session, pb);
        }
    }
    pthread_mutex_unlock(&g_hash_lock);
    
    return pb;
}

void* procbuf_session_callback(int type, void *ctx, void *sess, int *retVal) {
    struct session *session = (struct session*)sess;
    proc_buf_t *pb;

    if(!session) {
        LOG_ERR("Invalid session pointer");
        *retVal = AEE_EINVALIDPARAM;
        return NULL;
    }
    switch (type) {
        case CALLBACK_TYPE_INIT:
            pb = get_session_buffer(session, 1);
            if (!pb) {
                LOG_ERR("Failed to create buffer for session %p", sess);
                *retVal = AEE_ENOMEM;
                return NULL;
            }

            pb->buf = rpcmem_alloc(RPCMEM_DEFAULT_HEAP, 
                                  RPCMEM_HEAP_DEFAULT, PROC_BUF_SIZE);
            if(!pb->buf) {
                LOG_ERR("Failed to allocate buffer for session %p", sess);
                free(pb);
                *retVal = AEE_ENOMEM;
                return NULL;
            }

            pb->size = PROC_BUF_SIZE;
            pb->used = PARAM_HEADER_SIZE;
            pb->fd = rpcmem_to_fd(pb->buf);
            pb->param_count = 0;
            
            LOG_INF("Buffer initialized: session=%p size=%u fd=%d", 
                    sess, pb->size, pb->fd);
            break;

        case CALLBACK_TYPE_DEINIT:
            pthread_mutex_lock(&g_hash_lock);
            HASH_FIND_PTR(g_session_bufs, &sess, pb);
            if (pb) {
                HASH_DEL(g_session_bufs, pb);
                rpcmem_free(pb->buf);
                pthread_mutex_destroy(&pb->lock);
                free(pb);
                LOG_INF("Buffer cleaned up: session=%p", sess);
            }
            pthread_mutex_unlock(&g_hash_lock);
            break;

        default:
            LOG_ERR("Invalid session event: %d", type);
            *retVal = AEE_EINVALIDPARAM;
            return NULL;
    }

    *retVal = AEE_SUCCESS;
    return NULL;
}

/**
 * @brief Add parameter using session pointer
 */
int procbuf_add_param(void *session, uint32_t id, const void *data, uint32_t size) {
    if (!session || !data || !size) {
        LOG_ERR("Invalid parameters: sess=%p data=%p size=%u", 
                session, data, size);
        return AEE_EINVALIDPARAM;
    }

    proc_buf_t *pb = get_session_buffer(session, 0);
    if (!pb) {
        LOG_ERR("No buffer found for session %p", session);
        return AEE_EINVALIDSTATE;
    }

    return procbuf_add_param_to_buf(pb, id, data, size);
}

/**
 * @brief Add parameter using extended domain ID
 */
int procbuf_add_param_ext(uint32_t ext_domain, uint32_t id, 
                         const void *data, uint32_t size) {
    int domain = GET_DOMAIN_ID(ext_domain);
    int session_id = GET_SESSION_ID(ext_domain);
    proc_buf_t *pb = NULL;

    pthread_mutex_lock(&g_hash_lock);
    for (pb = g_session_bufs; pb != NULL; pb = pb->hh.next) {
        struct session *sess = (struct session*)pb->session;
        if (sess->dsp->dsp_id == domain && 
            sess->session_id == session_id) {
            break;
        }
    }
    pthread_mutex_unlock(&g_hash_lock);

    if (!pb) {
        LOG_ERR("No buffer found for domain=%d session=%d", 
                domain, session_id);
        return AEE_EINVALIDPARAM;
    }

    return procbuf_add_param_to_buf(pb, id, data, size);
}

/**
 * @brief Common parameter addition logic
 */
int procbuf_add_param_to_buf(proc_buf_t *pb, uint32_t id, const void *data, uint32_t size) {
    pthread_mutex_lock(&pb->lock);

    if (pb->param_count >= MAX_PARAMS) {
        pthread_mutex_unlock(&pb->lock);
        LOG_ERR("Maximum parameters reached for buffer %p", pb);
        return AEE_EFAILED;
    }

    uint32_t aligned_size = ALIGN(size, PARAM_ALIGN);
    uint32_t total_size = PARAM_HEADER_SIZE + aligned_size;

    if (pb->used + total_size > pb->size) {
        pthread_mutex_unlock(&pb->lock);
        LOG_ERR("Buffer full: needed=%u available=%u", 
                total_size, pb->size - pb->used);
        return AEE_ENOMEM;
    }

    // Get location for new parameter
    param_header_t *hdr = get_param_header(pb->buf, pb->param_count);
    
    // Write parameter header
    hdr->id = id;
    hdr->size = size;

    // Write parameter data
    void *data_ptr = get_param_data(hdr);
    memcpy(data_ptr, data, size);

    // Update buffer state
    pb->used += total_size;
    pb->param_count++;
    *(uint32_t*)pb->buf = pb->param_count;  // Update count at start of buffer

    pthread_mutex_unlock(&pb->lock);
    
    LOG_INF("Added parameter: buf=%p addr=%p id=%u size=%u total_params=%u", 
        pb->buf, hdr, id, size, pb->param_count);
    return AEE_SUCCESS;
}

/**
 * @brief Get buffer information for a session
 */
int procbuf_get_info(void *session, int *fd, void **addr, uint32_t *size) {
    if (!session || !fd || !addr || !size) {
        LOG_ERR("Invalid parameters");
        return AEE_EINVALIDPARAM;
    }

    proc_buf_t *pb = get_session_buffer(session, 0);
    if (!pb) {
        LOG_ERR("No buffer found for session %p", session);
        return AEE_EINVALIDSTATE;
    }

    pthread_mutex_lock(&pb->lock);
    *fd = pb->fd;
    *addr = pb->buf;
    *size = pb->used;
    pthread_mutex_unlock(&pb->lock);
    
    LOG_INF("Got buffer info: session=%p fd=%d addr=%p size=%u", 
            session, *fd, *addr, *size);
    return AEE_SUCCESS;
}

void* procbuf_dsp_callback(int event, void *ctx, void *dsp, int *retVal) {
    struct dsp *ldsp = (struct dsp*)dsp;
    int domain = (int)(uintptr_t)ctx;
    
    switch (event) {
        case CALLBACK_TYPE_INIT:
            LOG_INF("Initializing process buffer for domain %d", domain);
            register_session_callback(ldsp, CALLBACK_TYPE_INIT, procbuf_session_callback);
            break;
            
        case CALLBACK_TYPE_DEINIT:
            LOG_INF("Deinitializing process buffer for domain %d", domain);
            unregister_session_callback(ldsp, CALLBACK_TYPE_DEINIT, procbuf_session_callback);
            break;
            
        default:
            LOG_ERR("Unknown event %d for domain %d", event, domain);
            *retVal = AEE_EINVALIDPARAM;
    }
    return NULL;
}
