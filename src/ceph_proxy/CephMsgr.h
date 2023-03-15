/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#ifndef CEPH_PROXY_MSGR_H
#define CEPH_PROXY_MSGR_H

#include <string>
#include <vector>
#include <map>

#include "CephProxyInterface.h"

#ifndef PROXY_API_PUBLIC
#define PROXY_API_PUBLIC __attribute__((visibility ("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *proxyMsgrHandler;

class ProxyMsgr;
class ProxyDispatcher;

int32_t InitProxyMsgr(proxyMsgrHandler *handler);

int32_t DestroyProxyMsgr(proxyMsgrHandler *handler);

int32_t MsgrGetOSDMap(proxyMsgrHandler handler, 
    std::map<uint32_t, std::pair<int32_t, std::string>> &osdMap);  // <osdid : <weight, ip>>

int32_t MsgrGetPGMap(proxyMsgrHandler handler, int64_t poolId,
    std::map<uint32_t, std::vector<int>> &pgMap);          // <pg : [osd]>

int32_t MsgrCalBatchPGId(proxyMsgrHandler handler,
    int64_t poolId, std::vector<std::string> oid, std::vector<uint32_t> &pgs);

int32_t GetPoolUsedAndAvail(proxyMsgrHandler handler, uint64_t &usedSize, uint64_t &maxAvail);

int32_t CCMRegisterPoolNotifyFn(proxyMsgrHandler handler, NotifyPoolEventFn fn);

int32_t GetPoolBaseInfo(proxyMsgrHandler handler, uint32_t poolId, struct PoolInfo *info);

#ifdef __cplusplus
}
#endif

#endif // CEPH_PROXY_MSGR_H