// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "config.h"

/**
 * @file config.c
 * @brief Configuration Store Implementation
 *
 * This file implements a thread-safe key-value configuration store using
 * a hash table for efficient lookups. The store supports:
 * - Dynamic creation and destruction of config stores
 * - Thread-safe operations using mutex synchronization
 * - String keys with arbitrary pointer values
 * - Hash table based lookup using uthash
 *
 * Limitations:
 * - Maximum key length is 31 characters
 * - Values must be dynamically allocated
 * - No support for atomic operations
 * - No built-in serialization/deserialization
 *
 */
 
config_store_t *config_store_create() {
    LOG_INF("Creating new config store");
    config_store_t *store = (config_store_t *)malloc(sizeof(config_store_t));
    if (store) {
        store->entries = NULL;
        pthread_mutex_init(&store->mutex, NULL);
        LOG_INF("Config store %p created successfully", store);
    } else {
        LOG_ERR("Failed to allocate memory for config store");
    }
    return store;
}

void config_store_destroy(config_store_t *store) {
    if (store) {
        LOG_INF("Destroying config store");
        config_entry_t *entry, *tmp;
        pthread_mutex_lock(&store->mutex);
        HASH_ITER(hh, store->entries, entry, tmp) {
            LOG_INF("Removing config entry with key: %s", entry->key);
            HASH_DEL(store->entries, entry);
            free(entry->value); // Free the value if it was dynamically allocated
            free(entry);
        }
        pthread_mutex_unlock(&store->mutex);
        pthread_mutex_destroy(&store->mutex);
        free(store);
        LOG_INF("Config store destroyed");
    } else {
        LOG_ERR("Attempt to destroy NULL config store");
    }
}

int config_store_set(config_store_t *store, const char *key, void *value, size_t size) {
    if(!store || !key) {
        LOG_ERR("Invalid arguments");
        return AEE_EINVALIDPARAM;
    }
    LOG_INF("Store %p Setting config value for key: %s value: %p", store, key, value);
    pthread_mutex_lock(&store->mutex);

    config_entry_t *entry = NULL;
    HASH_FIND_STR(store->entries, key, entry);
    if (!entry) {
        entry = (config_entry_t *)malloc(sizeof(config_entry_t));
        if (entry == NULL) {
            LOG_ERR("Failed to allocate memory for new config entry");
            pthread_mutex_unlock(&store->mutex);
            return AEE_ENOMEM;
        }
        strncpy(entry->key, key, sizeof(entry->key) - 1);
        entry->key[sizeof(entry->key) - 1] = '\0';
        entry->value = calloc(1, size + 1);
        if (entry->value == NULL) {
            LOG_ERR("Failed to allocate memory for new config value");
            free(entry);
            pthread_mutex_unlock(&store->mutex);
            return AEE_ENOMEM;
        }
        memmove(entry->value, value, size);
        HASH_ADD_STR(store->entries, key, entry);
        LOG_INF("Created new config entry for key: %s, entry: %p value: %p [%d]", key, entry, entry->value, *(int*)(entry->value));
    } else if (!value || size == 0) {
        LOG_INF("Removing config entry for key: %s", key);
        free(entry->key); // Free the old key if it was dynamically allocated
        free(entry->value); // Free the old value if it was dynamically allocated
        free(entry);
    } else {
        LOG_INF("Updating config entry for key: %s", key);
    }
    
    pthread_mutex_unlock(&store->mutex);
    LOG_INF("Successfully set config value for key: %s", key);
    return AEE_SUCCESS;
}

void *config_store_get(config_store_t *store, const char *key) {
    void *value;
    LOG_INF("Getting config value for store %p key: %s", store, key);

    if(!store || !key) {
        LOG_ERR("Invalid arguments");
        return NULL;
    }
    pthread_mutex_lock(&store->mutex);

    config_entry_t *entry = NULL;
    HASH_FIND_STR(store->entries, key, entry);
    value = (entry != NULL) ? entry->value : NULL;
    pthread_mutex_unlock(&store->mutex);
    if (value) {
        LOG_INF("Found config value for key: %s %p [%d]", key, value, *(int*)(value));
    } else {
        LOG_ERR("No config value found for key: %s", key);
    }
    return value;
}

