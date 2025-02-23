/**
 * @file fops.c
 * @brief Thread-safe File Operations Wrapper using Dynamic List
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "error.h"
#include "log.h"

#define INVALID_HANDLE  (-1)
#define MIN_BUF_SIZE    4096

typedef enum {
    FOPS_READ = 1,
    FOPS_WRITE = 2,
    FOPS_APPEND = 4,
    FOPS_CREATE = 8,
    FOPS_TRUNC = 16
} fops_mode_t;

typedef struct fops_handle {
    int handle_id;           // Unique handle identifier
    int fd;                  // File descriptor
    fops_mode_t mode;       // Access mode
    void* buffer;           // I/O buffer
    size_t buf_size;        // Buffer size
    pthread_mutex_t lock;    // Per-handle lock
    struct fops_handle *next;
} fops_handle_t;

typedef struct {
    pthread_mutex_t global_lock;
    fops_handle_t *head;     // Head of handle list
    int next_handle;         // Next available handle ID
    int initialized;
} fops_context_t;

static fops_context_t g_fops_ctx = {
    .global_lock = PTHREAD_MUTEX_INITIALIZER,
    .head = NULL,
    .next_handle = 1,
    .initialized = 0
};

/**
 * @brief Get handle from handle ID
 */
static fops_handle_t* get_handle(int handle_id) {
    fops_handle_t *current = g_fops_ctx.head;
    while (current) {
        if (current->handle_id == handle_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * @brief Add new handle to list
 */
static fops_handle_t* add_handle(int fd, fops_mode_t mode) {
    fops_handle_t *handle = calloc(1, sizeof(fops_handle_t));
    if (!handle) {
        LOG_ERR("Failed to allocate handle");
        return NULL;
    }

    handle->handle_id = g_fops_ctx.next_handle++;
    handle->fd = fd;
    handle->mode = mode;
    handle->buffer = malloc(MIN_BUF_SIZE);
    handle->buf_size = MIN_BUF_SIZE;
    pthread_mutex_init(&handle->lock, NULL);

    // Add to list
    handle->next = g_fops_ctx.head;
    g_fops_ctx.head = handle;

    return handle;
}

/**
 * @brief Remove handle from list
 */
static void remove_handle(int handle_id) {
    fops_handle_t *current = g_fops_ctx.head;
    fops_handle_t *prev = NULL;

    while (current) {
        if (current->handle_id == handle_id) {
            if (prev) {
                prev->next = current->next;
            } else {
                g_fops_ctx.head = current->next;
            }
            pthread_mutex_destroy(&current->lock);
            free(current->buffer);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

int fops_open(const char* path, const char* mode, int* handle) {
    if (!path || !mode || !handle) {
        LOG_ERR("Invalid parameters");
        return AEE_EINVALIDPARAM;
    }

    pthread_mutex_lock(&g_fops_ctx.global_lock);

    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        pthread_mutex_unlock(&g_fops_ctx.global_lock);
        LOG_ERR("Failed to open file: %s", strerror(errno));
        return AEE_EFILE;
    }

    fops_handle_t *h = add_handle(fd, str_to_mode(mode));
    if (!h) {
        close(fd);
        pthread_mutex_unlock(&g_fops_ctx.global_lock);
        return AEE_ENOMEMORY;
    }

    *handle = h->handle_id;
    pthread_mutex_unlock(&g_fops_ctx.global_lock);
    
    LOG_INF("Opened file: path=%s handle=%d", path, h->handle_id);
    return AEE_SUCCESS;
}

void fops_deinit(void) {
    pthread_mutex_lock(&g_fops_ctx.global_lock);
    
    while (g_fops_ctx.head) {
        fops_handle_t *next = g_fops_ctx.head->next;
        close(g_fops_ctx.head->fd);
        pthread_mutex_destroy(&g_fops_ctx.head->lock);
        free(g_fops_ctx.head->buffer);
        free(g_fops_ctx.head);
        g_fops_ctx.head = next;
    }

    g_fops_ctx.initialized = 0;
    pthread_mutex_unlock(&g_fops_ctx.global_lock);
    LOG_INF("File operations deinitialized");
}