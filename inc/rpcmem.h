// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __RPCMEM_H__
#define __RPCMEM_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <pthread.h>
#include "AEEQList.h"
#include "error.h"
#include "log.h"
#include "AEEstd.h"

struct rpcmem {
    int fd;
    void *ptr;
    size_t size;
    uint32_t offset;
    uint32_t attr;
    int ref_count;
    QNode n;
};

struct dsp_mem {
    pthread_mutex_t lock;
    QList allocations;
};

/**
 * @brief Initialize the Global Memory Tracker.
 *
 * This function initializes the global memory tracker by setting up the mutex
 * and the allocations list. It ensures that the global memory tracker is ready
 * for managing DMA memory allocations.
 *
 * @details
 * This function is thread-safe and should be called once during the initialization
 * phase of the application.
 *
 * @return void
 */
void rpcmem_init(void);

/**
 * @brief Allocate DMA Memory.
 *
 * This function allocates DMA memory of the specified size. It creates a new
 * `rpcmem` structure to track the allocation and adds it to the global memory tracker.
 *
 * @details
 * This function is thread-safe and ensures that the memory allocation is properly
 * tracked and managed.
 *
 * @param size The size of the memory to be allocated.
 *
 * @return A pointer to the allocated memory, or NULL if the allocation fails.
 *
 * @retval NULL if memory allocation fails.
 */
void *rpcmem_alloc(size_t size);

/**
 * @brief Free DMA Memory.
 *
 * This function frees the specified DMA memory. It removes the corresponding
 * `rpcmem` structure from the global memory tracker and deallocates the memory.
 *
 * @details
 * This function is thread-safe and ensures that the memory deallocation is properly
 * tracked and managed.
 *
 * @param ptr A pointer to the memory to be freed.
 *
 * @return void
 */
void rpcmem_free(void *ptr);

#endif // __RPCMEM_H__