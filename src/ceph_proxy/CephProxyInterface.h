/*
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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
#include <map>
#include <string>
#include <vector>
#include <time.h>
#include "Gcbufferlist.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PROXY_API_PUBLIC
#define PROXY_API_PUBLIC __attribute__((visibility ("default")))
#endif

#define CEPHPROXY_CREATE_EXCLUSIVE  1
#define CEPHPROXY_CREATE_IDEMPOTENT 0

#define POOL_NAME_MAX_LEN   128
#define OBJECT_ID_MAX_LEN   128
#define CONFIG_PATH_LEN     128
#define INDEX_OBJECT_ID_LEN     50

// read & write Op flags;
enum {
  // fail a create operation if the object already exists
  CEPHPROXY_OP_FLAG_EXCL               =  0x1,
  // allow the transaction to succeed even if the flagged op fails
  CEPHPROXY_OP_FLAG_FAILOK 	           = 0x2,
  // indicate read/write op random
  CEPHPROXY_OP_FLAG_FADVISE_RANDOM     = 0x4,
  // indicate read/write op sequential
  CEPHPROXY_FLAG_FADVISE_SEQUENTIAL    = 0x8,
  // indicate read/write data will be accessed in the near future (by someone)
  CEPHPROXY_OP_FLAG_FADVISE_WILLNEED   = 0x10,
  // indicate read/write data will not accessed in the near future (by anyone)
  CEPHPROXY_OP_FLAG_FADVISE_DONTNEED   = 0x20,
  // indicate read/write data will not accessed again (by *this* client)
  CEPHPROXY_OP_FLAG_FADVISE_NOCACHE    = 0x40,
  // optionally support FUA (force unit access) on write requests
  CEPHPROXY_OP_FLAG_FADVISE_FUA        = 0x80,
};

enum {
    // equal
    CEPHPROXY_CMPXATTR_OP_EQ  = 1,
    // not equal
    CEPHPROXY_CMPXATTR_OP_NE  = 2,
    // greater 
    CEPHPROXY_CMPXATTR_OP_GT  = 3,
    // greate or equal
    CEPHPROXY_CMPXATTR_OP_GTE = 4,
    // less
    CEPHPROXY_CMPXATTR_OP_LT  = 5,
    // less or equal
    CEPHPROXY_CMPXATTR_OP_LTE = 6
};

enum {
  CEPHPROXY_OPERATION_NOFLAG             = 0,
  CEPHPROXY_OPERATION_BALANCE_READS      = 1,
  CEPHPROXY_OPERATION_LOCALIZE_READS     = 2,
  CEPHPROXY_OPERATION_ORDER_READS_WRITES = 4,
  CEPHPROXY_OPERATION_IGNORE_CACHE       = 8,
  CEPHPROXY_OPERATION_SKIPRWLOCKS        = 16,
  CEPHPROXY_OPERATION_IGNORE_OVERLAY     = 32,
  CEPHPROXY_OPERATION_FULL_TRY           = 64,
  CEPHPROXY_OPERATION_FULL_FORCE         = 128,
  CEPHPROXY_OPERATION_IGNORE_REDIRECT    = 256,
  CEPHPROXY_OPERATION_ORDERSNAP          = 512,
};

enum {
  CEPHPROXY_ALLOC_HINT_FLAG_SEQUENTIAL_WRITE = 1,
  CEPHPROXY_ALLOC_HINT_FLAG_RANDOM_WRITE     = 2,
  CEPHPROXY_ALLOC_HINT_FLAG_SEQUENTIAL_READ  = 4,
  CEPHPROXY_ALLOC_HINT_FLAG_RANDOM_READ      = 8,
  CEPHPROXY_ALLOC_HINT_FLAG_APPEND_ONLY      = 16,
  CEPHPROXY_ALLOC_HINT_FLAG_IMMUTABLE        = 32,
  CEPHPROXY_ALLOC_HINT_FLAG_SHORTLIVED       = 64,
  CEPHPROXY_ALLOC_HINT_FLAG_LONGLIVED        = 128,
  CEPHPROXY_ALLOC_HINT_FLAG_COMPRESSIBLE     = 256,
  CEPHPROXY_ALLOC_HINT_FLAG_INCOMPRESSIBLE   = 512,
};

typedef enum {
    SINGLE_OP       = 0x01,
    BATCH_READ_OP   = 0x02,
    BATCH_WRITE_OP  = 0x03,
    MOSD_OP         = 0x04,
} CephProxyOpType;

typedef enum {
    PROXY_NOP           = 0x01,
    PROXY_ASYNC_READ    = 0x02,
    PROXY_READ          = 0x03,
    PROXY_ASYNC_WRITE   = 0x04,
    PROXY_WRITE         = 0x05,
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

#define POOL_HANDLER_TABLE_SUCCESS  0x00
#define POOL_HANDLER_TABLE_EXIST    0x01

#define PROXYOP_SUCCESS     0x00
#define PROXYOP_CREATE_ERR  0x01
#define PROXYOP_INVALID     0x02

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
    // space used in bytes
    uint64_t numBytes;
    // space used in KB
    uint64_t numKb;
    // number of objects in the pool
    uint64_t numObjects;
    // number of clones of objects
    uint64_t numObjectClones;
    // num_objects * num_replicas
    uint64_t numObjectCopies;
    // number of objects missing on primary
    uint64_t numObjectsMissingOnPrimary;
    // number of objects found on no OSDs
    uint64_t numObjectsUnfound;
    // number of objects replicated fewer times than they should be
    // (but found on at least one OSD)
    uint64_t numObjectsDegraded;
    // number of objects read
    uint64_t numRd;
    // objects read in KB
    uint64_t numRdKb;
    // number of objects written
    uint64_t numWr;
    // objects written in KB
    uint64_t numWrKb;
    // bytes originally provided by user
    uint64_t numUserBytes;
    // bytes passed compression
    uint64_t compressedBytesOrig;
    // bytes resulted after compression
    uint64_t compressedBytes;
    // bytes allocated at storage
    uint64_t compressedBytesAlloc;
} CephPoolStat;

typedef struct {
    uint64_t num;       // list length
    uint32_t *osdId;    // osd list
    int32_t *weight;    // weight list
    uint32_t *ipIndex;      // ip index list
    char **ip;              // ip list
    uint32_t ipListLength;
} OSDStat;

typedef struct {
    uint64_t num;   // list num
    uint32_t *pg;   // pg list
    int32_t **osd;  // osd list(osd list num + 1), osd[0] == osd list num
} PGLocation;

typedef struct {
    int objNum;    // obj list num
    struct {
        uint32_t pg;
        char objectName[INDEX_OBJECT_ID_LEN];
    } objList[0];
} CalOSDPGParm;

typedef struct {
    // total device size
    uint64_t kb;
    // total used
    uint64_t kbUsed;
    // total available/free
    uint64_t kbAvail;
    /// number of objects
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
    uint32_t k;     // 数据块数量
    uint32_t m;     // 校验块数量
    uint32_t stripeUnit;    // 条带大小
};

/*
 * 功能描述: 初始化一个CephProxy实例
 * 参数：conf: {in}, ceph集群配置文件路径
        wNum: {in}, 工作线程数量
        log:  {in}, 日志路径
        proxy: {out}: CephProxy实例
 * 返回值：0表示成功，非零表示失败
 * 备注：同步接口
 */
