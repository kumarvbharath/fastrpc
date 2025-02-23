// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "remotectl.h"

// Remote method IDs
#define REMOTECTL_METHOD_OPEN     0
#define REMOTECTL_METHOD_CLOSE    1
#define REMOTECTL_METHOD_OPEN1    2
#define REMOTECTL_METHOD_CLOSE1   3
#define REMOTECTL_METHOD_HEAP     4
#define REMOTECTL_METHOD_PARAM    5

int remotectl1_open(const char* uri, remote_handle64* h) {
   return remote_handle64_open(uri, h);
}
int remotectl1_close(remote_handle64 h) {
   return __QAIC_REMOTE(remote_handle64_close)(h);
}

static int remotectl1_open1(remote_handle64 _handle, const char* name, int* handle, char* dlerror, int dlerrorLen, int* nErr) {
    uint32_t name_len = 0;
    remote_arg args[4] = {0};
    uint32_t prim_in[2] = {0};
    uint32_t prim_out[2] = {0};
    int err = 0;

    if(!name || !handle || !dlerror || dlerrorLen <= 0) {
        LOG_ERR("remotectl1_open1: invalid parameter");
        return AEE_EINVALIDPARAM;
    }
    name_len = strlen(name) + 1;

    // Setup input primitives
    prim_in[0] = name_len;
    prim_in[1] = dlerrorLen;

    // Setup argument buffers
    args[0].buf.pv = prim_in;
    args[0].buf.nLen = sizeof(prim_in);
    args[1].buf.pv = (void*)name;
    args[1].buf.nLen = prim_in[0];
    args[2].buf.pv = prim_out;
    args[2].buf.nLen = sizeof(prim_out);
    args[3].buf.pv = dlerror;
    args[3].buf.nLen = dlerrorLen;

    LOG_INF("remotectl1_open1: name=%s error_len=%d", name, dlerrorLen);
    err = remote_handle64_invoke(_handle, 
                                   REMOTE_SCALARS_MAKEX(0, REMOTECTL_METHOD_OPEN1, 2, 2, 0, 0),
                                   args);
    if (err) {
        *handle = 0;
        *nErr = err;
        LOG_ERR("remotectl1_open1 failed: err=0x%x handle=0x%"PRIx64, err, _handle);
        return err;
    }

    *handle = prim_out[0];
    *nErr = prim_out[1];
    LOG_INF("Successfully completed remotectl1_open1: err=0x%x handle=0x%"PRIx64" nErr %d", err, prim_out[0], prim_out[1]);
    return AEE_SUCCESS;
}

static int remotectl1_close1(remote_handle64 _handle, int handle, char* dlerror, int dlerrorLen, int* nErr) {
    remote_arg args[3] = {0};
    uint32_t prim_in[2] = {0};
    uint32_t prim_out[1] = {0};

    // Setup input primitives
    prim_in[0] = handle;
    prim_in[1] = dlerrorLen;

    // Setup argument buffers
    args[0].buf.pv = prim_in;
    args[0].buf.nLen = sizeof(prim_in);
    args[1].buf.pv = dlerror;
    args[1].buf.nLen = dlerrorLen;
    args[2].buf.pv = prim_out;
    args[2].buf.nLen = sizeof(prim_out);

    LOG_INF("remotectl1_close1: handle=%d error_len=%d", _handle, dlerrorLen);
    int err = remote_handle64_invoke(_handle,
                                   REMOTE_SCALARS_MAKEX(0, REMOTECTL_METHOD_CLOSE1, 1, 2, 0, 0),
                                   args);
    LOG_INF("remotectl1_close1: handle=%d error_len=%d", _handle, dlerrorLen);
    if (err) {
        *nErr = AEE_EFAILED;
        LOG_ERR("remotectl1_close1 failed: err=0x%x handle=0x%"PRIx64, err, handle);
        return err;
    }
    LOG_INF("Successfully completed remotectl1_close1: err=0x%x handle=0x%"PRIx64, err, handle);
    *nErr = prim_out[0];
    return AEE_SUCCESS;
}

