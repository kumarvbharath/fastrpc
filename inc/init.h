#ifndef __FASTRPC_INIT_H__
#define __FASTRPC_INIT_H__

/**
 * @brief DSP callback function for remote control events.
 *
 * @param event The type of event.
 * @param ctx A pointer to the context data associated with the callback.
 * @param data A pointer to the data associated with the callback.
 * @param ret A pointer to a void pointer where the callback can store a result.
 *
 * @return int An error code indicating the success or failure of the callback.
 */
int remotectl_dsp_callback(int event, void **ctx, void *data, void **ret);


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


/*
 * register the dsp callback API. Notifies the init and deinit of
 * the sessions created on the DSP.
 */
void *rpcmem_dsp_callback(int event, void **ctx, void *data, int *retVal);

#endif /*__FASTRPC_INIT_H__*/