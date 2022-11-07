/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltf All rights reserved.
 *
 */
#include "CephProxyInterface.h"
#include "CephProxy.h"
#include "RadosWrapper.h"
#include "CephProxyOp.h"

#include <iostream>
#include <string>
#include <time.h>

int CephProxyInit(const char *conf, size_t wNum, const char *log,
	         ceph_proxy_t *proxy)
{
    int ret = 0;
	if (conf == nullptr || log == nullptr || proxy == nullptr) {
		ProxyDbgLogErr("conf %p or log %p or proxy %p should not nullptr", conf, log, proxy);
		return -EINVAL;
	}
    std::string config(conf);
    std::string logPath(log);

    CephProxy *cephProxy = CephProxy::GetProxy();
    ret = cephProxy->Init(config, logPath, wNum);
    if (ret < 0) {
		ProxyDbgLogErr("CephProxy Init failed: %d", ret);
		*proxy = nullptr;
		return ret;
    }
    *proxy = cephProxy;
    return ret;
}

void CephProxyShutdown(ceph_proxy_t proxy)
{
    CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr) {
		ProxyDbgLogErr("proxy %p is invalid", cephProxy);
		return;
	}
    cephProxy->Shutdown();
    cephProxy->instance = NULL;
    delete cephProxy;
    proxy = nullptr;
}

ceph_proxy_t GetCephProxyInstance(void)
{
	return (ceph_proxy_t)(CephProxy::instance);
}

int32_t CephProxyQueueOp(ceph_proxy_t proxy, ceph_proxy_op_t op, completion_t c)
{
    CephProxy *cephProxy = static_cast<CephProxy*>(proxy);
	if (cephProxy == nullptr) {
		ProxyDbgLogErr("proxy %p is invalid", cephProxy);
		return -EINVAL;
	}
    return cephProxy->Enqueue(op, c);
}

rados_ioctx_t CephProxyGetIoCtx(ceph_proxy_t proxy, const char *poolname)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	return cephProxy->GetIoCtx(poolname);
}
 
rados_ioctx_t CephProxyGetIoCtx2(ceph_proxy_t proxy, const int64_t poolId)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr) {
		ProxyDbgLogErr("proxy %p is invalid", cephProxy);
		return nullptr;
	}
	return cephProxy->GetIoCtx2(poolId);
}

rados_ioctx_t CephProxyGetIoCtxFromCeph(ceph_proxy_t proxy, const int64_t poolId)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr) {
		ProxyDbgLogErr("proxy %p is invalid", cephProxy);
		return nullptr;
	}
	return cephProxy->GetIoCtxFromCeph(poolId);
}

void CephProxyReleaseIoCtx(rados_ioctx_t ioctx)
{
	return RadosReleaseIoCtx(ioctx);
}

int64_t CephProxyGetPoolIdByPoolName(ceph_proxy_t proxy, const char *poolName)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	return cephProxy->GetPoolIdByPoolName(poolName);
}

int CephProxyGetPoolNameByPoolId(ceph_proxy_t proxy, int64_t poolId, char *buf, unsigned maxLen)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	return cephProxy->GetPoolNameByPoolId(poolId, buf, maxLen);
}

int64_t CephProxyGetPoolIdByCtx(ceph_proxy_t proxy, rados_ioctx_t ioctx)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	return cephProxy->GetPoolIdByCtx(ioctx);
}

int CephProxyGetMinAllocSize(ceph_proxy_t proxy, uint32_t *minAllocSize, CEPH_BDEV_TYPE_E type)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	return cephProxy->GetMinAllocSize(minAllocSize, type);
}

int CephProxyGetClusterStat(ceph_proxy_t proxy, CephClusterStat *result)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr || result == nullptr) {
		ProxyDbgLogErr("proxy %p or result %p is invalid", cephProxy, result);
		return -EINVAL;
	}
	return cephProxy->GetClusterStat(result);
}

int CephProxyGetPoolStat(ceph_proxy_t proxy, rados_ioctx_t io, CephPoolStat *stats)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr || stats == nullptr) {
		ProxyDbgLogErr("proxy %p or stats %p is invalid", cephProxy, stats);
		return -EINVAL;
	}
	return cephProxy->GetPoolStat(io, stats);
}

