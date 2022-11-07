/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef _CEPH_PROXY_POOL_HANDLER_H_
#define _CEPH_PROXY_POOL_HANDLER_H_

#include "CephProxyInterface.h"
#include "RadosWrapper.h"

#include <pthread.h>

#include <map>
#include <string>

class IOCtxTable {
private:
    pthread_spinlock_t lock;
    std::map<std::string, rados_ioctx_t> table1;
    std::map<const int64_t, rados_ioctx_t> table2;
public:
    IOCtxTable() {

    }

    ~IOCtxTable() {
	pthread_spin_destroy(&lock);
    }

    int Init() {    
        pthread_spin_init(&lock, PTHREAD_PROCESS_SHARED);
        table1.clear();
        table2.clear();
	return 0;
    }

    int Insert(const std::string &poolname, rados_ioctx_t ioctx) {
	int ret = 0;
	pthread_spin_lock(&lock);
	auto iter = table1.find(poolname);
	if (iter != table1.end()) {
	    ret = -1;
	}

	table1[poolname] = ioctx;
	pthread_spin_unlock(&lock);
	return ret;
    }
    
    int Insert(const int64_t poolId, rados_ioctx_t ioctx) {
	int ret = 0;
	pthread_spin_lock(&lock);
	auto iter = table2.find(poolId);
	if (iter != table2.end()) {
	    ret = -1;
	} else {
	    table2[poolId] = ioctx;
	}
	pthread_spin_unlock(&lock);
	return ret;
    }

    rados_ioctx_t GetIoCtx(const std::string &poolname) {
	rados_ioctx_t ioctx = nullptr;
	pthread_spin_lock(&lock);
	auto iter = table1.find(poolname);
	if (iter != table1.end()) {
	    ioctx = table1[poolname];
	}
	pthread_spin_unlock(&lock);

	return ioctx;
    }
    
    rados_ioctx_t GetIoCtx(const int64_t poolId) {
	rados_ioctx_t ioctx = nullptr;
	pthread_spin_lock(&lock);
	auto iter = table2.find(poolId);
	if (iter != table2.end()) {
	    ioctx = table2[poolId];
	}
	pthread_spin_unlock(&lock);

	return ioctx;
    }
	
    void Delete(const std::string &poolname) {
	pthread_spin_lock(&lock);
	auto iter = table1.find(poolname);
	if (iter != table1.end()) {
	    RadosReleaseIoCtx(iter->second);
            table1.erase(iter);
	}
	pthread_spin_unlock(&lock);
    }

    void Delete(const int64_t poolId) {
	pthread_spin_lock(&lock);
	auto iter = table2.find(poolId);
	if (iter != table2.end()) {
	    RadosReleaseIoCtx(iter->second);
            table2.erase(iter);
	}
	pthread_spin_unlock(&lock);
    }

    void Clear() {
	pthread_spin_lock(&lock);

        for (auto iter : table1) {
	    RadosReleaseIoCtx(iter.second);
	}

        for (auto iter : table2) {
	    RadosReleaseIoCtx(iter.second);
	}

	table1.clear();
	table2.clear();

	pthread_spin_unlock(&lock);
    }
};

#endif
