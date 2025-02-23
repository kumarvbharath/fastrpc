// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file interface64.c
 * @brief FastRPC 64-bit Remote Interface Implementation
 *
 * This file implements the 64-bit remote interface for FastRPC, providing:
 * - Remote handle open/close/invoke operations
 * - URI parsing and validation
 * - Domain ID mapping and validation
 * - Session management integration
 * - Remote control operations
 *
 * The interface supports:
 * - Multiple DSP domains (ADSP, MDSP, SDSP, CDSP)
 * - Session-based module management
 * - Dynamic module loading
 * - Remote procedure invocation
 * - Session configuration
 *
 * URI Format:
 * file:///<soname>?_dom=<domain>&_session=<sessionid>&<interface>
 * 
 * Example:
 * file:///libtest.so?_dom=adsp&_session=1&_skel_handle_invoke
 *
 * Limitations:
 * - Only supports file-based module loading
 * - Session control operations are limited
 * - No support for async operations
 * - No built-in security validation
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fastrpc.h"
#include "remote.h"
#include "log.h"
#include "error.h"

typedef struct {
    char *sofilename;
    char *interface;
    int domain;
    int session;
} uri_info_t;

static int convert_domain_to_id(const char *domain) {
    if (strncmp(domain, "adsp", 4) == 0) {
        return ADSP_DOMAIN_ID;
    } else if (strncmp(domain, "mdsp", 4) == 0) {
        return MDSP_DOMAIN_ID;
    } else if (strncmp(domain, "sdsp", 4) == 0) {
        return SDSP_DOMAIN_ID;
    } else if (strncmp(domain, "cdsp", 4) == 0) {
        return CDSP_DOMAIN_ID;
    } else {
        return -1; // Invalid domain
    }
}

static uri_info_t* parse_uri(const char* uri) {
    if (!uri) {
        LOG_ERR("NULL URI provided");
        return NULL;
    }

    uri_info_t *info = (uri_info_t *)calloc(1, sizeof(uri_info_t));
    if (!info) {
        LOG_ERR("Failed to allocate memory for uri_info");
        return NULL;
    }

    info->domain = 3;  // Default domain value
    info->session = 0; // Default session value

    // Copy URI to a modifiable string
    char *uri_copy = strdup(uri);
    if (!uri_copy) {
        LOG_ERR("Failed to duplicate URI string");
        free(info);
        return NULL;
    }

    // Parse the URI
    char *saveptr = NULL;
    char *token = strtok_r(uri_copy, "?", &saveptr);
    if (token && strncmp(token, "file:///", 8) == 0) {
        info->sofilename = strdup(token + 8);
        if (!info->sofilename) {
            goto cleanup;
        }
    } else {
        goto cleanup;
    }

    bool found_interface = false;
    while ((token = strtok_r(NULL, "&", &saveptr)) != NULL) {
        if (strncmp(token, "_dom=", 5) == 0) {
            int domain = convert_domain_to_id(token + 5);
            if (domain == -1) {
                goto cleanup;
            }
            info->domain = domain;
        } else if (strncmp(token, "_session=", 9) == 0) {
            info->session = atoi(token + 9);
        } else if (strstr(token, "_skel_handle_invoke") != NULL) {
            info->interface = strdup(token);
            if (!info->interface) {
                goto cleanup;
            }
            found_interface = true;
        }
    }

    if (!found_interface) {
        goto cleanup;  // Interface is required
    }

    free(uri_copy);
    return info;

cleanup:
    free(info->sofilename);
    free(info->interface);
    free(info);
    free(uri_copy);
    return NULL;
}

