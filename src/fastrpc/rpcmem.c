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

#define DMA_DEVICE "/dev/dma_buf"

// Global memory tracker
static struct dsp_mem g_mem;
static int g_initialized = 0;

// Update heap operations to match new function signature
static void* heap_alloc(size_t size, uint32_t flags, int *fd) {
    LOG_INF("Heap allocation: size=%zu flags=0x%x", size, flags);
    *fd = -1; // Heap allocations don't use fd
    return malloc(size);
}

static void heap_free(void* ptr, int fd, size_t size) {
    LOG_INF("Heap free: ptr=%p size=%zu", ptr, size);
    free(ptr);
}

// Fix DMA operations
static void* dma_alloc(size_t size, uint32_t flags, int *fd) {
    LOG_INF("DMA allocation: size=%zu flags=0x%x", size, flags);
    
    *fd = open(DMA_DEVICE, O_RDWR);
    if (*fd < 0) {
        LOG_ERR("Failed to open DMA device: %s", strerror(errno));
        return NULL;
    }

    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (ptr == MAP_FAILED) {
        LOG_ERR("Failed to map DMA buffer: %s", strerror(errno));
        close(*fd);
        *fd = -1;
        return NULL;
    }

    return ptr;
}

static void dma_free(void* ptr, int fd, size_t size) {
    LOG_INF("DMA free: ptr=%p fd=%d size=%zu", ptr, fd, size);
    
    if (munmap(ptr, size) != 0) {
        LOG_ERR("Failed to unmap DMA buffer: %s", strerror(errno));
    }
    
    if (fd >= 0) {
        if (close(fd) != 0) {
            LOG_ERR("Failed to close DMA fd %d: %s", fd, strerror(errno));
        }
    }
}

// Select memory operations based on compilation flag
#ifdef USE_DMA_ALLOCATION
static struct mem_ops mem_ops = {
    .alloc = dma_alloc,
    .free = dma_free
};
#else
static struct mem_ops mem_ops = {
    .alloc = heap_alloc,
    .free = heap_free
};
#endif

/* Initialize Global Memory Tracker */
void rpcmem_init(void) {
    LOG_INF("Initializing RPC memory subsystem");
    if (g_initialized) {
        LOG_ERR("RPC memory subsystem already initialized");
        return;
    }

    pthread_mutex_init(&g_mem.lock, NULL);
    QList_Ctor(&g_mem.allocations);
    g_initialized = 1;
    LOG_INF("RPC memory subsystem initialized");
}

void rpcmem_deinit(void) {
    LOG_INF("Deinitializing RPC memory subsystem");
    if (!g_initialized) {
        LOG_ERR("RPC memory subsystem not initialized");
        return;
    }

    LOG_INF("Locking for deinitialization");
    pthread_mutex_lock(&g_mem.lock);
    LOG_INF("Lock aquired for deinitialization");
    if (!QList_IsEmpty(&g_mem.allocations)) {
        LOG_INF("RPC memory subsystem deinitialized");
        pthread_mutex_unlock(&g_mem.lock);
        pthread_mutex_destroy(&g_mem.lock);
        g_initialized = 0;
        return;
    }

    LOG_INF("Freeing all unfreed buffers");
    QNode *node, *next;
    QLIST_NEXTSAFE_FOR_ALL(&g_mem.allocations, node, next) {
        struct rpcmem *mem = STD_RECOVER_REC(struct rpcmem, n, node);
        LOG_INF("Freeing unfreed buffer: ptr=%p size=%zu", mem->ptr, mem->size);
        mem->free_fn(mem->ptr, mem->fd, mem->size);  // Use stored free function
        QNode_DequeueZ(&mem->n);
        free(mem);
    }
    pthread_mutex_unlock(&g_mem.lock);
    pthread_mutex_destroy(&g_mem.lock);
    g_initialized = 0;
    LOG_INF("RPC memory subsystem deinitialized");
    return;
}

