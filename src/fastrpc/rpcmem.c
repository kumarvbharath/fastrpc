// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "rpcmem.h"

/**
 * @file rpcmem.c
 * @brief DMA Memory Management for FastRPC
 *
 * This file implements the management of DMA (Direct Memory Access) memory
 * allocations and tracking using a global memory tracker. It includes functions
 * for initializing the memory tracker, allocating and freeing DMA memory, and
 * adding memory nodes.
 *
 * @details
 * The program ensures thread safety using mutexes and provides a structured way
 * to manage DMA memory allocations and tracking.
 *
 */

struct dsp_mem g_mem;

/* Initialize Global Memory Tracker */
void rpcmem_init(void)
{
    pthread_mutex_init(&g_mem.lock, NULL);
    QList_Ctor(&g_mem.allocations);
}

/* Allocate DMA Memory */
void *rpcmem_alloc(size_t size)
{
    struct rpcmem *alloc;
    void *ptr = malloc(size); // Dummy allocation, should be replaced with smmu allocation

    if (!ptr) {
        return NULL;
    }

    alloc = malloc(sizeof(*alloc));
    if (!alloc) {
        free(ptr);
        return NULL;
    }

    alloc->ptr = ptr;
    alloc->size = size;
    alloc->ref_count = 1;
    QNode_CtorZ(&alloc->n);

    pthread_mutex_lock(&g_mem.lock);
    QList_AppendNode(&g_mem.allocations, &alloc->n);
    pthread_mutex_unlock(&g_mem.lock);

    return ptr;
}

/* Free DMA Memory */
void rpcmem_free(void *ptr)
{
    struct rpcmem *alloc = NULL;
    QNode *node;

    if (!ptr) {
        return;
    }

    pthread_mutex_lock(&g_mem.lock);
    QLIST_FOR_ALL(&g_mem.allocations, node) {
        alloc = (struct rpcmem *)node;
        if (alloc->ptr == ptr) {
            QNode_Dequeue(&alloc->n);
            free(alloc->ptr);
            free(alloc);
            break;
        }
    }
    pthread_mutex_unlock(&g_mem.lock);
}

/* Add RPC Memory Node, could be used when clients already have an fd */
void add_rpcmem_node(void *buf, size_t size, int fd, uint32_t attr) {
    pthread_mutex_lock(&g_mem.lock);

    // Check if the buffer is already in the list
    QNode *node;
    QLIST_FOR_ALL(&g_mem.allocations, node) {
        struct rpcmem *mem = STD_RECOVER_REC(struct rpcmem, n, node);
        if (mem->ptr == buf && mem->fd == fd) {
            LOG_INF("Buffer already registered: buf: %p, size: %zu, fd: %d, attr: %u", buf, size, fd, attr);
            pthread_mutex_unlock(&g_mem.lock);
            return;
        }
    }

    // Add new node to the list
    struct rpcmem *new_node = (struct rpcmem *)malloc(sizeof(struct rpcmem));
    if (!new_node) {
        LOG_ERR("Failed to allocate memory for rpcmem node");
        pthread_mutex_unlock(&g_mem.lock);
        return;
    }

    new_node->ptr = buf;
    new_node->size = size;
    new_node->fd = fd;
    new_node->attr = attr;
    new_node->ref_count = 1;
    QNode_CtorZ(&new_node->n);
    QList_AppendNode(&g_mem.allocations, &new_node->n);

    LOG_INF("Added rpcmem node with buf: %p, size: %zu, fd: %d, attr: %u", buf, size, fd, attr);
    pthread_mutex_unlock(&g_mem.lock);
}