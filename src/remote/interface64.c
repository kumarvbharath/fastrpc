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

__thread struct session *local_session = NULL;

typedef struct {
    char *sofilename;
    char *interface;
    int pdtype;
    int domain;
    int session;
} uri_info_t;

// Static URI mapping structure
typedef struct {
    const char *uri;
    int domain;
    int session;
    const char *sofile;
} static_uri_map_t;

// Static URI mappings table
static const static_uri_map_t static_uris[] = {
    {"createstaticpd",  3, 0, "audiopd"},
    {"geteventfd",      0, 0, "listener"},
    {"attachguestos",   0, 0, "GuestOS"},
    {"attachaudiopd",   0, 0, "audiopd"},
    {"attachsensorpd",  0, 0, "sensorpd"},
    {NULL, -1, -1, NULL}  // Terminator
};

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

// Helper function to free uri_info structure
static void free_uri_info(uri_info_t *info) {
    if (info) {
        free(info->sofilename);
        free(info->interface);
        free(info);
    }
}

// Helper function to allocate and initialize uri_info
static uri_info_t* create_uri_info(void) {
    uri_info_t *info = calloc(1, sizeof(uri_info_t));
    if (!info) {
        LOG_ERR("Failed to allocate uri_info structure");
        return NULL;
    }
    info->domain = -1;
    info->session = -1;
    return info;
}

// Parse static URIs with predefined configurations
uri_info_t* parse_static_uri(const char* uri) {
    LOG_DBG("Enter: parse_static_uri(uri=%s)", uri ? uri : "NULL");
    
    if (!uri) {
        LOG_ERR("Static URI parser received NULL uri");
        return NULL;
    }

    // Look up in static mapping table
    for (const static_uri_map_t *map = static_uris; map->uri != NULL; map++) {
        if (strcmp(uri, map->uri) == 0) {
            uri_info_t *info = create_uri_info();
            if (!info) {
                return NULL;
            }

            info->domain = map->domain;
            info->session = map->session;
            info->sofilename = strdup(map->sofile);
            info->interface = strdup(uri);

            if (!info->sofilename || !info->interface) {
                LOG_ERR("Memory allocation failed for static URI components: sofile=%s, interface=%s",
                        map->sofile, uri);
                free_uri_info(info);
                return NULL;
            }

            LOG_INF("Static URI parsed: uri=%s, domain=%d, session=%d, sofile=%s",
                   uri, info->domain, info->session, info->sofilename);
            return info;
        }
    }

    LOG_DBG("Exit: parse_static_uri - URI not found in static mappings: %s", uri);
    return NULL;
}

// Helper function to parse and validate token
static int parse_token(char *token, int token_num, uri_info_t *info) {
    LOG_DBG("Parsing token %d: %s", token_num, token);
    
    switch (token_num) {
        case 0: // domain
            info->domain = atoi(token);
            if (info->domain < 0) {
                LOG_ERR("Invalid domain value: %s", token);
                return -1;
            }
            break;
            
        case 1: // session
            info->session = atoi(token);
            if (info->session < 0) {
                LOG_ERR("Invalid session value: %s", token);
                return -1;
            }
            break;
            
        case 2: // sofile
            info->sofilename = strdup(token);
            if (!info->sofilename) {
                LOG_ERR("Failed to allocate sofilename: %s", token);
                return -1;
            }
            break;
            
        case 3: // interface
            info->interface = strdup(token);
            if (!info->interface) {
                LOG_ERR("Failed to allocate interface: %s", token);
                return -1;
            }
            break;
    }
    return 0;
}

// Optimized generic URI parser
uri_info_t* parse_dynamic_uri(const char* uri) {
    LOG_DBG("Enter: parse_dynamic_uri(uri=%s)", uri ? uri : "NULL");

    if (!uri) {
        LOG_ERR("Generic URI parser received NULL uri");
        return NULL;
    }

    uri_info_t *info = create_uri_info();
    if (!info) {
        return NULL;
    }

    char *uri_copy = strdup(uri);
    if (!uri_copy) {
        LOG_ERR("Failed to duplicate URI string: %s", uri);
        free_uri_info(info);
        return NULL;
    }

    char *saveptr;
    char *token = strtok_r(uri_copy, ":", &saveptr);
    int token_count = 0;
    int result = 0;

    while (token && token_count < 4) {
        result = parse_token(token, token_count, info);
        if (result < 0) {
            LOG_ERR("Failed to parse token %d in URI: %s", token_count, uri);
            goto error;
        }
        token = strtok_r(NULL, ":", &saveptr);
        token_count++;
    }

    if (token_count != 4) {
        LOG_ERR("Invalid URI format: expected 4 components, got %d (uri=%s)", 
                token_count, uri);
        goto error;
    }

    free(uri_copy);
    LOG_INF("Generic URI parsed: domain=%d, session=%d, sofile=%s, interface=%s",
           info->domain, info->session, info->sofilename, info->interface);
    return info;

error:
    free(uri_copy);
    free_uri_info(info);
    LOG_DBG("Exit: parse_dynamic_uri - Failed to parse URI: %s", uri);
    return NULL;
}

