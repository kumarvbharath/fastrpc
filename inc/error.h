// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef __FASTRPC_ERROR_H__
#define __FASTRPC_ERROR_H__

/**
 * @brief FastRPC Error Codes
 * Error codes returned by FastRPC functions
 */

/** Operation completed successfully */
#define AEE_SUCCESS              0x0000

/** General failure, operation did not complete */
#define AEE_EFAILED             0x0001

/** Memory allocation failed */
#define AEE_ENOMEM              0x0002

/** Operation failed due to active sessions */
#define AEE_EACTIVESESSIONS     0x0007

/** Invalid parameter provided to function */
#define AEE_EINVALIDPARAM       0x000E

/** Operation cannot be performed in current state */
#define AEE_EINVALIDSTATE       0x000D

/** Requested operation is not supported */
#define AEE_ENOTSUPPORTED       0x0014


#endif /* __FASTRPC_ERROR_H__ */