int remote_handle64_open(const char* uri, remote_handle64 *ph) {
    fastrpc_init();

    LOG_INF("Opening remote handle for URI: %s", uri);
    uri_info_t *info = parse_uri(uri);
    if (!info) {
        LOG_ERR("Failed to parse URI: %s", uri);
        return AEE_EINVALIDPARAM;
    }

    if (info->domain == -1) {
        LOG_ERR("Invalid domain in URI: %s", uri);
        goto cleanup_info;
    }

    void *dsp = dsp_init(info->domain);
    if (!dsp) {
        LOG_ERR("Failed to initialize DSP for domain: %d", info->domain);
        goto cleanup_info;
    }

    int session_id = (info->session == -1) ? 0 : info->session;
    struct session *sess = session_init(dsp, session_id);
    if (!sess) {
        LOG_ERR("Failed to initialize session for domain: %d, session: %d", info->domain, session_id);
        goto cleanup_dsp;
    }

    // One-time initialization for static modules
    register_static_modules(sess);

    uintptr_t module_handle = session_add_module(sess, info->sofilename);
    if (!module_handle) {
        LOG_ERR("Failed to add module to session for domain: %d, session: %d", info->domain, session_id);
        goto cleanup_session;
    }

    // Free allocated memory
    free(info->sofilename);
    free(info->interface);
    free(info);

    // Assign the module handle to the provided handle pointer
    *ph = (remote_handle64)module_handle;

    // LOG_INF("Successfully opened remote handle %llx for uri: %s", module_handle, uri);
    return AEE_SUCCESS;

cleanup_session:
    session_deinit(sess);
cleanup_dsp:
    dsp_deinit(info->domain);
cleanup_info:
    free(info->sofilename);
    free(info->interface);
    free(info);
    return AEE_EFAILED;
}

int remote_handle64_invoke(remote_handle64 h, uint32_t dwScalars, remote_arg *pra) {
    fastrpc_init();

    LOG_INF("Invoking remote handle: %llx", h);
    struct session *sess = get_session_from_handle(h);
    if (!sess) {
        LOG_ERR("Failed to get session from handle: %llx", h);
        return AEE_EINVALIDPARAM;
    }

    get_session(sess->dsp, sess->session_id);

    int result = session_invoke(sess, h, dwScalars, pra);
    if (result != AEE_SUCCESS) {
        LOG_ERR("Failed to invoke session with handle: %llx, result: %d", h, result);
        return result;
    }

    put_session(sess);

    // LOG_INF("Successfully invoked session with handle: %llx", h);
    return AEE_SUCCESS;
}

int remote_handle64_close(remote_handle64 h) {
    int dsp_id = -1;

    fastrpc_init();

    struct session *sess = get_session_from_handle(h);
    if (!sess) {
        LOG_ERR("Failed to get session from handle: %llx", h);
        return AEE_EINVALIDPARAM;
    }
    dsp_id = sess->dsp->dsp_id;
    
    session_remove_module(sess, h);
    session_deinit(sess);

    dsp_deinit(dsp_id);
    // LOG_INF("Successfully closed remote handle: %llx", h);
    return AEE_SUCCESS;
}

int remote_handle64_control(remote_handle64 h, uint32_t req, void* data, uint32_t datalen) {
    fastrpc_init();
    LOG_INF("Called remote_handle64_control with handle: %llx, req: %u", h, req);
    
    return AEE_ENOTSUPPORTED;
}

int remote_session_control(uint32_t req, void *data, uint32_t datalen) {
    fastrpc_init();

    if (!data || datalen <= 0) {
        LOG_ERR("Invalid data or data length");
        return AEE_EINVALIDPARAM;
    }

    if(req == DSPRPC_CONTROL_UNSIGNED_MODULE) {
        global_configure("fastrpc.pd.type", data, datalen);
    } else if(req == FASTRPC_THREAD_PARAMS) {
        global_configure("fastrpc.thread.priority", data, datalen);
        global_configure("fastrpc.thread.stacksize", data, datalen);
    } else if(req == FASTRPC_CONTROL_PD_DUMP) {
        global_configure("fastrpc.pd.dump", data, datalen);
    } else if (req == FASTRPC_PD_INITMEM_SIZE) {
        global_configure("fastrpc.pd.initmem.size", data, datalen);
    } else {
        LOG_ERR("Invalid request passed %d", req);
        return AEE_ENOTSUPPORTED;
    }
    LOG_INF("Successfully configured session with handle: %p, req: %u", data, req);
    return AEE_SUCCESS;
}
