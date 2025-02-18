// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef __FASTRPC_CONFIG_H__
#define __FASTRPC_CONFIG_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "remote.h"
#include "log.h"
#include "uthash.h"

typedef struct config_entry {
    char key[32];            // Key for the config entry
    void *value;             // Value for the config entry
    UT_hash_handle hh;       // Makes this structure hashable
} config_entry_t;

struct config_store {
    config_entry_t *entries;
    pthread_mutex_t mutex;
};

typedef struct config_store config_store_t;

/**
 * @brief Creates a new configuration store
 * 
 * Allocates and initializes a new configuration store with an empty hash table
 * and initialized mutex for thread safety.
 * 
 * @return Pointer to newly created config store, NULL if allocation fails
 * @thread_safety Thread-safe, uses internal mutex
 */
config_store_t *config_store_create();

/**
 * @brief Destroys a configuration store
 * 
 * Frees all entries in the store, their associated values, and the store itself.
 * Also destroys the mutex used for synchronization.
 * 
 * @param store Pointer to config store to destroy
 * @thread_safety Thread-safe, uses internal mutex
 * @note All stored values must have been dynamically allocated
 */
void config_store_destroy(config_store_t *store);

/**
 * @brief Sets a configuration value in the store
 * 
 * Adds a new key-value pair or updates an existing one in the config store.
 * The key is copied but the value pointer is stored as-is.
 * 
 * @param store Pointer to config store
 * @param key Configuration key (max 31 characters)
 * @param value Pointer to configuration value
 * @return AEE_SUCCESS on success, AEE_ENOMEM if memory allocation fails
 * @thread_safety Thread-safe, uses internal mutex
 * @note Caller retains ownership of value memory
 */
int config_store_set(config_store_t *store, const char *key, void *value);

/**
 * @brief Retrieves a configuration value from the store
 * 
 * Looks up the value associated with the given key in the config store.
 * 
 * @param store Pointer to config store
 * @param key Configuration key to look up
 * @return Pointer to stored value if found, NULL if key doesn't exist
 * @thread_safety Thread-safe, uses internal mutex
 * @note Returned pointer remains owned by the config store
 */
void *config_store_get(config_store_t *store, const char *key);

#endif // __FASTRPC_CONFIG_H__