// Update the allocation function to use function pointers
void* rpcmem_alloc(int heapid, uint32_t flags, int size) {
    LOG_INF("Allocating memory: size=%d flags=0x%x", size, flags);
    
    if(!g_initialized) {
        LOG_ERR("RPC memory subsystem not initialized");
        errno = AEE_EINVALIDSTATE;
        return NULL;
    }
    
    if (size <= 0) {
        LOG_ERR("Invalid size requested: %d", size);
        errno = AEE_EINVALIDPARAM;
        return NULL;
    }

    pthread_mutex_lock(&g_mem.lock);
    int fd;
    void *ptr = mem_ops.alloc(size, flags, &fd);
    if (!ptr) {
        LOG_ERR("Memory allocation failed");
        pthread_mutex_unlock(&g_mem.lock);
        errno = AEE_ENOMEM;
        return NULL;
    }

    struct rpcmem *mem = malloc(sizeof(struct rpcmem));
    if (!mem) {
        LOG_ERR("Failed to allocate rpcmem structure");
        mem_ops.free(ptr, fd, size);
        pthread_mutex_unlock(&g_mem.lock);
        errno = AEE_ENOMEM;
        return NULL;
    }

    mem->ptr = ptr;
    mem->size = size;
    mem->fd = fd;
    mem->attr = flags;
    mem->ref = 1;
    mem->free_fn = mem_ops.free;
    QNode_CtorZ(&mem->n);
    QList_AppendNode(&g_mem.allocations, &mem->n);

    LOG_INF("Allocated memory: ptr=%p size=%d fd=%d", ptr, size, fd);
    pthread_mutex_unlock(&g_mem.lock);
    return ptr;
}

void* rpcmem_alloc2(int heapid, uint32_t flags, size_t size) {
    if (size > INT_MAX) {
        LOG_ERR("Size exceeds maximum supported: %zu", size);
        return NULL;
    }
    return rpcmem_alloc(heapid, flags, (int)size);
}

// Update the free function to use function pointers
void rpcmem_free(void* po) {

    if (!g_initialized || !po) {
        LOG_ERR("Invalid free request: initialized=%d ptr=%p", g_initialized, po);
        return;
    }

    pthread_mutex_lock(&g_mem.lock);
    if(QList_IsEmpty(&g_mem.allocations)) {
        pthread_mutex_unlock(&g_mem.lock);
        LOG_ERR("No active allocations to free");
        return;
    }
    QNode *node, *next;
    QLIST_NEXTSAFE_FOR_ALL(&g_mem.allocations, node, next) {
        struct rpcmem *mem = STD_RECOVER_REC(struct rpcmem, n, node);
        if (mem->ptr == po) {
            if (--mem->ref == 0) {
                LOG_INF("Freeing memory: ptr=%p size=%zu", mem->ptr, mem->size);
                mem->free_fn(mem->ptr, mem->fd, mem->size);
                QNode_DequeueZ(&mem->n);
                free(mem);
            }
            pthread_mutex_unlock(&g_mem.lock);
            return;
        }
    }

    LOG_ERR("Attempt to free unregistered pointer: %p", po);
    pthread_mutex_unlock(&g_mem.lock);
}

int rpcmem_to_fd(void* po) {
    if (!g_initialized) {
        LOG_ERR("RPC memory subsystem not initialized");
        errno = AEE_EINVALIDSTATE;
        return -1;
    }

    if (!po) {
        LOG_ERR("NULL pointer passed to rpcmem_to_fd");
        errno = AEE_EINVALIDPARAM;
        return -1;
    }

    pthread_mutex_lock(&g_mem.lock);
    
    if (QList_IsEmpty(&g_mem.allocations)) {
        LOG_ERR("Empty allocation list");
        pthread_mutex_unlock(&g_mem.lock);
        errno = AEE_EINVALIDSTATE;
        return -1;
    }

    QNode *node, *next;
    QLIST_NEXTSAFE_FOR_ALL(&g_mem.allocations, node, next) {
        struct rpcmem *mem = STD_RECOVER_REC(struct rpcmem, n, node);
        if (mem->ptr == po) {
            int fd = mem->fd;
            LOG_INF("Found fd %d for buffer %p", fd, po);
            pthread_mutex_unlock(&g_mem.lock);
            return fd;
        }
    }

    LOG_ERR("No fd found for pointer: %p", po);
    pthread_mutex_unlock(&g_mem.lock);
    errno = AEE_EINVALIDPARAM;
    return -1;
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
    new_node->ref = 1;
    QNode_CtorZ(&new_node->n);
    QList_AppendNode(&g_mem.allocations, &new_node->n);

    LOG_INF("Added rpcmem node with buf: %p, size: %zu, fd: %d, attr: %u", buf, size, fd, attr);
    pthread_mutex_unlock(&g_mem.lock);
}