// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "backend.h"
#include "remotectl.h"
#include "remote.h"
#include "fastrpc.h"
#include "error.h"
#include "log.h"

static void* remote_session_callback(int event, void **ctx, void *data, int *retVal);

struct session_ctx {
    int dev;
    int sess_id;
    int domain_id;
};

struct init_params {
    uint32_t flags;
    int attr;
    struct {
        size_t len;
        void *data;
        int fd;
    } shell, mem, testsig;
};

static void* handle_dsp_init(void *data, int *retVal) {
    struct dsp *dsp = (struct dsp *)data;

    if (!dsp) {
        LOG_ERR("invalid DSP handle");
        *retVal = AEE_EINVALIDPARAM;
        return NULL;
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
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_dsp_deinit(void *handle, int *retVal) {
    struct dsp *dsp = (struct dsp *)handle;

    if (!dsp) {
        LOG_ERR("invalid DSP handle");
        *retVal = AEE_EINVALIDPARAM;
        return NULL;
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
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_init(void **ctx, void *session, int *retVal) {
    struct session *sess = (struct session *)session;
    *ctx = malloc(sizeof(struct session_ctx));
    if (!*ctx) {
        LOG_ERR("Failed to allocate session context");
        *retVal = AEE_ENOMEM;
        return NULL;
    }
    struct session_ctx *sctx = (struct session_ctx *)*ctx;
    sctx->sess_id = sess->session_id;
    sctx->domain_id = sess->dsp->dsp_id;
    LOG_INF("Session context %p, session %d, dsp %d", *ctx, sctx->sess_id, sctx->domain_id);
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_deinit(void *ctx, void *session, int *retVal) {
    free(ctx);
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* read_file_to_buffer(const char *path, size_t *size) {
    FILE *fp;
    void *buf = NULL;
    size_t file_size;

    fp = fopen(path, "rb");
    if (!fp) {
        LOG_ERR("Failed to open file: %s", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    buf = rpcmem_alloc(RPCMEM_HEAP_DEFAULT, 0, file_size);
    if (!buf) {
        LOG_ERR("Failed to allocate buffer");
        fclose(fp);
        return NULL;
    }
    
    if (fread(buf, 1, file_size, fp) != file_size) {
        LOG_ERR("Failed to read file");
        rpcmem_free(buf);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    *size = file_size;
    return buf;
}

static int handle_init_ioctl(int dev, uint32_t flags, struct init_params *params) {
    int err;

    switch (flags) {
        case 0x1: /* FASTRPC_INIT_ATTACH */
            return ioctl(dev, FASTRPC_IOCTL_INIT_ATTACH, NULL);

        case 0x2: /* FASTRPC_INIT_ATTACH_SENSORS */
            return ioctl(dev, FASTRPC_IOCTL_INIT_ATTACH_SNS, NULL);

        case 0x3: /* FASTRPC_INIT_CREATE_STATIC */ {
            struct fastrpc_ioctl_init_create_static init = {
                .namelen = params->shell.len,
                .memlen = params->mem.len,
                .name = (uint64_t)params->shell.data
            };
            return ioctl(dev, FASTRPC_IOCTL_INIT_CREATE_STATIC, &init);
        }

        case 0x4: /* FASTRPC_INIT_CREATE */ {
            struct fastrpc_ioctl_init_create init = {
                .file = (uint64_t)params->shell.data,
                .filelen = params->shell.len,
                .filefd = params->shell.fd,
                .attrs = params->attr,
                .siglen = params->testsig.len
            };
            return ioctl(dev, FASTRPC_IOCTL_INIT_CREATE, &init);
        }

        default:
            LOG_ERR("Invalid init flags: %d", flags);
            return AEE_EINVALIDPARAM;
    }
}

static void* handle_session_open(void *ctx, void *config, int *retVal) {
    config_store_t *cfg = (config_store_t *)config;
    struct session_ctx *sctx = (struct session_ctx *)ctx;
/*
    char filename[32], testsig = "testsig", devicenode[32] = "/dev/fastrpc";
    struct init_params params = {0};
    void *shell_buf = NULL;
    size_t shell_size = 0;
    int err;

    LOG_INF("Session context %p sctx %p Config store %p", ctx, sctx, cfg);
    if (!sctx) {
        LOG_ERR("Null context!!!");
        *retVal = AEE_ENOMEM;
        return NULL;
    }

    // Get configuration
    params.flags = *((int*)config_store_get(cfg, "fastrpc.init.flags"));
    struct remote_process_type *pdtype = (struct remote_process_type*) config_store_get(config, "fastrpc.pd.type");
    params.attr = *((int*)config_store_get(cfg, "fastrpc.pd.attr"));
    const char *type = (pdtype->process_type == 0) ? "signed" : "unsigned";
    LOG_INF("sctx sessionid %d, domainid %d", sctx->sess_id, sctx->domain_id);
    snprintf(filename, 32, "fastrpc_shell_%s_%d", type, sctx->domain_id);

    LOG_INF("Init params: flags=%llx, attr=%llx, type=%s, filename=%s", params.flags, params.attr, type, filename);
    // Read shell file if specified
    if (filename) {
        shell_buf = read_file_to_buffer(filename, &shell_size);
        if (!shell_buf) {
            free(sctx);
            *retVal = AEE_EFAILED;
            return NULL;
        }
        params.shell.data = shell_buf;
        params.shell.len = shell_size;
    }

    // Open device
    sctx->dev = open(devicenode, O_RDWR);
    if (sctx->dev < 0) {
        LOG_ERR("Failed to open device: %s", strerror(errno));
        if (shell_buf) rpcmem_free(shell_buf);
        free(sctx);
        *retVal = AEE_EFAILED;
        return NULL;
    }

    // Handle initialization
    err = handle_init_ioctl(sctx->dev, params.flags, &params);
    if (err) {
        LOG_ERR("Init failed: err=%d", err);
        close(sctx->dev);
        if (shell_buf) rpcmem_free(shell_buf);
        free(sctx);
        *retVal = err;
        return NULL;
    }

    LOG_INF("Session opened successfully: dev=%d", sctx->dev);
*/
    *retVal = AEE_SUCCESS;
    return NULL;
}


static void* handle_session_close(void *ctx, void *session, int *retVal) {
    struct session_ctx *sctx = (struct session_ctx *)ctx;
    int err = AEE_SUCCESS;

    err = close(sctx->dev);
    if (err) {
        LOG_ERR("Failed to close dev: err=0x%x", err);
        *retVal = err;
        return NULL;
    }

    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_invoke(void *ctx, void *invoke_params, int *retVal) {
    struct session_ctx *sctx = (struct session_ctx *)ctx;
    struct invoke_params *params = (struct invoke_params *)invoke_params;
/*
    if (!sctx || !params || !params->args) {
        LOG_ERR("Invalid parameters");
        *retVal = AEE_EINVALIDPARAM;
        return NULL;
    }
    params->type = 0; //TODO: Add support for other types

    // Only support normal invoke, check if this is a different type
    if (params->type != 0) {
        LOG_ERR("Unsupported invoke type with attrs=%d", params->type);
        *retVal = AEE_ENOTSUPPORTED;
        return NULL;
    }

    struct fastrpc_ioctl_invoke invoke = {
        .handle = params->handle,
        .sc = params->sc,
        .args = (uint64_t)params->args
    };

    *retVal = ioctl(sctx->dev, FASTRPC_IOCTL_INVOKE, &invoke);
*/
    *retVal = AEE_SUCCESS;
    return NULL;
}

static void* handle_session_map(void *ctx, void *rpcmem, int *retVal) {
    struct session_ctx *sctx = (struct session_ctx *)ctx;
    struct rpcmem *mem = (struct rpcmem *)rpcmem;
    uint64_t mapped_ptr;

    // Try MEM_MAP first
    struct fastrpc_ioctl_mem_map mem_map = {
        .version = 0,
        .fd = mem->fd,
        .offset = mem->offset,
        .flags = mem->flags,
        .vaddrin = (uint64_t)mem->ptr,
        .length = mem->size,
        .attrs = mem->attr
    };

    int err = ioctl(sctx->dev, FASTRPC_IOCTL_MEM_MAP, &mem_map);
    if (err != -ENOTTY) { // If not "not supported" error
        if (err == AEE_SUCCESS) {
            mapped_ptr = mem_map.vaddrout;
        }
        *retVal = err;
        return mapped_ptr;
    }

    // Fall back to MMAP
    struct fastrpc_ioctl_req_mmap mmap = {
        .fd = mem->fd,
        .flags = mem->flags,
        .vaddrin = (uint64_t)mem->ptr,
        .size = mem->size
    };

    err = ioctl(sctx->dev, FASTRPC_IOCTL_MMAP, &mmap);
    if (err == AEE_SUCCESS) {
        mapped_ptr = mmap.vaddrout;
    }
    *retVal = err;
    return NULL;
}

static void* handle_session_unmap(void *ctx, void *rpcmem, int *retVal) {
    struct session_ctx *sctx = (struct session_ctx *)ctx;
    struct rpcmem *mem = (struct rpcmem *)rpcmem;
    int err;

    // Try MEM_UNMAP first
    struct fastrpc_ioctl_mem_unmap mem_unmap = {
        .version = 0,
        .fd = mem->fd,
        .vaddr = mem->ptr,
        .length = mem->size
    };
    
    err = ioctl(sctx->dev, FASTRPC_IOCTL_MEM_UNMAP, &mem_unmap);
    if (err != -ENOTTY) { // If not "not supported" error
        *retVal = err;
        return NULL;
    }

    // Fall back to MUNMAP
    struct fastrpc_ioctl_req_munmap munmap = {
        .vaddrout = mem->ptr,
        .size = mem->size
    };
    
    err = ioctl(sctx->dev, FASTRPC_IOCTL_MUNMAP, &munmap);
    *retVal = err;
    return NULL;
}

void* remote_dsp_callback(int event, void **ctx, void *data, int *retVal)
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

static void* remote_session_callback(int event, void **ctx, void *data, int *retVal)
{
    LOG_INF("Session event callback: event=%d data=%p", event, data);
    switch (event) {
        case CALLBACK_TYPE_INIT:
            return handle_session_init(ctx, data, retVal);
        case CALLBACK_TYPE_DEINIT:
            return handle_session_deinit(*ctx, data, retVal);
        case CALLBACK_TYPE_OPEN:
            return handle_session_open(*ctx, data, retVal);
        case CALLBACK_TYPE_CLOSE:
            return handle_session_close(*ctx, data, retVal);
        case CALLBACK_TYPE_INVOKE:
            return handle_session_invoke(*ctx, data, retVal);
        case CALLBACK_TYPE_MAP:
            return handle_session_map(*ctx, data, retVal);
        case CALLBACK_TYPE_UNMAP:
            return handle_session_unmap(*ctx, data, retVal);
        default:
            LOG_ERR("Session unknown event callback: event=%d data=%p", event, data);
            *retVal = AEE_ENOTSUPPORTED;
            return NULL;
    }
}