PROXY_API_PUBLIC int CephProxyInit(const char *conf, size_t wNum, const char *log, 
                  ceph_proxy_t *proxy); 

/*
 * 功能描述: 销毁CephProxy实例
 * 参数：proxy: {in}: CephProxy实例
 * 返回值：无返回值
 * 备注：同步接口
 */
PROXY_API_PUBLIC void CephProxyShutdown(ceph_proxy_t proxy);

/*
 * 功能描述: 获取CephProxy的实例
 * 参数：无参数
 * 返回值：CephProxy实例
 * 备注：同步接口
 */
PROXY_API_PUBLIC ceph_proxy_t GetCephProxyInstance(void);

/*
 * 功能描述: 通过poolName获取指定Pool的IO上下文
 * 参数: poolname: {in}, pool名称
 * 返回值：返回一个rados_ioctx_t，表示Pool的上下文信息
 * 备注：同步接口
 */
rados_ioctx_t CephProxyGetIoCtx(ceph_proxy_t proxy, const char *poolname);

/*
 * 功能描述: 通过PoolID获取指定Pool的IO上下文
 * 参数: poolId: {in}, pool ID
 * 返回值：返回一个rados_ioctx_t，表示Pool的上下文信息
 * 备注：同步接口
 */
PROXY_API_PUBLIC rados_ioctx_t CephProxyGetIoCtx2(ceph_proxy_t proxy, const int64_t poolId);

