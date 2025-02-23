/**
 * @file listener.c
 * @brief FastRPC Listener Implementation
 *
 * Provides:
 * - Session initialization and cleanup
 * - Domain-specific listener threads
 * - Dynamic buffer management
 * - Thread-safe operations
 * - Module invocation handling
 *
 * Thread Safety:
 * - All public APIs are thread-safe
 * - Multiple listener instances can run concurrently
 * - Each domain has its own context and thread
 *
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/eventfd.h>
#include "sbuf_parser.h"
#include "sbuf.h"
#include "fastrpc.h"
#include "remote64.h"
#include "error.h"
#include "log.h"
#include "rpcmem.h"

#define LISTENER_INIT_BUF_SIZE (4 * 1024)  // 4KB initial buffer
#define LISTENER_MAX_DOMAINS   8
#define INVALID_DOMAIN       (-1)

typedef struct {
    int domain;
    int session_id;
    pthread_t thread;
    int eventfd;
    volatile int running;
    remote_handle64 handle;
    void* inBuf;
    size_t inBufSize;
    void* outBuf;
    size_t outBufSize;
} listener_context;

static void* listener_thread(void* arg) {
    listener_context* ctx = (listener_context*)arg;
    int result = AEE_SUCCESS;
    adsp_listener_ctx invoke_ctx = 0;
    remote_handle64 module_handle;
    uint32_t sc;
    remote_arg args[512] = {0};
    struct sbuf buf;
    int req_size;
    
    LOG_INF("Listener thread starting: domain=%d session=%d", 
            ctx->domain, ctx->session_id);

    // Initialize listener module
    result = adsp_listener1_init2(ctx->domain, &ctx->handle);
    if (result != AEE_SUCCESS) {
        LOG_ERR("Failed to initialize listener: domain=%d err=0x%x", 
                ctx->domain, result);
        return NULL;
    }
    LOG_INF("Listener initialized: domain=%d handle=0x%"PRIx64, 
            ctx->domain, ctx->handle);

    while (ctx->running) {
        // Get next invocation
        result = adsp_listener1_next2(ctx->handle, invoke_ctx, 0, 
                                    ctx->outBuf, ctx->outBufSize,
                                    &invoke_ctx, &module_handle, &sc,
                                    ctx->inBuf, ctx->inBufSize, &req_size);

        if (result == AEE_EPIPE || result == AEE_ENOSUCH || result == AEE_EINTR) {
            LOG_WARN("Listener interrupted, retrying: domain=%d", ctx->domain);
            invoke_ctx = 0;
            continue;
        }

        if (result != AEE_SUCCESS) {
            LOG_ERR("Listener next failed: domain=%d err=0x%x", ctx->domain, result);
            break;
        }

        // Handle buffer reallocation if needed
        if (req_size > ctx->inBufSize) {
            void* new_buf = rpcmem_realloc_internal(ctx->inBuf, ctx->inBufSize,
                                                   req_size);
            if (!new_buf) {
                LOG_ERR("Buffer realloc failed: domain=%d size=%d", 
                        ctx->domain, req_size);
                break;
            }
            ctx->inBuf = new_buf;
            ctx->inBufSize = req_size;
        }

        // Unpack and invoke
        sbuf_init(&buf, 0, ctx->inBuf, ctx->inBufSize);
        if (unpack_in_bufs(&buf, args, REMOTE_SCALARS_INBUFS(sc)) != AEE_SUCCESS) { 
            LOG_ERR("Failed to unpack buffers: domain=%d", ctx->domain);
            continue;
        }

        result = reverse_request(module_handle, sc, args);
        if (result != AEE_SUCCESS) {
            LOG_ERR("Invocation failed: domain=%d err=0x%x", 
                                          ctx->domain, result);
            if (!ctx->running) break;
        }
    }

    // Cleanup listener
    if (ctx->handle != 0) {
        result = adsp_listener1_deinit(ctx->handle);
        if (result != AEE_SUCCESS) {
            LOG_ERR("Failed to deinitialize listener: domain=%d err=0x%x",
                    ctx->domain, result);
        }
    }

    LOG_INF("Listener thread exiting: domain=%d session=%d", 
            ctx->domain, ctx->session_id);
    return NULL;
}

static void* listener_domain_init(int type, void *ctx, void *sess, int *retVal) {
    listener_context* lctx;
    struct session* sctx = (struct session*)sess;
    int ret = AEE_SUCCESS;

    lctx = calloc(1, sizeof(listener_context));
    if (!lctx) {
        LOG_ERR("Failed to allocate context");
        return AEE_ENOMEM;
    }

    lctx->domain = sctx->dsp->dsp_id;
    lctx->session_id = sctx->session_id;
    lctx->running = 1;
    lctx->eventfd = eventfd(0, 0);
    if (lctx->eventfd < 0) {
        LOG_ERR("Failed to create eventfd: %s", strerror(errno));
        ret = AEE_EFAILED;
        goto error;
    }

    // Initialize buffers
    lctx->inBuf = rpcmem_alloc_internal(LISTENER_INIT_BUF_SIZE);
    lctx->outBuf = rpcmem_alloc_internal(LISTENER_INIT_BUF_SIZE);
    if (!lctx->inBuf || !lctx->outBuf) {
        LOG_ERR("Failed to allocate buffers");
        ret = AEE_ENOMEM;
        goto error;
    }
    lctx->inBufSize = lctx->outBufSize = LISTENER_INIT_BUF_SIZE;

    // Start listener thread
    if (pthread_create(&lctx->thread, NULL, listener_thread, lctx) != 0) {
        LOG_ERR("Failed to create thread: %s", strerror(errno));
        ret = AEE_EFAILED;
        goto error;
    }

    ctx = lctx;
    return AEE_SUCCESS;

error:
    if (lctx->inBuf) rpcmem_free(lctx->inBuf);
    if (lctx->outBuf) rpcmem_free(lctx->outBuf);
    if (lctx->eventfd >= 0) close(lctx->eventfd);
    free(lctx);
    return ret;
}

static int listener_domain_deinit(int event, void *ctx, void *sess, int *retVal) {
    listener_context *lctx = (listener_context*)ctx;
    eventfd_t value = 1;
    struct timespec ts = {.tv_sec = 1, .tv_nsec = 0};

    if (!lctx) {
        LOG_ERR("Invalid context pointer");
        *retVal = AEE_EINVALIDPARAM;
        return AEE_EINVALIDPARAM;
    }

    if (event != CALLBACK_TYPE_DEINIT) {
        LOG_ERR("Invalid event type: %d", event);
        *retVal = AEE_EINVALIDPARAM;
        return AEE_EINVALIDPARAM;
    }

    LOG_INF("Deinitializing listener: domain=%d session=%d", 
            lctx->domain, lctx->session_id);

    // Signal thread to stop
    lctx->running = 0;
    if (eventfd_write(lctx->eventfd, value) < 0) {
        LOG_ERR("Failed to signal thread: %s", strerror(errno));
    }

    // Wait for thread to exit with timeout
    if (pthread_timedjoin_np(lctx->thread, NULL, &ts) != 0) {
        LOG_WARN("Thread join timeout, forcing cleanup: domain=%d", lctx->domain);
        pthread_cancel(lctx->thread);
        pthread_join(lctx->thread, NULL);
    }

    if (lctx->inBuf) {
        rpcmem_free(lctx->inBuf);
        lctx->inBuf = NULL;
    }
    
    if (lctx->outBuf) {
        rpcmem_free(lctx->outBuf);
        lctx->outBuf = NULL;
    }
    
    if (lctx->eventfd >= 0) {
        close(lctx->eventfd);
        lctx->eventfd = -1;
    }

    // Cleanup context
    memset(lctx, 0, sizeof(listener_context));
    free(lctx);

    LOG_INF("Listener deinitialized successfully");
    return AEE_SUCCESS;
}

void* listener_dsp_callback(int event, void *ctx, void *data, int *retVal) {
  struct dsp *dsp = (struct dsp *)data;
  LOG_INF("DSP event callback: event=%d data=%p", event, data);
  switch (event) {
      case CALLBACK_TYPE_INIT:
          register_session_callback(dsp, CALLBACK_TYPE_INIT, 
                                          listener_domain_init);
          register_session_callback(dsp, CALLBACK_TYPE_DEINIT, 
                                          listener_domain_deinit);
          break;
      case CALLBACK_TYPE_DEINIT:
          break;
      default:
          LOG_ERR("DSP unsupported event callback: event=%d data=%p", event, data);
          *retVal = AEE_ENOTSUPPORTED;
          return NULL;
  }
  LOG_INF("Session callback registered successfully");
  return NULL;
}