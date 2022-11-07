/*
* Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef _CEPH_PROXY_INTERFACE_H_
#define _CEPH_PROXY_INTERFACE_H_

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "Gcbufferlist.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PROXY_API_PUBLIC
#define PROXY_API_PUBLIC __attribute__((visibility ("default")))
#endif

#define CEPHPROXY_CREATE_EXCLUSIVE 1
#define CEPHPROXY_CREATE_IDEMPOTENT 0

#define POOL_NAME_MAX_LEN 128
#define OBJECT_ID_MAX_LEN 128
#define CONFIG_PATH_LEN   128


enum {

    CEPHPROXY_OP_FLAG_EXCL                = 0x1,

    CEPHPROXY_OP_FLAG_FAILOK              = 0x2,

    CEPHPROXY_OP_FLAG_FADVISE_RANDOM      = 0x4,

    CEPHPROXY_FLAG_FADVISE_SEQUENTIAL     = 0x8,

    CEPHPROXY_OP_FLAG_FADVISE_WILLNEED    = 0x10,

    CEPHPROXY_OP_FLAG_FADVISE_DONTNEED    = 0x20,

    CEPHPROXY_OP_FLAG_FADVISE_NOCACHE     = 0x40,

    CEPHPROXY_OP_FLAG_FADVISE_FUA         = 0x80,
};

enum {

    CEPHPROXY_CMPXATTR_OP_EQ              = 1,

    CEPHPROXY_CMPXATTR_OP_NE              = 2,

    CEPHPROXY_CMPXATTR_OP_GT              = 3,

    CEPHPROXY_CMPXATTR_OP_GTE             = 4,

    CEPHPROXY_CMPXATTR_OP_LT              = 5,

    CEPHPROXY_CMPXATTR_OP_LTE             = 6
};

enum {
    CEPHPROXY_OPERATION_NOFLAG                   = 0, 
    CEPHPROXY_OPERATION_BALANCE_READS            = 1, 
    CEPHPROXY_OPERATION_LOCALIZE_READS           = 2, 
    CEPHPROXY_OPERATION_ORDER_READS_WRITES       = 4, 
    CEPHPROXY_OPERATION_IGNORE_CACHE             = 8, 
    CEPHPROXY_OPERATION_SKIPRWLOCKS              = 16,
    CEPHPROXY_OPERATION_IGNORE_OVERLAY           = 32,
    CEPHPROXY_OPERATION_FULL_TRY                 = 64, 
    CEPHPROXY_OPERATION_FULL_FORCE               = 128, 
    CEPHPROXY_OPERATION_IGNORE_REDIRECT          = 256, 
    CEPHPROXY_OPERATION_ORDERSNAP                = 512, 
};

enum {
    CEPHPROXY_ALLOC_HINT_FLAG_SEQUENTIAL_WRITE    = 1,
    CEPHPROXY_ALLOC_HINT_FLAG_RANDOM_WRITE        = 2,
    CEPHPROXY_ALLOC_HINT_FLAG_SEQUENTIAL_READ     = 4,
    CEPHPROXY_ALLOC_HINT_FLAG_RANDOM_READ         = 8,
    CEPHPROXY_ALLOC_HINT_FLAG_APPEND_ONLY         = 16,
    CEPHPROXY_ALLOC_HINT_FLAG_IMMUTABLE           = 32,
    CEPHPROXY_ALLOC_HINT_FLAG_SHORTLIVED          = 64,
    CEPHPROXY_ALLOC_HINT_FLAG_LONGLIVED           = 128,
    CEPHPROXY_ALLOC_HINT_FLAG_COMPRESSIBLE        = 256,
    CEPHPROXY_ALLOC_HINT_FLAG_INCOMPRESSIBLE      = 512,
};

typedef enum {
    SINGLE_OP      = 0x01,
    BATCH_READ_OP  = 0x02,
    BATCH_WRITE_OP = 0x03,
    MOSD_OP        = 0x04,
} CephProxyOpType;
 
typedef enum {
    PROXY_NOP            =0x01,
    PROXY_ASYNC_READ     =0x02,
    PROXY_READ           =0x03,
    PROXY_ASYNC_WRITE    =0x04,
    PROXY_WRITE          =0x05,
} CephProxyOpCode;

#define CLIENT_OPERATE_SUCCESS  0x00
#define CLIENT_INIT_ERR         0x01
#define CLIENT_READ_CONF_ERR    0x02
#define CLIENT_CONNECT_ERR      0x03
#define CLIENT_CREATE_IOCTX_ERR 0x04
#define CLIENT_LOG_INIT_ERR     0x05

#define ASYNC_NOP_ERR           0x01
#define ASYNC_READ_ERR          0x02
#define ASYNC_WRITE_ERR         0x03
#define ASYNC_OPERATE_ERR       0x04
#define ASYNC_READ_OPERATE_ERR  0x05
#define ASYNC_WRITE_OPERATE_ERR 0x06

#define POOL_HANDLER_TABLE_SUCCESS           0x00
#define POOL_HANDLER_TABLE_EXIST             0x01

#define PROXYOP_SUCCESS                0x00
#define PROXYOP_CREATE_ERR             0x01
#define PROXYOP_INVALID                0x02

typedef enum {
    BUFFER_INC = 0x01,
    DIRECT_INC = 0x02,
} CephProxyOpFrom;

typedef enum {
    PROXY_CHECKSUM_TYPE_XXHASH32 = 0x00,
    PROXY_CHECKSUM_TYPE_XXHASH64 = 0x01,
    PROXY_CHECKSUM_TYPE_CRC32C   = 0x02,
} proxy_checksum_type_t;

typedef enum {
    CEPH_BDEV_HDD = 0x00,
    CEPH_BDEV_SSD = 0x01,
} CEPH_BDEV_TYPE_E;

typedef struct {
    char *prevAlignBuffer;
    size_t prevAlignLen;
    char *backAlignBuffer;
    size_t backAlignLen;
} AlignBuffer;

typedef struct {

    uint64_t numBytes;

    uint64_t numKb;

    uint64_t numObjects;

    uint64_t numObjectClones;

    uint64_t numObjectCopies;

    uint64_t numObjectsMissingOnPrimary;

    uint64_t numObjectsUnfound;


    uint64_t numObjectsDegraded;

    uint64_t numRd;

    uint64_t numRdKb;

    uint64_t numWr;

    uint64_t numWrKb;

    uint64_t numUserBytes;

    uint64_t compressedBytesOrig;

    uint64_t compressedBytes;

    uint64_t compressedBytesAlloc;
} CephPoolStat;

typedef struct {

    uint64_t kb;

    uint64_t kbUsed;

    uint64_t kbAvail;

    uint64_t numObjects;
} CephClusterStat;

typedef int32_t (*NotifyPoolEventFn)(uint32_t *poolId, uint32_t length);

typedef void *completion_t;
typedef void (*CallBack_t)(int ret, void *arg);
typedef void *ceph_proxy_op_t;
typedef void *rados_ioctx_t;
typedef void *ceph_proxy_t;
typedef uint64_t snap_t;
typedef void *proxy_xattrs_iter_t;
typedef void *proxy_omap_iter_t;
typedef void *rados_client_t;

struct PoolInfo{
    uint32_t k;
    uint32_t m;
    uint32_t stripeUnit;
};








PROXY_API_PUBLIC int CephProxyInit(const char *conf, size_t wNum, const char *log,
	       		ceph_proxy_t *proxy);







PROXY_API_PUBLIC void CephProxyShutdown(ceph_proxy_t proxy);






PROXY_API_PUBLIC ceph_proxy_t GetCephProxyInstance(void);







rados_ioctx_t CephProxyGetIoCtx(ceph_proxy_t proxy, const char *poolname);
 






PROXY_API_PUBLIC rados_ioctx_t CephProxyGetIoCtx2(ceph_proxy_t proxy, const int64_t poolId);







PROXY_API_PUBLIC rados_ioctx_t CephProxyGetIoCtxFromCeph(ceph_proxy_t proxy, const int64_t poolId);







PROXY_API_PUBLIC void CephProxyReleaseIoCtx(rados_ioctx_t ioctx);







int64_t CephProxyGetPoolIdByPoolName(ceph_proxy_t proxy, const char *poolName);









int CephProxyGetPoolNameByPoolId(ceph_proxy_t proxy, int64_t poolId, char *buf, unsigned maxLen);








int64_t CephProxyGetPoolIdByCtx(ceph_proxy_t proxy, rados_ioctx_t ioctx);








int CephProxyGetMinAllocSize(ceph_proxy_t proxy, uint32_t *minAllocSize, CEPH_BDEV_TYPE_E type);







PROXY_API_PUBLIC int CephProxyGetClusterStat(ceph_proxy_t proxy, CephClusterStat *result);








PROXY_API_PUBLIC int CephProxyGetPoolStat(ceph_proxy_t proxy, rados_ioctx_t ioctx, CephPoolStat *stats);







PROXY_API_PUBLIC int CephProxyGetPoolsStat(ceph_proxy_t proxy, CephPoolStat *stats, uint64_t *poolId, uint32_t poolNum);








PROXY_API_PUBLIC int CephProxyGetUsedSizeAndMaxAvail(ceph_proxy_t proxy, uint64_t &usedSize, uint64_t &maxAvail);








PROXY_API_PUBLIC int32_t CephProxyQueueOp(ceph_proxy_t proxy, ceph_proxy_op_t op, completion_t c);








PROXY_API_PUBLIC int CephProxyWriteOpInit2(ceph_proxy_op_t *op, const int64_t poolId, const char* oid);






PROXY_API_PUBLIC void CephProxyWriteOpRelease(ceph_proxy_op_t op);







void CephProxyWriteOpSetFlags(ceph_proxy_op_t op, int flags);






void CephProxyWriteOpAssertExists(ceph_proxy_op_t op);






void CephProxyWriteOpAssertVersion(ceph_proxy_op_t op, uint64_t ver);










void CephProxyWriteOpCmpext(ceph_proxy_op_t op, const char *cmpBuf, size_t cmpLen, uint64_t off, int *prval);










void CephProxyWriteOpCmpXattr(ceph_proxy_op_t op,  const char *name, uint8_t compOperator, const char *value, size_t valLen);











void CephProxyWriteOpOmapCmp(ceph_proxy_op_t op, const char *key, uint8_t compOperator, const char *value, size_t valLen, int *prval);









void CephProxyWriteOpSetXattr(ceph_proxy_op_t op, const char *name, const char *value, size_t valLen);







void CephProxyWriteOpRemoveXattr(ceph_proxy_op_t op, const char *name);








void CephProxyWriteOpCreateObject(ceph_proxy_op_t op, int exclusive, const char *category);









PROXY_API_PUBLIC void CephProxyWriteOpWrite(ceph_proxy_op_t op, const char *buffer, size_t len, uint64_t off);











PROXY_API_PUBLIC void CephProxyWriteOpWriteBl(ceph_proxy_op_t op, void *bl, size_t len1, uint64_t off, AlignBuffer *alignBuffer, int isRelease);






PROXY_API_PUBLIC void CephProxyWriteOpRemove(ceph_proxy_op_t op);












void CephProxyWriteOpOmapSet(ceph_proxy_op_t op, char const* const* keys, char const* const* vals, const size_t *lens, size_t num);








void CephProxyWriteOpOmapRmKeys(ceph_proxy_op_t op, char const* const* keys, size_t keysLen);






void CephProxyWriteOpOmapClear(ceph_proxy_op_t op);









void CephProxyWriteOpSetAllocHint(ceph_proxy_op_t op, uint64_t expectedObjSize, uint64_t expectedWriteSize, uint32_t flags);








PROXY_API_PUBLIC int CephProxyReadOpInit2(ceph_proxy_op_t *op, const int64_t poolId, const char* oid);






PROXY_API_PUBLIC void CephProxyReadOpRelease(ceph_proxy_op_t op);







void CephProxyReadOpSetFlags(ceph_proxy_op_t op, int flags);






void CephProxyReadOpAssertExists(ceph_proxy_op_t op);







void CephProxyReadOpAssertVersion(ceph_proxy_op_t op, uint64_t ver);










void CephProxyReadOpCmpext(ceph_proxy_op_t op, const char *cmpBuf, size_t cmpLen, uint64_t off, int *prval);










void CephProxyReadOpCmpXattr(ceph_proxy_op_t op,  const char *name, uint8_t compOperator, const char *value, size_t valueLen);








void CephProxyReadOpGetXattrs(ceph_proxy_op_t op, proxy_xattrs_iter_t *iter, int *prval);











void CephProxyReadOpOmapCmp(ceph_proxy_op_t op, const char *key, uint8_t compOperator, const char *val, size_t valLen, int *prval);









PROXY_API_PUBLIC void CephProxyReadOpStat(ceph_proxy_op_t op, uint64_t *psize, time_t *pmtime, int *prval);











PROXY_API_PUBLIC void CephProxyReadOpRead(ceph_proxy_op_t op, uint64_t offset, size_t len, char *buffer, size_t *bytesRead, int *prval);










PROXY_API_PUBLIC void CephProxyReadOpReadBl(ceph_proxy_op_t op, uint64_t offset, size_t len, void *bl, int *prval, int isRelease);















void CephProxyReadOpCheckSum(ceph_proxy_op_t op, proxy_checksum_type_t type, const char *initValue, size_t initValueLen,
	       			uint64_t offset, size_t len, size_t chunkSize, char *pCheckSum, 
				size_t checkSumLen, int *prval);











PROXY_API_PUBLIC completion_t CephProxyCreateCompletion(CallBack_t fn, void *arg);






PROXY_API_PUBLIC void CephProxyCompletionDestroy(completion_t c);






int CephProxyRegisterPoolDelNotifyFn(NotifyPoolEventFn fn);






PROXY_API_PUBLIC int CephProxyRegisterPoolNewNotifyFn(NotifyPoolEventFn fn);

PROXY_API_PUBLIC int CephProxyGetPoolInfo(ceph_proxy_t proxy, uint32_t poolId, struct PoolInfo *info);

#ifdef __cplusplus
}
#endif

#endif