/*
 * 功能描述: 通过PoolID获取指定Pool的IO上下文
 * 参数: poolId: {in}, pool ID
 * 返回值：返回一个rados_ioctx_t，表示Pool的上下文信息
 * 备注：同步接口
 */
PROXY_API_PUBLIC rados_ioctx_t CephProxyGetIoCtxFromCeph(ceph_proxy_t proxy, const int64_t poolId);

/*
 * 功能描述: Release IoCtx
 * 参数: ioctx: {in}: 要释放的ioctx
 * 返回值：无
 * 备注：同步接口
 */
PROXY_API_PUBLIC void CephProxyReleaseIoCtx(rados_ioctx_t ioctx);

/*
 * 功能描述: 通过PoolName获取PoolID
 * 参数: poolName: {in}, poolName
 * 返回值：返回poolId, 小于0表示错误
 * 备注：同步接口
 */
int64_t CephProxyGetPoolIdByPoolName(ceph_proxy_t proxy, const char *poolName);

/*
 * 功能描述: 通过PoolID获取PoolName
 * 参数: poolId: {in}, pool ID
         buf: poolName buffer
         maxLen: buf最大长度
 * 返回值：返回值为buf真实长度，如果<0则表示错误
 * 备注：同步接口
 */
int CephProxyGetPoolNameByPoolId(ceph_proxy_t proxy, int64_t poolId, char *buf, unsigned maxLen);

/*
 * 功能描述: 通过IOCtx获取指定PoolId
 * 参数:  proxy: {in}, proxy实例指针
          ioctx: {in}, ioctx指针
 * 返回值：返回poolId, 小于0表示错误
 * 备注：同步接口
 */
int64_t CephProxyGetPoolIdByCtx(ceph_proxy_t proxy, rados_ioctx_t ioctx);

/*
 * 功能描述: 读取Ceph集群min_alloc_size
 * 参数: proxy: {in}, proxy句柄
 *       type:  {in}, Ceph BLuestore 设备类型
 *       minAllocSize: {out}, min_alloc_size输出参数
 * 返回值：0表示成功，非0表示失败，与errno一致
 */
int CephProxyGetMinAllocSize(ceph_proxy_t proxy, uint32_t *minAllocSize, CEPH_BDEV_TYPE_E type);

