// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "fastrpc.h"
#include "error.h"
#include "log.h"

/* Initialization control */
static pthread_once_t remote_init_once = PTHREAD_ONCE_INIT;

static void* remote_session_callback(int event, void *ctx, void *data, int *retVal);

static void* handle_dsp_init(void *data, int *retVal) {
    struct dsp *dsp = (struct dsp *)data;

    if (!dsp) {
        LOG_ERR("invalid DSP handle");
        *retVal = AEE_EINVALIDPARAM;
        return NULL;
    }

    register_session_callback(dsp, CALLBACK_TYPE_INIT, remote_session_callback);

    LOG_INF("Session callback registered successfully");    
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_dsp_deinit(void *handle, int *retVal) {
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_init(void *session, int *retVal) {
    //open device node
    //do INIT_CREATE or INIT_ATTACH ioctl -> get the details from the session.
    //proc attributes from session
    //for attachguestos or createstaticpd, no action should be taken.
    //for geteventfd, should return the fd from listener. TODO: Bharath to check how
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_deinit(void *session, int *retVal) {
    // nothing i guess.
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_load(void *session, int *retVal) {
    //remotectl_open
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_unload(void *session, int *retVal) {
    //remotectl_close
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_invoke(void *session, int *retVal) {
    LOG_INF("Session invoke callback: session=%p", session);
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_map(void *session, int *retVal) {
    //MMAP ioctl
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_unmap(void *session, int *retVal) {
    //MUNMAP ioctl
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* remote_dsp_callback(int event, void *ctx, void *data, int *retVal)
{
    LOG_INF("DSP event callback: event=%d data=%p", event, data);
    switch (event) {
        case CALLBACK_TYPE_INIT:
            return handle_dsp_init(data, retVal);
        case CALLBACK_TYPE_DEINIT:
            return handle_dsp_deinit(data, retVal);
        default:
            LOG_ERR("DSP unsupported event callback: event=%d data=%p", event, data);
            *retVal = AEE_ENOTSUPPORTED;
            return NULL;
    }
}

static void* remote_session_callback(int event, void *ctx, void *data, int *retVal)
{
    switch (event) {
        case CALLBACK_TYPE_INIT:
            return handle_session_init(data, retVal);
        case CALLBACK_TYPE_DEINIT:
            return handle_session_deinit(data, retVal);
        case CALLBACK_TYPE_LOAD:
            return handle_session_load(data, retVal);
        case CALLBACK_TYPE_UNLOAD:
            return handle_session_unload(data, retVal);
        case CALLBACK_TYPE_INVOKE:
            return handle_session_invoke(data, retVal);
        case CALLBACK_TYPE_MAP:
            return handle_session_map(data, retVal);
        case CALLBACK_TYPE_UNMAP:
            return handle_session_unmap(data, retVal);
        default:
            LOG_ERR("Session unknown event callback: event=%d data=%p", event, data);
            *retVal = AEE_ENOTSUPPORTED;
            return NULL;
    }
}

void remote_init_impl(void)
{
    register_dsp_callback(CALLBACK_TYPE_INIT, remote_dsp_callback);
}

void remote_init()
{
    pthread_once(&remote_init_once, remote_init_impl);
}
