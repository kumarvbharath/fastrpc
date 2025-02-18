// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "remote.h"
#include "error.h"

int remote_handle_open(const char* name, remote_handle *ph) {
    printf("Called remote_handle_open with name: %s\n", name);
    return AEE_ENOTSUPPORTED;
}

int remote_handle_invoke(remote_handle h, uint32_t dwScalars, remote_arg *pra) {
    printf("Called remote_handle_invoke with handle: %u\n", h);
    return AEE_ENOTSUPPORTED;
}

int remote_handle_close(remote_handle h) {
    printf("Called remote_handle_close with handle: %u\n", h);
    return AEE_ENOTSUPPORTED;
}

int remote_handle_control(uint32_t req, void* data, uint32_t datalen) {
    printf("Called remote_handle_control with req: %u\n", req);
    return AEE_ENOTSUPPORTED;
}

int remote_handle_invoke_async(remote_handle h, fastrpc_async_descriptor_t *desc, uint32_t dwScalars, remote_arg *pra) {
    printf("Called remote_handle_invoke_async with handle: %u\n", h);
    return AEE_ENOTSUPPORTED;
}

int remote_mmap(int fd, uint32_t flags, uint32_t vaddrin, int size, uint32_t* vaddrout) {
    printf("Called remote_mmap with fd: %d\n", fd);
    return AEE_ENOTSUPPORTED;
}

int remote_munmap(uint32_t vaddrout, int size) {
    printf("Called remote_munmap with vaddrout: %u\n", vaddrout);
    return AEE_ENOTSUPPORTED;
}

int remote_mem_map(int domain, int fd, int flags, uint64_t virtAddr, size_t size, uint64_t* remoteVirtAddr) {
    printf("Called remote_mem_map with domain: %d\n", domain);
    return AEE_ENOTSUPPORTED;
}

int remote_mem_unmap(int domain, uint64_t remoteVirtAddr, size_t size) {
    printf("Called remote_mem_unmap with domain: %d\n", domain);
    return AEE_ENOTSUPPORTED;
}

int remote_mmap64(int fd, uint32_t flags, uint64_t vaddrin, int64_t size, uint64_t* vaddrout) {
    printf("Called remote_mmap64 with fd: %d\n", fd);
    return AEE_ENOTSUPPORTED;
}

int remote_munmap64(uint64_t vaddrout, int64_t size) {
    printf("Called remote_munmap64 with vaddrout: %llu\n", vaddrout);
    return AEE_ENOTSUPPORTED;
}