/*
 * 功能描述：获取Ceph集群的usage 信息
 * 参数：proxy: {in}, CephPorxy实例
        state: {out}, 集群信息结构体
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyGetClusterStat(ceph_proxy_t proxy, CephClusterStat *result);

/*
 * 功能描述：获取Pool指定Pool的usage信息
 * 参数：proxy: {in}, CephProxy实例
        ioctx: {in}, Pool的IO上下文
        stats: {out}, Pool的usage信息结构体
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyGetPoolStat(ceph_proxy_t proxy, rados_ioctx_t ioctx, CephPoolStat *stats);

/*
 * 功能描述：获取OSDMap信息
 * 参数：proxy: {in}, CephProxy实例
        stat: {out}, osdMap
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyGetOSDMap(ceph_proxy_t proxy, OSDStat &stat);

PROXY_API_PUBLIC void CephProxyPutOSDMap(OSDStat &stat);

/*
 * 功能描述：获取PGMap信息
 * 参数：proxy: {in}, CephProxy实例
        poolId: poolId
        pgView: {out}, <pgid : osds>
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyGetPGMap(ceph_proxy_t proxy, int64_t poolId, PGLocation &pgView);

PROXY_API_PUBLIC void CephProxyPutPGMap(PGLocation &pgView);
/*
 * 功能描述：批量获取对象id对应的pgid
 * 参数：proxy: {in}, CephProxy实例
        poolId: poolId
        param: {in or out}, pg(vector)
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyCalBatchPGId(ceph_proxy_t proxy, int64_t poolId, CalOSDPGParm &param);

/*
 * 功能描述：获取Pool指定Pool的usage信息
 * 参数：proxy: {in}, CephProxy实例
        poolId: {in}, 查询poolid的名称
        poolNum： {in}, 查询pool的数量
        stats: {out}, Pool的usage信息结构体
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyGetPoolsStat(ceph_proxy_t proxy, CephPoolStat *stats, uint64_t *poolId, uint32_t poolNum);
/*
 * 功能描述：获取集群中所有Pool的UsedSize和maxAvail信息
 * 参数：proxy: {in}, CephProxy实例
        usedSize: {out}, 所有Pool的总的已使用容量
        maxAvail: {out}, Pool的最大可用空间；
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyGetUsedSizeAndMaxAvail(ceph_proxy_t proxy, uint64_t &usedSize, uint64_t &maxAvail);

/*
 * 功能描述: 提交一个请求
 * 参数：proxy: {in}, CephProxy实例
        op:    {in}, 请求
        c:     {in}, 完成回调
 * 返回值：0表示成功，-1表示入队失败
*/
PROXY_API_PUBLIC int32_t CephProxyQueueOp(ceph_proxy_t proxy, ceph_proxy_op_t op, completion_t c);

/*
 * 功能描述：初始化一个写类型请求
 * 参数说明：op: {out}, 请求Op的指针
            poolId: {in}, rados pool Id;
            oid: {in}, 对象ID
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyWriteOpInit2(ceph_proxy_op_t *op, const int64_t poolId, const char *oid);

/*
 * 功能描述: 释放一个写请求
 * 参数: op: {in}, 请求Op的指针
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC void CephProxyWriteOpRelease(ceph_proxy_op_t op);

/*
 * 功能描述: 设置WriteOp的flags
 * 参数: op: {in}, 请求Op的指针
        flags: {in}, flags定义包括：CEPHPROXY_OP_FLAG_*;
 * 返回值：无返回值
 */
void CephProxyWriteOpSetFlags(ceph_proxy_op_t op, int flags);

/*
 * 功能描述: 确保对象在写之前是存在的
 * 参数: op: {in}, 请求Op的指针
 * 返回值：无返回值
 */
void CephProxyWriteOpAssertExists(ceph_proxy_op_t op);

/*
 * 功能描述: 确保对象在写之前是存在的，且内部版本等于ver
 * 参数: op: {in}, 请求Op的指针
 * 返回值：如果对象的版本小于断言的版本，则在请求提交时将返回-EOVERFLOW，而不是执行op
 */
void CephProxyWriteOpAssertVersion(ceph_proxy_op_t op, uint64_t ver);

/*
 * 功能描述: 确保给定的对象的满足comparison，即比较对象某个范围的数据是否满足要求
 * 参数: op: {in}, 请求Op的指针
         cmpBuf: {in}, 用于比较的data buffer
         cmpLen: {in}, cmpBuf的长度
         off: {in}, 对象内的偏移地址
         prval: {out}, 0表示相同，非0表示不相同
 * 返回值：无返回值
 */
void CephProxyWriteOpCmpext(ceph_proxy_op_t op, const char *cmpBuf, size_t cmpLen, uint64_t off, int *prval);

