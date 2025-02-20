// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file remote_internal.h
 * @brief Internal Remote Interface for FastRPC DSP Communication
 *
 * Provides:
 * - Internal callback interfaces for DSP communication
 * - Remote invocation support functions
 * - Domain-specific event handling
 * - Session management utilities
 *
 * This header defines internal interfaces that should not be used directly
 * by applications. For public APIs, see remote.h.
 *
 * Thread Safety:
 * - All callbacks are thread-safe
 * - Can be called from multiple concurrent sessions
 *
 */

#ifndef __REMOTE_INTERNAL_H__
#define __REMOTE_INTERNAL_H__

/**
 * @brief DSP event callback handler
 *
 * @param event Event type (INIT, DEINIT, etc)
 * @param ctx Context pointer passed during registration
 * @param data Event-specific data
 * @param retVal Pointer to store operation result
 * @return Callback-specific return value
 */
void* remote_dsp_callback(int event, void *ctx, void *data, int *retVal);

#endif // __REMOTE_INTERNAL_H__