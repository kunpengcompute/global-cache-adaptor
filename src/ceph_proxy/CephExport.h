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

/*
 * 功能描述: 快照移除接口
 * 参数: 
 * pool_id: 传入参数 池id
 * namespace_name: 传入参数 命名空间
 * image_id: 传入参数 image id
 * snap_id: 传入参数 要删除的快照id
 * force: 是否强制删除
 * 返回值：
 *  0 成功
 *  负数错误码 失败
 */
PROXY_API_PUBLIC int CephLibrbdSnapRemove(int64_t pool_id,
                        const char *namespace_name,
                        const char *image_id,
                        uint64_t snap_id,
                        bool force);

PROXY_API_PUBLIC int CephLibrbdGetImageInfo(int64_t pool_id,
                        const char *_image_id,
                        int32_t *num_objs);

PROXY_API_PUBLIC int CephLibrbdDiskUsage(uint64_t *usage);

#ifdef __cplusplus
}
#endif

#endif // _LIBRBD_EXPORT_H_