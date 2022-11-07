/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */
#ifndef _CEPH_PROXY_H_
#define _CEPH_PROXY_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "CephProxyInterface.h"
#include "RadosWorker.h"
#include "PoolContext.h"
#include "RadosMonitor.h"

typedef void *rados_client_t;

class RadosWorker;
class PoolUsageStat;

typedef enum {
	PROXY_INITING = 1,
	PROXY_ACTIVE  = 2,
	PROXY_DOWN    = 3,
}CephProxyState;

struct ProxyConfig {
    std::string cephConfigFile;
    std::string logPath;
    size_t workerNum;
    bool useCheck;
};

class CephProxy {
public:
	rados_client_t radosClient;
	IOCtxTable ptable;
	ProxyConfig config;
	RadosWorker *worker;
	PoolUsageStat *poolStatManager;

	CephProxyState state;

	static CephProxy *instance;
private:
	CephProxy(): state(PROXY_DOWN) { }
public:
	static CephProxy* GetProxy() {
	    if ( instance == nullptr) {
		 instance = new(std::nothrow) CephProxy();
     	 	  if ( instance == nullptr) {
			return nullptr;
		  }
	     }

	     return instance;
	}

	int Init(const std::string& cephConf, const std::string &logPath,
 	   	 size_t wNum);
	
	void Shutdown();
	int32_t Enqueue(ceph_proxy_op_t op, completion_t c);

	CephProxyState GetState() const;
	rados_ioctx_t GetIoCtx(const std::string& pool);
	rados_ioctx_t GetIoCtx2(const int64_t poolId);
	rados_ioctx_t GetIoCtxFromCeph(const int64_t poolId);
	int64_t GetPoolIdByPoolName(const char *poolName);
	int GetPoolNameByPoolId(int64_t poolId, char *buf, unsigned maxLen);
	int64_t GetPoolIdByCtx(rados_ioctx_t ioctx);
	int GetClusterStat(CephClusterStat *stat);
	int GetPoolStat(rados_ioctx_t ctx, CephPoolStat *stat);
	int GetPoolsStat(CephPoolStat *stat, uint64_t *poolId, uint32_t poolNum);
	int GetMinAllocSize(uint32_t *minAllocSize, CEPH_BDEV_TYPE_E type);
	int GetPoolUsedSizeAndMaxAvail(uint64_t &usedSize, uint64_t &maxAvail);
	int RegisterPoolNewNotifyFn(NotifyPoolEventFn fn);
	int GetPoolInfo(uint32_t poolId, struct PoolInfo *info);
};

#endif
