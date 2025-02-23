// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __REMOTECTL_H__
#define __REMOTECTL_H__

/**
 * @file remotectl.h
 * @brief Remote Control Interface for DSP Library Management
 *
 * This file provides the interface for loading and managing libraries on the DSP.
 * The remotectl module is responsible for:
 * - Dynamic loading of shared libraries on DSP
 * - Module handle management
 * - Remote heap management
 * - Parameter configuration for loaded modules
 *
 * Implementation Notes:
 * - The module handle for remotectl is always 0 (predefined)
 * - remotectl_open/close are dummy calls and not used in production
 * - Use remotectl_open1/close1 for actual library loading operations
 *
 * Example Usage:
 * @code
 * remote_handle64 handle = 0; // Predefined handle
 * int module_handle;
 * char error_buf[256];
 * int error;
 *
 * // Load a library on DSP
 * ret = remotectl1_open1(handle, "libdsp.so", &module_handle,
 *                       error_buf, sizeof(error_buf), &error);
 * if (ret != AEE_SUCCESS) {
 *     // Handle error
 * }
 * @endcode
 */

#include <string.h>
#include <inttypes.h>
#include "fastrpc.h"
#include "remote64.h"
#include "error.h"
#include "log.h" 

//should be 0
#define REMOTECTL_HANDLE 8

/**
 * @brief Dummy open function - not used in production
 *
 * This function is maintained for backward compatibility but should not be used.
 * The handle for remotectl module is always 0.
 *
 * @param uri Unused parameter
 * @param h Pointer to store handle (unused)
 * @return Always returns AEE_SUCCESS
 */
int remotectl1_open(const char* uri, remote_handle64* h);

/**
 * @brief Dummy close function - not used in production
 *
 * This function is maintained for backward compatibility but should not be used.
 *
 * @param h Handle to close (unused)
 * @return Always returns AEE_SUCCESS
 */
int remotectl1_close(remote_handle64 h);

/**
 * @brief Grow the DSP heap
 *
 * Increases the DSP heap size by allocating additional memory at the specified
 * physical address.
 *
 * @param _handle Remote handle (must be 0)
 * @param phyAddr Physical address for new heap memory
 * @param nSize Size to grow heap by in bytes
 * @return AEE_SUCCESS on success, error code otherwise
 * @note Thread-safe
 */
int remotectl1_grow_heap(remote_handle64 _handle, uint32_t phyAddr, uint32_t nSize);

/**
 * @brief Set parameters for a loaded module
 *
 * Configure parameters for a previously loaded module.
 *
 * @param _handle Remote handle (must be 0)
 * @param reqID Request ID for parameter configuration
 * @param params Array of parameters to set
 * @param paramsLen Number of parameters in array
 * @return AEE_SUCCESS on success, error code otherwise
 * @note Thread-safe
 */
int remotectl1_set_param(remote_handle64 _handle, int reqID,
                        const uint32_t* params, int paramsLen);


void *remotectl_dsp_callback(int event, void **ctx, void *data, int *retVal);
#endif // __REMOTECTL_H__