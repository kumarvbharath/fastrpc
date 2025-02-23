// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __FASTRPC_IOCTL_UPSTREAM_H__
#define __FASTRPC_IOCTL_UPSTREAM_H__

#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include "remote.h"

/* File only compiled  when support to upstream kernel is required*/

/**
 * FastRPC IOCTL functions
 **/
#define FASTRPC_IOCTL_ALLOC_DMA_BUFF		_IOWR('R', 1, struct fastrpc_ioctl_alloc_dma_buf)
#define FASTRPC_IOCTL_FREE_DMA_BUFF		_IOWR('R', 2, __u32)
#define FASTRPC_IOCTL_INVOKE			_IOWR('R', 3, struct fastrpc_ioctl_invoke)
#define FASTRPC_IOCTL_INIT_ATTACH		_IO('R', 4)
#define FASTRPC_IOCTL_INIT_CREATE		_IOWR('R', 5, struct fastrpc_ioctl_init_create)
#define FASTRPC_IOCTL_MMAP			_IOWR('R', 6, struct fastrpc_ioctl_req_mmap)
#define FASTRPC_IOCTL_MUNMAP			_IOWR('R', 7, struct fastrpc_ioctl_req_munmap)
#define FASTRPC_IOCTL_INIT_ATTACH_SNS		_IO('R', 8)
#define FASTRPC_IOCTL_INIT_CREATE_STATIC	_IOWR('R', 9, struct fastrpc_ioctl_init_create_static)
#define FASTRPC_IOCTL_MEM_MAP			_IOWR('R', 10, struct fastrpc_ioctl_mem_map)
#define FASTRPC_IOCTL_MEM_UNMAP			_IOWR('R', 11, struct fastrpc_ioctl_mem_unmap)
#define FASTRPC_IOCTL_GET_DSP_INFO		_IOWR('R', 13, struct fastrpc_ioctl_capability)

#define ADSPRPC_DEVICE "/dev/fastrpc-adsp"
#define SDSPRPC_DEVICE "/dev/fastrpc-sdsp"
#define MDSPRPC_DEVICE "/dev/fastrpc-mdsp"
#define CDSPRPC_DEVICE "/dev/fastrpc-cdsp"
#define CDSP1RPC_DEVICE "/dev/fastrpc-cdsp1"
#define ADSPRPC_SECURE_DEVICE "/dev/fastrpc-adsp-secure"
#define SDSPRPC_SECURE_DEVICE "/dev/fastrpc-sdsp-secure"
#define MDSPRPC_SECURE_DEVICE "/dev/fastrpc-mdsp-secure"
#define CDSPRPC_SECURE_DEVICE "/dev/fastrpc-cdsp-secure"
#define CDSP1RPC_SECURE_DEVICE "/dev/fastrpc-cdsp1-secure"

#define FASTRPC_ATTR_NOVA (256)

/* Secure and default device nodes */
#if DEFAULT_DOMAIN_ID==ADSP_DOMAIN_ID
	#define SECURE_DEVICE "/dev/fastrpc-adsp-secure"
	#define DEFAULT_DEVICE "/dev/fastrpc-adsp"
#elif DEFAULT_DOMAIN_ID==MDSP_DOMAIN_ID
	#define SECURE_DEVICE "/dev/fastrpc-mdsp-secure"
	#define DEFAULT_DEVICE "/dev/fastrpc-mdsp"
#elif DEFAULT_DOMAIN_ID==SDSP_DOMAIN_ID
	#define SECURE_DEVICE "/dev/fastrpc-sdsp-secure"
	#define DEFAULT_DEVICE "/dev/fastrpc-sdsp"
#elif DEFAULT_DOMAIN_ID==CDSP_DOMAIN_ID
	#define SECURE_DEVICE "/dev/fastrpc-cdsp-secure"
	#define DEFAULT_DEVICE "/dev/fastrpc-cdsp"
#else
	#define SECURE_DEVICE ""
	#define DEFAULT_DEVICE ""
#endif

struct fastrpc_invoke_args {
	__u64 ptr; /* pointer to invoke address*/
	__u64 length; /* size*/
	__s32 fd; /* fd */
	__u32 attr; /* invoke attributes */
};

struct fastrpc_ioctl_invoke {
	__u32 handle;
	__u32 sc;
	__u64 args;
};

struct fastrpc_ioctl_alloc_dma_buf {
	__s32 fd;	/* fd */
	__u32 flags; /* flags to map with */
	__u64 size;	/* size */
};

struct fastrpc_ioctl_init_create {
	__u32 filelen;	/* elf file length */
	__s32 filefd;	/* fd for the file */
	__u32 attrs;
	__u32 siglen;
	__u64 file;	/* pointer to elf file */
};

struct fastrpc_ioctl_init_create_static {
	__u32 namelen;	/* length of pd process name */
	__u32 memlen;
	__u64 name;	/* pd process name */
};

struct fastrpc_ioctl_req_mmap {
	__s32 fd;
	__u32 flags;	/* flags for dsp to map with */
	__u64 vaddrin;	/* optional virtual address */
	__u64 size;	/* size */
	__u64 vaddrout;	/* dsp virtual address */
};

struct fastrpc_ioctl_mem_map {
	__s32 version;
	__s32 fd;		/* fd */
	__s32 offset;		/* buffer offset */
	__u32 flags;		/* flags defined in enum fastrpc_map_flags */
	__u64 vaddrin;		/* buffer virtual address */
	__u64 length;		/* buffer length */
	__u64 vaddrout;		/* [out] remote virtual address */
	__s32 attrs;		/* buffer attributes used for SMMU mapping */
	__s32 reserved[4];
};

struct fastrpc_ioctl_req_munmap {
	__u64 vaddrout;	/* address to unmap */
	__u64 size;	/* size */
};

struct fastrpc_ioctl_mem_unmap {
	__s32 version;
	__s32 fd;		/* fd */
	__u64 vaddr;		/* remote process (dsp) virtual address */
	__u64 length;		/* buffer size */
	__s32 reserved[5];
};

#endif // __FASTRPC_IOCTL_UPSTREAM_H__