int CephProxyGetPoolsStat(ceph_proxy_t proxy, CephPoolStat *stats, uint64_t *poolId, uint32_t poolNum)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr || stats == nullptr || poolId == nullptr) {
		ProxyDbgLogErr("proxy %p or stats %p or poolId %p is invalid", cephProxy, stats, poolId);
		return -EINVAL;
	}
	return cephProxy->GetPoolsStat(stats, poolId, poolNum);
}

int CephProxyGetState(ceph_proxy_t proxy)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	return (int)(cephProxy->GetState());
}

int CephProxyGetUsedSizeAndMaxAvail(ceph_proxy_t proxy, uint64_t &usedSize, uint64_t &maxAvail)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr) {
		ProxyDbgLogErr("proxy %p is invalid", cephProxy);
		return -EINVAL;
	}
	return cephProxy->GetPoolUsedSizeAndMaxAvail(usedSize, maxAvail);
}

int CephProxyRegisterPoolNewNotifyFn(NotifyPoolEventFn fn)
{
	ceph_proxy_t proxy = GetCephProxyInstance();
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr) {
		ProxyDbgLogErr("proxy %p is invalid", cephProxy);
		return -EINVAL;
	}
	return cephProxy->RegisterPoolNewNotifyFn(fn);
}

int CephProxyGetPoolInfo(ceph_proxy_t proxy, uint32_t poolId, struct PoolInfo *info)
{
	CephProxy *cephProxy = static_cast<CephProxy *>(proxy);
	if (cephProxy == nullptr || info == nullptr) {
		ProxyDbgLogErr("proxy %p or pool info %p is invalid", cephProxy, info);
		return -EINVAL;
	}
	return cephProxy->GetPoolInfo(poolId, info);
}

int CephProxyWriteOpInit2(ceph_proxy_op_t *op, const int64_t poolId, const char* oid)
{
	if (op == nullptr || oid == nullptr) {
		ProxyDbgLogErr("op %p or oid %p is invalid", op, oid);
		return -EINVAL;
	}
	*op = RadosWriteOpInit2(poolId, oid);
	if (*op == nullptr) {
	    ProxyDbgLogErr("Create Write Op failed.");
	    return -1;
	}

	return 0;
}

void CephProxyWriteOpRelease(ceph_proxy_op_t op) {
	RadosWriteOpRelease(op);
}

void CephProxyWriteOpSetFlags(ceph_proxy_op_t op, int flags)
{
	RadosWriteOpSetFlags(op, flags);
}

void CephProxyWriteOpAssertExists(ceph_proxy_op_t op)
{
	RadosWriteOpAssertExists(op);
}

void CephProxyWriteOpAssertVersion(ceph_proxy_op_t op, uint64_t ver)
{
	RadosWriteOpAssertVersion(op, ver);
}

void CephProxyWriteOpCmpext(ceph_proxy_op_t op, const char *cmpBuf,
	       			size_t cmpLen, uint64_t off, int *prval)
{
	RadosWriteOpCmpext(op, cmpBuf, cmpLen, off ,prval);
}

void CephProxyWriteOpCmpXattr(ceph_proxy_op_t op,  const char *name, 
		uint8_t compOperator, const char *value, size_t valLen)
{
	RadosWriteOpCmpXattr(op, name ,compOperator, value, valLen);
}

void CephProxyWriteOpOmapCmp(ceph_proxy_op_t op, const char *key, uint8_t compOperator, 
				const char *value, size_t valLen, int *prval)
{
	RadosWriteOpOmapCmp(op, key, compOperator, value, valLen, prval);
}

void CephProxyWriteOpSetXattr(ceph_proxy_op_t op, const char *name, 
				const char *value, size_t valLen)
{
	RadosWriteOpSetXattr(op, name, value, valLen);
}

void CephProxyWriteOpRemoveXattr(ceph_proxy_op_t op, const char *name)
{
	RadosWriteOpRemoveXattr(op, name);
}

void CephProxyWriteOpCreateObject(ceph_proxy_op_t op, int exclusive, const char *category)
{
	RadosWriteOpCreateObject(op, exclusive, category);
}

void CephProxyWriteOpWrite(ceph_proxy_op_t op, const char *buffer, size_t len, uint64_t off)
{
	RadosWriteOpWrite(op, buffer, len, off);
}

void CephProxyWriteOpWriteBl(ceph_proxy_op_t op, void *s, size_t len1, uint64_t off, AlignBuffer *alignBuffer, int isRelease)
{
	RadosWriteOpWriteBl(op, static_cast<GcBufferList *>(s), len1, off, alignBuffer, isRelease);
}

