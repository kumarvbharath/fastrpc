// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file mem.c
 * @brief FastRPC Memory Management Implementation
 *
 * This file implements the memory management functionality for FastRPC, providing:
 * - Remote memory mapping and unmapping
 * - Buffer registration for remote access
 * - DMA handle registration
 * - File descriptor registration
 * - Memory attribute management
 *
 * Key features:
 * - Thread-safe memory operations
 * - Support for various memory registration methods
 * - Integration with session-based memory management
 * - DMA buffer handling
 * - Attribute-based memory configuration
 *
 * Memory Registration Methods:
 * - Direct memory mapping (fastrpc_mmap/munmap)
 * - Buffer registration (remote_register_buf*)
 * - DMA handle registration (remote_register_dma_handle*)
 * - File descriptor registration (remote_register_fd*)
 *
 * Limitations:
 * - No support for HLOS-side cache operations
 * - Limited error recovery for mapping failures
 * - No automatic buffer cleanup on process exit
 * - Maximum buffer size limited by system memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AEEQList.h"
#include "remote.h"
#include "fastrpc.h"
#include "rpcmem.h"
#include "log.h"

struct rpcmem* create_rpcmem(void *addr, size_t length, int fd, int offset, enum fastrpc_map_flags flags) {
    struct rpcmem *mem = malloc(sizeof(struct rpcmem));
    if (!mem) {
        LOG_ERR("Failed to allocate memory for rpcmem");
        return NULL;
    }

    mem->ptr = addr;
    mem->size = length;
    mem->fd = fd;
    mem->offset = offset;
    mem->attr = flags;

    return mem;
}

void destroy_rpcmem(struct rpcmem *mem) {
    if (mem) {
        free(mem);
    }
}

int fastrpc_mmap(int domain, int fd, void *addr, int offset, size_t length, enum fastrpc_map_flags flags) {
    fastrpc_init();

    int dom_id = GET_DOMAIN_ID(domain);
    int sess_id = GET_SESSION_ID(domain);

    LOG_INF("Called fastrpc_mmap with domain: %d, session %d fd: %d, addr: %p, offset: %d, length: %zu, flags: %d", domain, sess_id, fd, addr, offset, length, flags);

    struct dsp *dsp = dsp_init(dom_id);
    if (!dsp) {
        LOG_ERR("Failed to initialize DSP for domain: %d", dom_id);
        return AEE_EFAILED;
    }

    struct session *sess = get_session(dsp, sess_id);
    if (!sess) {
        LOG_ERR("Failed to initialize session for domain: %d", dom_id);
        dsp_deinit(dom_id);
        return AEE_EFAILED;
    }

    struct rpcmem mem;
    mem.ptr = addr;
    mem.size = length;
    mem.fd = fd;
    mem.offset = offset;
    mem.attr = flags;

    void *ptr = session_map_memory(sess, &mem);
    if (!ptr) {
        LOG_ERR("Failed to map memory for domain: %d, sess id: %d fd: %d, addr: %p, offset: %d, length: %zu, flags: %d", dom_id, sess_id, fd, addr, offset, length, flags);
        session_deinit(sess);
        dsp_deinit(dom_id);
        return AEE_EINVALIDPARAM;
    }

    put_session(sess);
    
    LOG_INF("Successfully mapped memory for domain: %d, sess id: %d fd: %d, addr: %p, offset: %d, length: %zu, flags: %d", dom_id, sess_id, fd, addr, offset, length, flags);
    return AEE_SUCCESS;
}

int fastrpc_munmap(int domain, int fd, void *addr, size_t length) {
    fastrpc_init();
    LOG_INF("Called fastrpc_munmap with domain: %d, fd: %d, addr: %p, length: %zu", domain, fd, addr, length);

    int dom_id = GET_DOMAIN_ID(domain);
    int sess_id = GET_SESSION_ID(domain);

    struct dsp *dsp = dsp_init(dom_id);
    if (!dsp) {
        LOG_ERR("Failed to initialize DSP for domain: %d", dom_id);
        return AEE_EFAILED;
    }

    struct session *sess = get_session(dsp, sess_id);
    if (!sess) {
        LOG_ERR("Failed to initialize session for domain: %d", dom_id);
        dsp_deinit(dom_id);
        return AEE_EFAILED;
    }

    struct rpcmem mem;
    mem.ptr = addr;
    mem.size = length;
    mem.fd = fd;

    session_unmap_memory(sess, &mem);

    put_session(sess);

    LOG_INF("Successfully unmapped memory for domain: %d, fd: %d, addr: %p, length: %zu", dom_id, fd, addr, length);
    return AEE_SUCCESS;
}

void remote_register_buf(void* buf, int size, int fd) {
    LOG_INF("Called remote_register_buf with buf: %p, size: %d, fd: %d", buf, size, fd);
    add_rpcmem_node(buf, size, fd, 0);
}

void remote_register_buf_attr(void* buf, int size, int fd, int attr) {
    LOG_INF("Called remote_register_buf_attr with buf: %p, size: %d, fd: %d, attr: %d", buf, size, fd, attr);
    add_rpcmem_node(buf, size, fd, attr);
}

void remote_register_buf_attr2(void* buf, size_t size, int fd, int attr) {
    LOG_INF("Called remote_register_buf_attr2 with buf: %p, size: %zu, fd: %d, attr: %d", buf, size, fd, attr);
    add_rpcmem_node(buf, size, fd, attr);
}

int remote_register_dma_handle(int fd, uint32_t len) {
    LOG_INF("Called remote_register_dma_handle with fd: %d, len: %u", fd, len);
    add_rpcmem_node(NULL, len, fd, 0);
    return 0;
}

int remote_register_dma_handle_attr(int fd, uint32_t len, uint32_t attr) {
    LOG_INF("Called remote_register_dma_handle_attr with fd: %d, len: %u, attr: %u", fd, len, attr);
    add_rpcmem_node(NULL, len, fd, attr);
    return 0;
}

void* remote_register_fd(int fd, int size) {
    LOG_INF("Called remote_register_fd with fd: %d, size: %d", fd, size);
    add_rpcmem_node(NULL, size, fd, 0);
    return NULL;
}

void* remote_register_fd2(int fd, size_t size) {
    LOG_INF("Called remote_register_fd2 with fd: %d, size: %zu", fd, size);
    add_rpcmem_node(NULL, size, fd, 0);
    return NULL;
}