int remotectl1_grow_heap(remote_handle64 _handle, uint32_t phyAddr, uint32_t nSize) {
    remote_arg args[1] = {0};
    uint32_t prim_in[2] = {0};

    if(!phyAddr || nSize <= 0) {
        LOG_ERR("remotectl1_grow_heap: invalid parameter");
        return AEE_EINVALIDPARAM;
    }
    // Setup input primitives
    prim_in[0] = phyAddr;
    prim_in[1] = nSize;

    // Setup argument buffer
    args[0].buf.pv = prim_in;
    args[0].buf.nLen = sizeof(prim_in);

    LOG_INF("remotectl1_grow_heap: addr=0x%x size=%u", phyAddr, nSize);
    int err = remote_handle64_invoke(_handle,
                                   REMOTE_SCALARS_MAKEX(0, REMOTECTL_METHOD_HEAP, 1, 0, 0, 0),
                                   args);
    if (err) {
        LOG_ERR("remotectl1_grow_heap failed: err=0x%x handle=0x%llx", err, _handle);
    }
    return err;
}

int remotectl1_set_param(remote_handle64 _handle, int reqID, const uint32_t* params, int paramsLen) {
    remote_arg args[2] = {0};
    uint32_t prim_in[2] = {0};

    if(!params || paramsLen <= 0) {
        LOG_ERR("remotectl1_set_param: invalid parameter");
        return AEE_EINVALIDPARAM;
    }

    // Setup input primitives
    prim_in[0] = reqID;
    prim_in[1] = paramsLen;

    // Setup argument buffers
    args[0].buf.pv = prim_in;
    args[0].buf.nLen = sizeof(prim_in);
    args[1].buf.pv = (void*)params;
    args[1].buf.nLen = paramsLen * sizeof(uint32_t);

    LOG_INF("remotectl1_set_param: req_id=%d params_len=%d", reqID, paramsLen);
    int err = remote_handle64_invoke(_handle,
                                   REMOTE_SCALARS_MAKEX(0, REMOTECTL_METHOD_PARAM, 2, 0, 0, 0),
                                   args);
    if (err) {
        LOG_ERR("remotectl1_set_param failed: err=0x%x handle=0x%"PRIx64, err, _handle);
    }
    return err;
}

static void* handle_session_load(void *ctx, void *name, int *retVal) {
    struct session_ctx *sctx = (struct session_ctx *)ctx;
    char dlerror[256];
    int dlerrorLen = sizeof(dlerror);
    int handle = -1, nErr = 0;
    int err = AEE_SUCCESS;

    err = remotectl1_open1(REMOTECTL_HANDLE, name, &handle, dlerror, dlerrorLen, &nErr);
    if (err) {
        LOG_ERR("Failed to open remote handle: err=0x%x", err);
        *retVal = nErr;
        return NULL;
    }
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_unload(void *ctx, void *handle, int *retVal) {
    struct session_ctx *sctx = (struct session_ctx *)ctx;
    char dlerror[256];
    int dlerrorLen = sizeof(dlerror);
    int nErr = 0;

    remotectl1_close1(REMOTECTL_HANDLE, handle, dlerror, dlerrorLen, nErr);
    if (nErr) {
        LOG_ERR("Failed to close remote handle: err=0x%x", nErr);
        *retVal = nErr;
        return NULL;
    }
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* remotectl_session_callback(int event, void **ctx, void *data, int *retVal) {
    switch (event) {
        case CALLBACK_TYPE_LOAD:
            return handle_session_load(ctx, data, retVal);
        case CALLBACK_TYPE_UNLOAD:
            return handle_session_unload(ctx, data, retVal);
        default:
            LOG_ERR("Unsupported event callback: event=%d data=%p", event, data);
            *retVal = AEE_ENOTSUPPORTED;
            return NULL;
    }
}

void* remotectl_dsp_callback(int event, void **ctx, void *data, int *retVal) {
    LOG_INF("DSP event callback: event=%d data=%p", event, data);
    struct dsp *dsp = (struct dsp *)data;
    switch (event) {
        case CALLBACK_TYPE_INIT:
            register_session_callback(dsp, CALLBACK_TYPE_LOAD, remotectl_session_callback);
            break;
        case CALLBACK_TYPE_DEINIT:
            unregister_session_callback(dsp, CALLBACK_TYPE_UNLOAD, remotectl_session_callback);
            break;
        default:
            LOG_ERR("DSP unsupported event callback: event=%d data=%p", event, data);
            *retVal = AEE_ENOTSUPPORTED;
            return NULL;
    }
}