/*
 * 功能描述: 对比扩展属性是否匹配
 * 参数: op: {in}, 请求Op的指针
         name: {in}, xattr name
         compOperator: {in}, 比较操作类型，定义为CEPHPROXY_CMPXATTR_OP_*
         value: {in}, 要与xattr进行比较的value
         valLen: {in}, value的长度
 * 返回值：无返回值，如果不匹配，则在请求提交后，返回-ECANCELED；
 */
void CephProxyWriteOpCmpXattr(ceph_proxy_op_t op, const char *name, uint8_t compOperator, const char *value, size_t valLen);

/*
 * 功能描述: 对比Omap
 * 参数: op: {in}, 请求Op的指针
         key: {in}, omap key
         compOperator: {in}, 比较操作类型，定义为CEPHPROXY_CMPXATTR_OP_*
         value: {in}, 要与omap进行比较的value
         valLen: {in}, value的长度
         prval: {out}, 保存对比的返回值
 * 返回值：无返回值
 */
void CephProxyWriteOpOmapCmp(ceph_proxy_op_t op, const char *key, uint8_t compOperator, const char *value, size_t valLen, int *prval);

/*
 * 功能描述: 设置一个xattr
 * 参数: op: {in}, 请求Op的指针
         name: {in}, xattr name
         value: {in}, xattr value
         valLen: {in}, value的长度
 * 返回值：无返回值
 */
void CephProxyWriteOpSetXattr(ceph_proxy_op_t op, const char *name, const char *value, size_t valLen);

/*
 * 功能描述: 删除一个xattr
 * 参数: op: {in}, 请求Op的指针
        name: {in}, xattr name
 * 返回值：无返回值
 */
void CephProxyWriteOpRemoveXattr(ceph_proxy_op_t op, const char *name);

/*
 * 功能描述: 创建一个对象
 * 参数: op: {in}, 请求Op的指针
        exclusive: {in}, 取值有PROXY_CREATE_*
        category: {in}，弃用
 * 返回值：无返回值
 */
void CephProxyWriteOpCreateObject(ceph_proxy_op_t op, int exclusive, const char *category);

/*
 * 功能描述: 对象写操作
 * 参数: op: {in}, 请求Op的指针
        buffer: {in}, 要写入的databuffer
        len: {in}, databuffer长度
        off: {in}, 要写入的对象的偏移
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyWriteOpWrite(ceph_proxy_op_t op, const char *buffer, size_t len, uint64_t off);

/*
 * 功能描述：对象写操作
 * 参数：op: {in}, 请求Op的指针
 *      bl: {in}, 要写入bufferlist指针
 *      len1: {in}, 数据长度
 *      off: {in}, 要写入的对象的偏移
 *      buffer: {in}, 对齐填充buffer
 *      len2: {in}, buffer长度
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyWriteOpWriteBl(ceph_proxy_op_t op, void *bl, size_t len1, uint64_t off, AlignBuffer *alignBuffer, int isRelease);

/*
 * 功能描述: 删除一个对象
 * 参数: op: {in}, 请求Op的指针
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyWriteOpRemove(ceph_proxy_op_t op);

/*
 * 功能描述: 为对象设置多个omap k/v;
 * 参数: op: {in}, 请求Op的指针
        keys: {in}, omap keys
        vals: {in}, omap vals;
        lens: {in}, values数组中每个value对应的长度
        num: {in}, 有多少个kv对
 * 返回值：无返回值
 */
void CephProxyWriteOpOmapSet(ceph_proxy_op_t op, char const* const* keys, char const* const* vals, const size_t *lens, size_t num);

/*
 * 功能描述: 删除对象中的多个omap k/v
 * 参数: op: {in}, 请求Op的指针
        keys: {in}, omap keys数组
        keysLen: {in}, keys的数量
 * 返回值：无返回值
 */
void CephProxyWriteOpOmapRmKeys(ceph_proxy_op_t op, char const* const* keys, size_t keysLen);

/*
 * 功能描述: 删除对象的所有Omap k/v
 * 参数: op: {in}, 请求Op的指针
 * 返回值：无返回值
 */
void CephProxyWriteOpOmapClear(ceph_proxy_op_t op);

