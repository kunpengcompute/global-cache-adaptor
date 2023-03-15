/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef _CEPH_PROXY_RADOS_WRAPPER_H_
#define _CEPH_PROXY_RADOS_WRAPPER_H_

#include "CephProxyInterface.h"
#include "rados/librados.h"
#include "CephProxyOp.h"

void RadosBindMsgrWorker(std::vector<uint32_t> coreId, pid_t pid);
int RadosClientInit(rados_client_t *client, const std::string &cephConf);
int RadosSetConf(rados_client_t client, const char *option, const char *value);
void RadosClientShutdown(rados_client_t client);
int RadosOperationAioOperate(rados_client_t client, rados_op_t op, rados_ioctx_t io, userCallback_t fn, void *cbArg);
int RadosCreateIoCtx(rados_client_t client, const std::string &poolname, rados_ioctx_t *ctx);
int RadosCreateIoCtx2(rados_client_t client, const int64_t poolId, rados_ioctx_t *ioctx);
void RadosReleaseIoCtx(rados_ioctx_t ctx);
int64_t RadosGetPoolId(rados_ioctx_t ctx);
int RadosGetPoolName(rados_ioctx_t ctx, char *buf, unsigned maxLen);

int RadosGetMinAllocSizeHDD(rados_client_t client, uint32_t *minAllocSize);
int RadosGetMinAllocSizeSSD(rados_client_t client, uint32_t *minAllocSize);
int RadosGetClusterStat(rados_client_t client, CephClusterStat *stat);
int RadosGetPoolStat(rados_client_t client, rados_ioctx_t ctx, CephPoolStat *stat);
int RadosGetPoolsStat(rados_client_t client, CephPoolStat *stat, uint64_t *poolId, uint32_t poolNum);
/* WriteOp */

rados_op_t RadosWriteOpInit(const string& pool, const string &oid);

rados_op_t RadosWriteOpInit2(const int64_t poolId, const string &oid);

void RadosWriteOpRelease(rados_op_t op);

void RadosWriteOpSetFlags(rados_op_t op, int flags);

void RadosWriteOpAssertExists(rados_op_t op);

void RadosWriteOpAssertVersion(rados_op_t op, uint64_t ver);

void RadosWriteOpCmpext(rados_op_t op, const char *cmpBuf, 
                            size_t cmpLen, uint64_t off, int *prval);

void RadosWriteOpCmpXattr(rados_op_t op, const char *name, 
                                uint8_t compOperator, const char *value, size_t valLen);

void RadosWriteOpOmapCmp(rados_op_t op, const char *key, uint8_t compOperator, \
                            const char *value, size_t valLen, int *prval);

void RadosWriteOpSetXattr(rados_op_t op, const char *name, const char *value, size_t valLen);

void RadosWriteOpRemoveXattr(rados_op_t op, const char *name);

void RadosWriteOpCreateObject(rados_op_t op, int exclusive, const char *category);

void RadosWriteOpWrite(rados_op_t op, const char *buffer, size_t len, uint64_t off);

void RadosWriteOpWriteBl(rados_op_t op, GcBufferList *bl, size_t len1, uint64_t off, AlignBuffer *alignBuffer, int isRelease);

void RadosWriteOpRemove(rados_op_t op);

void RadosWriteOpTruncate(rados_op_t op, uint64_t off);

void RadosWriteOpZero(rados_op_t op, uint64_t off, uint64_t len);

void RadosWriteOpExec(rados_op_t op, const char *cls, const char *method, 
                            const char *inBuf, size_t inLen, int *prval);

void RadosWriteOpOmapSet(rados_op_t op, const char *const *keys, 
                            const char *const *vals, const size_t *lens, size_t num);

void RadosWriteOpOmapRmKeys(rados_op_t op, const char *const *keys, size_t keysLen);

void RadosWriteOpOmapClear(rados_op_t op);

void RadosWriteOpSetAllocHint(rados_op_t op, 
                             uint64_t expectedObjSize,
                             uint64_t expectedWriteSize,
                             uint32_t flags);

/* ReadOp */

rados_op_t RadosReadOpInit(const string& pool, const string &oid);

rados_op_t RadosReadOpInit2(const int64_t poolId, const string &oid);

void RadosReadOpRelease(rados_op_t op);

void RadosReadOpSetFlags(rados_op_t op, int flags);

void RadosReadOpAssertExists(rados_op_t op);

void RadosReadOpAssertVersion(rados_op_t op, uint64_t ver);

void RadosReadOpCmpext(rados_op_t op, const char *cmpBuf, 
                            size_t cmpLen, uint64_t off, int *prval);

void RadosReadOpCmpXattr(rados_op_t op, const char *name, uint8_t compOperator, 
                            const char *value, size_t valueLen);
void RadosReadOpGetXattr(rados_op_t op, const char *name, char **val, int *prval);
void RadosReadOpGetXattrs(rados_op_t op, proxy_xattrs_iter_t *iter, int *prval);


int RadosOmapGetNext(proxy_omap_iter_t iter, char **key, char **val, size_t *keyLen, size_t *valLen);
size_t RadosOmapIterSize(proxy_omap_iter_t iter);
void RadosOmapIterEnd(proxy_omap_iter_t iter);

void RadosReadOpOmapGetKeys(rados_op_t op, const char *startAfter, uint64_t maxReturn, 
                            proxy_omap_iter_t *iter, unsigned char *pmore, int *prval);

void RadosReadOpOmapGetVals(rados_op_t op, const char *startAfter,
                            uint64_t maxReturn, rados_omap_iter_t *iter, 
                            unsigned char *pmore, int *prval);

void RadosReadOpOmapCmp(rados_op_t op, const char *key, uint8_t compOperator, 
                            const char *val, size_t valLen, int *prval);

void RadosReadOpStat(rados_op_t op, uint64_t *psize, time_t *pmtime, int *prval);

void RadosReadOpRead(rados_op_t op, uint64_t offset, size_t len, char *buffer, 
                        size_t *bytesRead, int *prval);

void RadosReadOpReadBl(rados_op_t op, uint64_t offset, size_t len, GcBufferList *bl, int *prval, int isRelease);

void RadosReadOpCheckSum(rados_op_t op, proxy_checksum_type_t type, 
                            const char *initValue, size_t initValueLen, 
                            uint64_t offset, size_t len, size_t chunkSize, 
                            char *pCheckSum, size_t CheckSumLen, int *prval);

void RadosReadOpExec(rados_op_t op, const char *cls, const char *method, 
                        const char *inBuf, size_t inLen, char **outBuf, 
                        size_t *outLen, int *prval);

void RadosReadOpExecUserBuf(rados_op_t op, const char *cls, const char *method, 
                                const char *inBuf, size_t inLen, char *outBuf, 
                                size_t outLen, size_t *usedLen, int *prval);

int RadosGetXattrsNext(proxy_xattrs_iter_t iter, const char **name, const char **val, size_t *len);
void RadosGetXattrsEnd(proxy_xattrs_iter_t iter);

#endif
