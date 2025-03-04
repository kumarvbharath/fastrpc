#ifndef __FASTRPC_MEMORY_H__
#define __FASTRPC_MEMORY_H__

#define MALLOC fastrpc_malloc
#define FREE fastrpc_free

static inline void* fastrpc_malloc(size_t size) {
    return malloc(size);
}

static inline void fastrpc_free(void *ptr) {
    return free(ptr);
}

#endif /*__FASTRPC_MEMORY_H__*/