#ifndef __FASTRPC_PROCBUF_H__
#define __FASTRPC_PROCBUF_H__

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "uthash.h"
#include "error.h"
#include "log.h"
#include "rpcmem.h"
#include "fastrpc.h"

#define PROC_BUF_SIZE       4096
#define PARAM_HEADER_SIZE   (sizeof(uint32_t) + sizeof(uint32_t))  // id + size
#define MAX_PARAMS          32
#define PARAM_ALIGN         4

// Parameter header structure
typedef struct __attribute__((packed)) {
    uint32_t id:8;
    uint32_t size:24;
} param_header_t;

typedef struct {
    void *session;          /* key */
    void *buf;             /* shared buffer */
    uint32_t size;         /* total size */
    uint32_t used;         /* bytes used */
    int fd;                /* buffer fd */
    pthread_mutex_t lock;  /* per-buffer lock */
    uint32_t param_count;  /* number of parameters */
    UT_hash_handle hh;     /* hash table handle */
} proc_buf_t;

// Public APIs
void* procbuf_dsp_callback(int event, void *ctx, void *dsp, int *retVal);
int procbuf_add_param(void *session, uint32_t id, const void *data, uint32_t size);
int procbuf_get_info(void *session, int *fd, void **addr, uint32_t *size);
int procbuf_add_param_ext(uint32_t ext_domain, uint32_t id, const void *data, uint32_t size);
void* procbuf_session_callback(int type, void *ctx, void *sess, int *retVal);

// Helper functions
static inline param_header_t* get_param_header(void *buf, int index) {
    uint32_t offset = sizeof(uint32_t);  // Skip param count
    for (int i = 0; i < index; i++) {
        param_header_t *hdr = (param_header_t*)((char*)buf + offset);
        offset += PARAM_HEADER_SIZE + ALIGN(hdr->size, PARAM_ALIGN);
    }
    return (param_header_t*)((char*)buf + offset);
}

static inline void* get_param_data(param_header_t *hdr) {
    return (void*)((char*)hdr + PARAM_HEADER_SIZE);
}

#endif // __FASTRPC_PROCBUF_H__