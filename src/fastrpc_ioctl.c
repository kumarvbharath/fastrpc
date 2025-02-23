// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "fastrpc_ioctl.h"
#include "error.h"
#include "log.h"
#include "remote.h"
#include <sys/ioctl.h>

/**
 * @file fastrpc_ioctl.c
 * @brief FastRPC IOCTL Implementation
 *
 * Provides IOCTL interface for FastRPC operations:
 * - Initialization and attachment
 * - Remote invocation
 * - Memory mapping
 * - Performance monitoring
 * - Signal handling
 *
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

const char* get_secure_domain_name(int domain_id) {
    static const struct {
        int id;
        const char *name;
    } domain_map[] = {
        {ADSP_DOMAIN_ID, ADSPRPC_SECURE_DEVICE},
        {SDSP_DOMAIN_ID, SDSPRPC_SECURE_DEVICE},
        {MDSP_DOMAIN_ID, MDSPRPC_SECURE_DEVICE},
        {CDSP_DOMAIN_ID, CDSPRPC_SECURE_DEVICE},
        {CDSP1_DOMAIN_ID, CDSP1RPC_SECURE_DEVICE}
    };

    int domain = GET_DOMAIN(domain_id);
    for (size_t i = 0; i < sizeof(domain_map)/sizeof(domain_map[0]); i++) {
        if (domain_map[i].id == domain) {
            return domain_map[i].name;
        }
    }
    return DEFAULT_DEVICE;
}

int ioctl_init(const ioctl_init_params_t *params) {
    if (!params) {
        LOG_ERR("Invalid parameters");
        return AEE_EINVALIDPARAM;
    }

    switch (params->flags) {
        case FASTRPC_INIT_ATTACH:
            return ioctl(params->dev, FASTRPC_IOCTL_INIT_ATTACH, NULL);

        case FASTRPC_INIT_ATTACH_SENSORS:
            return ioctl(params->dev, FASTRPC_IOCTL_INIT_ATTACH_SNS, NULL);

        case FASTRPC_INIT_CREATE_STATIC: {
            struct fastrpc_ioctl_init_create_static init = {
                .namelen = params->shell.len,
                .memlen = params->mem.len,
                .name = (uint64_t)params->shell.data
            };
            return ioctl(params->dev, FASTRPC_IOCTL_INIT_CREATE_STATIC, &init);
        }

        case FASTRPC_INIT_CREATE: {
            struct fastrpc_ioctl_init_create init = {
                .file = (uint64_t)params->shell.data,
                .filelen = params->shell.len,
                .filefd = params->shell.fd,
                .attrs = params->attr,
                .siglen = params->tessiglen
            };
            return ioctl(params->dev, FASTRPC_IOCTL_INIT_CREATE, &init);
        }

        default:
            LOG_ERR("Invalid init flags: %d", params->flags);
            return AEE_EINVALIDPARAM;
    }
}

int ioctl_invoke(const ioctl_invoke_params_t *params) {
    if (!params || !params->pra) {
        LOG_ERR("Invalid parameters");
        return AEE_EINVALIDPARAM;
    }

    if (params->req < INVOKE || params->req > INVOKE_FD) {
        LOG_ERR("Unsupported invoke request: %d", params->req);
        return AEE_ENOTSUPPORTED;
    }

    struct fastrpc_ioctl_invoke invoke = {
        .handle = params->handle,
        .sc = params->sc,
        .args = (uint64_t)params->pra
    };

    return ioctl(params->dev, FASTRPC_IOCTL_INVOKE, &invoke);
}

int ioctl_invoke2_response(int dev, fastrpc_async_jobid *jobid,
                           remote_handle *handle, uint32_t *sc, int *result,
                           uint64_t *perf_kernel, uint64_t *perf_dsp) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_invoke2_notif(int dev, int *domain, int *session, int *status) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_mmap(const ioctl_mmap_params_t *params) {
    if (!params || !params->vaddrout) {
        LOG_ERR("Invalid parameters");
        return AEE_EINVALIDPARAM;
    }

    switch (params->req) {
        case MEM_MAP: {
            struct fastrpc_ioctl_mem_map map = {
                .version = 0,
                .fd = params->mem.fd,
                .offset = params->mem.offset,
                .flags = params->flags,
                .vaddrin = (uint64_t)params->vaddrin,
                .length = params->mem.len,
                .attrs = params->attr
            };
            int err = ioctl(params->dev, FASTRPC_IOCTL_MEM_MAP, &map);
            if (err == AEE_SUCCESS) {
                *params->vaddrout = map.vaddrout;
            }
            return err;
        }

        case MMAP:
        case MMAP_64: {
            struct fastrpc_ioctl_req_mmap map = {
                .fd = params->mem.fd,
                .flags = params->flags,
                .vaddrin = (uint64_t)params->vaddrin,
                .size = params->mem.len
            };
            int err = ioctl(params->dev, FASTRPC_IOCTL_MMAP, &map);
            if (err == AEE_SUCCESS) {
                *params->vaddrout = map.vaddrout;
            }
            return err;
        }

        default:
            LOG_ERR("Invalid mmap request: %d", params->req);
            return AEE_EINVALIDPARAM;
    }
}

int ioctl_munmap(int dev, int req, int attr, void *buf, int fd, int len,
                 uint64_t vaddr) {
  int ioErr = AEE_SUCCESS;

  switch (req) {
  case MEM_UNMAP:
  case MUNMAP_FD: {
    struct fastrpc_ioctl_mem_unmap unmap = {0};
    unmap.version = 0;
    unmap.fd = fd;
    unmap.vaddr = vaddr;
    unmap.length = len;
    ioErr = ioctl(dev, FASTRPC_IOCTL_MEM_UNMAP, (unsigned long)&unmap);
  } break;
  case MUNMAP:
  case MUNMAP_64: {
    struct fastrpc_ioctl_req_munmap unmap = {0};
    unmap.vaddrout = vaddr;
    unmap.size = (ssize_t)len;
    ioErr = ioctl(dev, FASTRPC_IOCTL_MUNMAP, (unsigned long)&unmap);
  } break;
  default:
    LOG_ERR("ERROR: %s Invalid request passed %d", __func__, req);
    break;
  }

  return ioErr;
}

int ioctl_getinfo(int dev, uint32_t *info) {
  *info = 1;
  return AEE_SUCCESS;
}

int ioctl_getdspinfo(int dev, int domain, uint32_t attr, uint32_t *capability) {
  int ioErr = AEE_SUCCESS;
  static struct fastrpc_ioctl_capability cap = {0};

  if (attr >= PERF_V2_DRIVER_SUPPORT && attr < FASTRPC_MAX_ATTRIBUTES) {
    *capability = 0;
    return 0;
  }

  cap.domain = domain;
  cap.attribute_id = attr;
  cap.capability = 0;
  ioErr = ioctl(dev, FASTRPC_IOCTL_GET_DSP_INFO, &cap);
  *capability = cap.capability;
  return ioErr;
}

int ioctl_setmode(int dev, int mode) {
  if (mode == FASTRPC_SESSION_ID1)
    return AEE_SUCCESS;

  return AEE_ENOTSUPPORTED;
}

int ioctl_control(int dev, int req, void *c) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_getperf(int dev, int key, void *data, int *datalen) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_signal_create(int dev, uint32_t signal, uint32_t flags) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_signal_destroy(int dev, uint32_t signal) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_signal_signal(int dev, uint32_t signal) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_signal_wait(int dev, uint32_t signal, uint32_t timeout_usec) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_signal_cancel_wait(int dev, uint32_t signal) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_sharedbuf(int dev,
                    struct fastrpc_proc_sharedbuf_info *sharedbuf_info) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_session_info(int dev, struct fastrpc_proc_sess_info *sess_info) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_optimization(int dev, uint32_t max_concurrency) {
  return AEE_ENOTSUPPORTED;
}

int ioctl_mdctx_manage(int dev, int req, void *user_ctx,
	unsigned int *domain_ids, unsigned int num_domain_ids, uint64_t *ctx)
{
	// TODO: Implement this for opensource
	return AEE_ENOTSUPPORTED;
}