/*
 * 功能描述: 为对象设置allocation hint
 * 参数: op: {in}, 请求Op的指针
        expectedObjSize: {in}, 对象预期的大小
        expectedWriteSize: {in}, 对象预期写入大小
        flags: {in}, allocation hint flags: 取值为CEPHPROXY_ALLOC_HINT_FLAG_*
 * 返回值：无返回值
 */
void CephProxyWriteOpSetAllocHint(ceph_proxy_op_t op, uint64_t expectedObjSize, uint64_t expectedWriteSize, uint32_t flags);

/*
 * 功能描述：初始化一个读类型请求
 * 参数说明：op: {out}, 请求Op的指针
            poolId: {in}, rados pool Id;
            oid: {in}, 对象ID
 * 返回值：0表示成功，非0表示失败
 */
PROXY_API_PUBLIC int CephProxyReadOpInit2(ceph_proxy_op_t *op, const int64_t poolId, const char *oid);

/*
 * 功能描述: 释放一个读请求
 * 参数: op: {in}, 请求Op的指针
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyReadOpRelease(ceph_proxy_op_t op);

/*
 * 功能描述: 为readOp设置flags
 * 参数: op: {in}, 请求Op的指针
        flags: {in}, flags, 取值为CEPHPROXY_OP_FLAG_*;
 * 返回值：无返回值
 */
void CephProxyReadOpSetFlags(ceph_proxy_op_t op, int flags);

/*
 * 功能描述: 在读操作之前，判断对象是否存在
 * 参数: op: {in}, 请求Op的指针
 * 返回值：无返回值
 */
void CephProxyReadOpAssertExists(ceph_proxy_op_t op);

/*
 * 功能描述: 在读操作之前，判断对象的版本是否等于ver
 * 参数: op: {in}, 请求Op的指针
         ver: {in}, 要比较的版本
 * 返回值：无返回值
 */
void CephProxyReadOpAssertVersion(ceph_proxy_op_t op, uint64_t ver);

/*
 * 功能描述: 比较对象中指定范围的数据是否满足comparison
 * 参数: op: {in}, 请求Op的指针
         cmpBuf: {in}, 用于比较的databuffer
         cmpLen: {in}, cmpBuf的长度
         off: {in}, 对象偏移
         prval: {out}, 0表示匹配，非0表示不匹配
 * 返回值：无返回值
 */
void CephProxyReadOpCmpext(ceph_proxy_op_t op, const char *cmpBuf, size_t cmpLen, uint64_t off, int *prval);

/*
 * 功能描述: 对比对象的扩展属性是否匹配
 * 参数: op: {in}, 请求Op的指针
         name: {in}, xattr name
         compOperator: {in}, 比较操作类型，取值为CEPHPROXY_CMPXATTR_OP_*
         value: {in}, 要比较的xattr的value
         valLen: {in}, value的长度
 * 返回值：无返回值
 */
void CephProxyReadOpCmpXattr(ceph_proxy_op_t op, const char *name, uint8_t compOperator, const char *value, size_t valueLen);

/*
 * 功能描述: 获取对象的所有扩展属性
 * 参数: op: {in}, 请求Op的指针
         iter: {out}, 保存扩展属性迭代器，通过CephProxyGetXattrsNext()访问
         prval: {in}, 返回值，0表示成功，非零表示失败
 * 返回值：无返回值
 */
void CephProxyReadOpGetXattrs(ceph_proxy_op_t op, proxy_xattrs_iter_t *iter, int *prval);

/*
 * 功能描述: 比较Omap
 * 参数: op: {in}, 请求Op的指针
         key: {in}, omap key
         compOperator: {in}, 取值CEPHPROXY_CMPXATTR_OP_*
         value: {in}, 用于对比的omap值
         valLen: {in}, value的大小
         prval: {out}, 保存对比的返回值
 * 返回值：无返回值
 */
void CephProxyReadOpOmapCmp(ceph_proxy_op_t op, const char *key, uint8_t compOperator, const char *val, size_t valLen, int *prval);

