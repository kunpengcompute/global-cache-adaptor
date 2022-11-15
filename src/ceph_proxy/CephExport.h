/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef _CEPH_EXPORT_H_
#define _CEPH_EXPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PROXY_API_PUBLIC
#define PROXY_API_PUBLIC __attribute__((visibility ("default")))
#endif

PROXY_API_PUBLIC int CephLibrbdSnapRemove(int64_t pool_id,
		const char* namespace_name,
		const char* image_id,
		uint64_t snap_id,
		bool force);

PROXY_API_PUBLIC int CephLibrbdGetImageInfo(int64_t pool_id,
						const char *_image_id,
						int32_t *num_objs);

PROXY_API_PUBLIC int CephLibrbdDiskUsage(uint64_t *usage);

#ifdef __cplusplus
}
#endif

#endif //_LIBRBD_EXPORT_H_