void CephProxyWriteOpRemove(ceph_proxy_op_t op)
{
	RadosWriteOpRemove(op);
}

void CephProxyWriteOpOmapSet(ceph_proxy_op_t op, char const* const* keys, 
			char const* const* vals, const size_t *lens, size_t num)
{
	RadosWriteOpOmapSet(op, keys, vals, lens, num);
}

void CephProxyWriteOpOmapRmKeys(ceph_proxy_op_t op, char const* const* keys, size_t keysLen)
{
	RadosWriteOpOmapRmKeys(op, keys, keysLen);
}

void CephProxyWriteOpOmapClear(ceph_proxy_op_t op)
{
	RadosWriteOpOmapClear(op);
}

void CephProxyWriteOpSetAllocHint(ceph_proxy_op_t op, uint64_t expectedObjSize,
	       		uint64_t expectedWriteSize, uint32_t flags)
{
	RadosWriteOpSetAllocHint(op, expectedObjSize, expectedWriteSize, flags);
}

int CephProxyReadOpInit2(ceph_proxy_op_t *op, const int64_t poolId, const char* oid)
{
	if (op == nullptr || oid == nullptr) {
		ProxyDbgLogErr("op %p or oid %p is invalid", op, oid);
		return -EINVAL;
	}
	*op = RadosReadOpInit2(poolId, oid);
	if (*op == nullptr) {
		ProxyDbgLogErr("Create Read Op failed.");
		return -1;
	}

	return 0;
}

void CephProxyReadOpRelease(ceph_proxy_op_t op)
{
	RadosReadOpRelease(op);
}

void CephProxyReadOpSetFlags(ceph_proxy_op_t op, int flags)
{
	RadosReadOpSetFlags(op, flags);
}

void CephProxyReadOpAssertExists(ceph_proxy_op_t op)
{
	RadosReadOpAssertExists(op);
}

void CephProxyReadOpAssertVersion(ceph_proxy_op_t op, uint64_t ver)
{
	RadosReadOpAssertVersion(op, ver);
}

void CephProxyReadOpCmpext(ceph_proxy_op_t op, const char *cmpBuf, 
			size_t cmpLen, uint64_t off, int *prval)
{
	RadosReadOpCmpext(op, cmpBuf, cmpLen, off, prval);
}

void CephProxyReadOpCmpXattr(ceph_proxy_op_t op,  const char *name, 
		uint8_t compOperator, const char *value, size_t valueLen)
{
	RadosReadOpCmpXattr(op, name, compOperator, value, valueLen);
}

void CephProxyReadOpGetXattrs(ceph_proxy_op_t op, proxy_xattrs_iter_t *iter, int *prval)
{
	RadosReadOpGetXattrs(op, iter, prval);
}

void CephProxyReadOpOmapCmp(ceph_proxy_op_t op, const char *key, 
		uint8_t compOperator, const char *val, size_t valLen, int *prval)
{
	RadosReadOpOmapCmp(op, key, compOperator, val, valLen, prval);
}

void CephProxyReadOpStat(ceph_proxy_op_t op, uint64_t *psize, time_t *pmtime, int *prval)
{
	RadosReadOpStat(op, psize, pmtime, prval);
}

void CephProxyReadOpRead(ceph_proxy_op_t op, uint64_t offset, size_t len, 
			char *buffer, size_t *bytesRead, int *prval)
{
	RadosReadOpRead(op, offset, len, buffer, bytesRead, prval);
}

void CephProxyReadOpReadBl(ceph_proxy_op_t op, uint64_t offset, size_t len, void *s, int *prval, int isRelease)
{
	RadosReadOpReadBl(op, offset, len , static_cast<GcBufferList *>(s), prval, isRelease);
}

void CephProxyReadOpCheckSum(ceph_proxy_op_t op, proxy_checksum_type_t type, 
			const char *initValue, size_t initValueLen, 
			uint64_t offset, size_t len, size_t chunkSize, char *pCheckSum,
		       	size_t CheckSumLen, int *prval) 
{
	RadosReadOpCheckSum(op ,type,  initValue, initValueLen, offset, len, chunkSize, pCheckSum, CheckSumLen, prval);
}

completion_t CephProxyCreateCompletion(CallBack_t fn, void *arg) {
	return CompletionInit(fn, arg);
}

void CephProxyCompletionDestroy(completion_t c) {
	CompletionDestroy(c);
}

