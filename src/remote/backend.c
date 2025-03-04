// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "remote.h"
#include "fastrpc.h"
#include "error.h"
#include "log.h"

static int remote_session_callback(int event, void **ctx, void *data, void **ret);

static int handle_dsp_init(void *data) {
    struct dsp *dsp = (struct dsp *)data;

    if (!dsp) {
        LOG_ERR("invalid DSP handle");
        return AEE_EINVALIDPARAM;
    }

    LOG_INF("Registering session callbacks for backend");
    register_session_callback(dsp, (CALLBACK_TYPE_INIT | 
                                    CALLBACK_TYPE_DEINIT |
                                    CALLBACK_TYPE_OPEN |
                                    CALLBACK_TYPE_CLOSE |
                                    CALLBACK_TYPE_MAP |
                                    CALLBACK_TYPE_UNMAP |
                                    CALLBACK_TYPE_INVOKE),
                                    remote_session_callback);

    LOG_INF("Session callback registered successfully");    
    return AEE_SUCCESS;
}

static int handle_dsp_deinit(void *handle) {
    struct dsp *dsp = (struct dsp *)handle;

    if (!dsp) {
        LOG_ERR("invalid DSP handle");
        return AEE_EINVALIDPARAM;
    }

    unregister_session_callback(dsp, (CALLBACK_TYPE_INIT | 
        CALLBACK_TYPE_DEINIT |
        CALLBACK_TYPE_OPEN |
        CALLBACK_TYPE_CLOSE |
        CALLBACK_TYPE_MAP |
        CALLBACK_TYPE_UNMAP |
        CALLBACK_TYPE_INVOKE),
        remote_session_callback);

    LOG_INF("Session callback unregistered successfully");
    return AEE_SUCCESS;
}

static int handle_session_init(void **ctx, void *session) {
    return AEE_SUCCESS;
}

static int handle_session_deinit(void *ctx, void *session) {
    return AEE_SUCCESS;
}

static int handle_session_open(void *ctx, void *config) {
    return AEE_SUCCESS;
}


static int handle_session_close(void *ctx, void *session) {
    return AEE_SUCCESS;
}

static int handle_session_invoke(void *ctx, void *invoke_params) {
    return AEE_SUCCESS;
}

static int handle_session_map(void *ctx, void *rpcmem) {
    return AEE_SUCCESS;
}

static int handle_session_unmap(void *ctx, void *rpcmem) {
    return AEE_SUCCESS;
}

int remote_dsp_callback(int event, void **ctx, void *data, int **ret)
{
    LOG_INF("DSP event callback: event=%d data=%p", event, data);
    switch (event) {
        case CALLBACK_TYPE_INIT:
            return handle_dsp_init(data);
        case CALLBACK_TYPE_DEINIT:
            return handle_dsp_deinit(data);
        default:
            LOG_ERR("DSP unsupported event callback: event=%d data=%p", event, data);
            return AEE_ENOTSUPPORTED;
    }
}

static int remote_session_callback(int event, void **ctx, void *data, void **ret)
{
    LOG_INF("Session event callback: event=%d data=%p", event, data);
    switch (event) {
        case CALLBACK_TYPE_INIT:
            return handle_session_init(ctx, data);
        case CALLBACK_TYPE_DEINIT:
            return handle_session_deinit(*ctx, data);
        case CALLBACK_TYPE_OPEN:
            return handle_session_open(*ctx, data);
        case CALLBACK_TYPE_CLOSE:
            return handle_session_close(*ctx, data);
        case CALLBACK_TYPE_INVOKE:
            return handle_session_invoke(*ctx, data);
        case CALLBACK_TYPE_MAP:
            return handle_session_map(*ctx, data);
        case CALLBACK_TYPE_UNMAP:
            return handle_session_unmap(*ctx, data);
        default:
            LOG_ERR("Session unknown event callback: event=%d data=%p", event, data);
            return AEE_ENOTSUPPORTED;
    }
    return AEE_ENOTSUPPORTED;
}

