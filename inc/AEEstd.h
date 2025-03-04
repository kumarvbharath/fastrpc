// Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __AEESTD_H__
#define __AEESTD_H__

#define STD_OFFSETOF(type,member)     (((char*)(&((type*)1)->member))-((char*)1))

#define STD_RECOVER_REC(type,member,p) ((void)((p)-&(((type*)1)->member)),\
                                        (type*)(void*)(((char*)(void*)(p))-STD_OFFSETOF(type,member)))

#define ALIGN(p, a)	      (((p) + ((a) - 1)) & ~((a) - 1))

#endif /* __AEESTD_H__ */