int remote_handle64_open(const char* uri, remote_handle64 *ph) {
    if (!uri || !ph) {
        LOG_ERR("Invalid parameters: uri=%p, ph=%p", uri, ph);
        return AEE_EINVALIDPARAM;
    }

    LOG_INF("Opening remote handle for URI: %s", uri);
    fastrpc_init();

    // Try parsing static URI first, then fall back to generic URI
    uri_info_t *info = parse_static_uri(uri);
    if (!info) {
        info = parse_dynamic_uri(uri);
        if (!info) {
            LOG_ERR("Failed to parse URI: %s", uri);
            return AEE_EINVALIDPARAM;
        }
    }

    // Use cleanup goto label with single point of cleanup
    int result = AEE_EFAILED;
    void *dsp = NULL;
    struct session *sess = NULL;
    uintptr_t module_handle = 0;

    // Validate domain
    if (info->domain == -1) {
        LOG_ERR("Invalid domain in URI: %s", uri);
        goto cleanup;
    }

    // Initialize DSP
    dsp = dsp_init(info->domain);
    if (!dsp) {
        LOG_ERR("Failed to initialize DSP for domain: %d", info->domain);
        goto cleanup;
    }

    // Initialize session
    int session_id = (info->session == -1) ? 0 : info->session;
    sess = session_init(dsp, session_id);
    if (!sess) {
        LOG_ERR("Failed to initialize session for domain: %d, session: %d", 
                info->domain, session_id);
        goto cleanup;
    }

    // Register static modules once per session
    register_static_modules(sess);

    // Add module to session
    module_handle = session_add_module(sess, info->sofilename);
    if (!module_handle) {
        LOG_ERR("Failed to add module to session for domain: %d, session: %d",
                info->domain, session_id);
        goto cleanup;
    }

    if(!IS_STATIC_MODULE(module_handle)) {
        local_session = sess;
    }
    // Success path
    *ph = (remote_handle64)module_handle;
    result = AEE_SUCCESS;

cleanup:
    // Clean up allocated resources
    if (info) {
        free(info->sofilename);
        free(info->interface);
        free(info);
    }

    // Only clean up session and DSP if we failed
    if (result != AEE_SUCCESS) {
        if (sess) session_deinit(sess);
        if (dsp) dsp_deinit(info->domain);
    }

    return result;
}

int remote_handle64_invoke(remote_handle64 h, uint32_t dwScalars, remote_arg *pra) {
    fastrpc_init();

    LOG_INF("Invoking remote handle: %llx", h);
    struct session *sess = get_session_from_handle(h);
    if (!sess && IS_STATIC_MODULE(h)) {
        sess = local_session;
    }
    
    if (!sess) {
        LOG_ERR("Failed to get session from handle: %llx%s", h, 
               (IS_STATIC_MODULE(h) ? ", and local session is also NULL" : ""));
        return AEE_EINVALIDPARAM;
    }
    
    if (sess == local_session) {
        LOG_INF("Using local session %p[%d] for handle: %llx", 
               sess, sess->session_id, h);
    }

    get_session(sess->dsp, sess->session_id);

    int result = session_invoke(sess, h, dwScalars, pra);
    if (result != AEE_SUCCESS) {
        LOG_ERR("Failed to invoke session with handle: %llx, result: %d", h, result);
        return result;
    }

    put_session(sess);

    LOG_INF("Successfully invoked session with handle: %llx", h);
    return AEE_SUCCESS;
}

int remote_handle64_close(remote_handle64 h) {
    int dsp_id = -1;

    fastrpc_init();
    struct session *sess = get_session_from_handle(h);
    if (!sess && IS_STATIC_MODULE(h)) {
        sess = local_session;
    }
    
    if (!sess) {
        LOG_ERR("Failed to get session from handle: %llx%s", h, 
               (IS_STATIC_MODULE(h) ? ", and local session is also NULL" : ""));
        return AEE_EINVALIDPARAM;
    }
    
    if (sess == local_session) {
        LOG_INF("Using local session %p[%d] for handle: %llx", 
               sess, sess->session_id, h);
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
        struct session *sess = get_session_from_handle(h);
    if (!sess && IS_STATIC_MODULE(h)) {
        sess = local_session;
    }
    
    if (!sess) {
        LOG_ERR("Failed to get session from handle: %llx%s", h, 
               (IS_STATIC_MODULE(h) ? ", and local session is also NULL" : ""));
        return AEE_EINVALIDPARAM;
    }
    
    if (sess == local_session) {
        LOG_INF("Using local session %p[%d] for handle: %llx", 
               sess, sess->session_id, h);
    }
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