/*
 * 功能描述: 获取对象的mtime和size
 * 参数: op: {in}, 请求Op的指针
         psize: {out}, 对象大小
         pmtime: {out}, 对象的mtime
         prval: {out}, 返回0表示成功，非0表示失败，errno
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyReadOpStat(ceph_proxy_op_t op, uint64_t *psize, time_t *pmtime, int *prval);

/*
 * 功能描述: 对象读操作
 * 参数: op: {in}, 请求Op的指针
         offset: {in}, 对象偏移位置
         len: {in}, 读取长度
         buffer: {in}, 数据buffer，读取数据保存在该buffer中
         bytesRead：{out}, 实际读取到的长度
         prval: {out}, 返回0表示成功，非0表示失败，errno
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyReadOpRead(ceph_proxy_op_t op, uint64_t offset, size_t len, char *buffer, size_t *bytesRead, int *prval);

/*
 * 功能描述：对象读操作
 * 参数：op: {in}, 请求Op的指针
 *      offset: {in}, 对象偏移位置
 *      len: {in}, 对象读取长度
 *      bl: {in}, 数据保存的bufferlist结构体
 *      prval: {out}, 返回0表示成功，非0表示失败，errno
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyReadOpReadBl(ceph_proxy_op_t op, uint64_t offset, size_t len, void *bl, int *prval, int isRelease);

/*
 * 功能描述: 计算对象的校验和
 * 参数: op: {in}, 请求Op的指针
         type: {in}, 校验类型：取值为PROXY_CHECKSUM_TYPE_*
         initValue: {in}, 初始值 
         initValLen: {in}, initValue长度
         offset: {in}, 对象偏移
         len: {in}, 长度
         chunkSize: {in}, chunk大小
         pCheckSum: {out}, 校验和存储buffer
         checkSumLen: {in}, 校验和长度，用户指定
         prval: {out}, 返回0表示成功，非0表示失败，errno
 * 返回值：无返回值
 */
void CephProxyReadOpCheckSum(ceph_proxy_op_t op, proxy_checksum_type_t type, const char *initValue, size_t initValueLen,
                             uint64_t offset, size_t len, size_t chunkSize, char *pCheckSum, 
                             size_t checkSumLen, int *prval);

// void CephProxyReadOpExec(ceph_proxy_op_t  op, const char *cls, const char *method, const char *inBuf, size_t inLen, char  **outBuf, size_t *outLen, int *prval);
// void CephProxyReadOpExecUserBuf(ceph_proxy_op_t  op, const char *cls, const char *method, const char *inBuf, size_t inLen, char *outBuf, size_t outLen, size_t *usedLen, int *prval);

/*
 * 功能描述: 创建一个completion
 * 参数: fn: {in}, 回调函数
         arg: {in}, 回调函数参数

 * 返回值：返回一个completion，如果失败则返回nullptr
 */
PROXY_API_PUBLIC completion_t CephProxyCreateCompletion(CallBack_t fn, void *arg);

/*
 * 功能描述: 销毁一个completion
 * 参数: c: {in} completion
 * 返回值：无返回值
 */
PROXY_API_PUBLIC void CephProxyCompletionDestroy(completion_t c);

/*
 * 功能描述: 向CephProxy模块注册一个Pool删除回调函数
 * 参数: fn: {in} Pool删除回调函数
 * 返回值：无返回值
 */
int CephProxyRegisterPoolDelNotifyFn(NotifyPoolEventFn fn);

/*
 * 功能描述: 向CephProxy模块注册一个Pool创建回调函数
 * 参数: fn: {in} Pool创建回调函数
 * 返回值：无返回值
 */
PROXY_API_PUBLIC int CephProxyRegisterPoolNewNotifyFn(NotifyPoolEventFn fn);

PROXY_API_PUBLIC int CephProxyGetPoolInfo(ceph_proxy_t proxy, uint32_t poolId, struct PoolInfo *info);

#ifdef __cplusplus
}
#endif

#endif
