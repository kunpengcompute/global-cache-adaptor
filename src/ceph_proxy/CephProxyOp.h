/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef _CEPH_PROXY_OP_H_
#define _CEPH_PROXY_OP_H_

#include <errno.h>

#include "CephProxyInterface.h"
#include "rados/librados.hpp"
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <cstddef>

#define UNKNOW_POOL_ID -1
#define INVALID_POOL_ID -2

typedef void *rados_op_t;
typedef void *rados_omap_iter_t;

using namespace std;
using namespace librados;

typedef void(*userCallback_t)(int ret, void *arg);

struct RadosXattrsIter {
    RadosXattrsIter(): val(nullptr) {
        i = attrset.end();
    }
    
    ~RadosXattrsIter() {
        if (val) {
            free(val);
        }
        val = nullptr;
    }

    std::map<std::string, bufferlist> attrset;
    std::map<std::string, bufferlist>::iterator i;
    char *val;
};

struct RadosOmapIter {
    std::map<std::string, bufferlist> values;
    std::map<std::string, bufferlist>::iterator i;

    std::set<std::string> keys;
    std::set<std::string>::iterator ki;

    RadosOmapIter() {
        i = values.end();
        ki = keys.end();
    }

    ~RadosOmapIter() {

    }
};

class RadosObjectOperation{
public:
    CephProxyOpType opType;
    int64_t poolId;
    std::string poolName;
    std::string objectId;

    userCallback_t callback;
    void *cbArg;

    uint64_t ts;
public:
    RadosObjectOperation(CephProxyOpType _type, const string &pool, const string &oid):
        opType(_type), poolId(UNKNOW_POOL_ID), poolName(pool), objectId(oid) {

    }

    RadosObjectOperation(CephProxyOpType _type, int64_t _poolId, const string &oid):
        opType(_type), poolId(_poolId), poolName(""), objectId(oid) {

    }

    virtual ~RadosObjectOperation() {

    }
};

class RadosObjectReadOp : public RadosObjectOperation {
public:
    librados::ObjectReadOperation op;

    struct reqContext {
        struct _read {
            char *buffer = nullptr;
            size_t *bytesRead = nullptr;
        } read;

        struct _readbl {
	    size_t len = 0;
	    int buildType = 0;
            GcBufferList *bl = nullptr;
        } readbl;

        struct _xattr {
            const char *name = nullptr;
            char **vals = nullptr;
        } xattr;

        struct _omap {
            proxy_omap_iter_t iter = nullptr;
        } omap;

        struct _xattrs {
            proxy_xattrs_iter_t iter = nullptr;
        } xattrs;

        struct _checksum {
            char *pCheckSum = nullptr;
            size_t chunkSumLen = 0;
        } checksum;

        struct _exec {
            char **outBuf = nullptr;
            size_t *outLen = 0;
        } exec;
    }reqCtx;
    
    bufferlist bl;


    bufferlist results;
    int retVals;


    bufferlist checksums;
    int checksumRetvals;


    bufferlist execOut;
    int execOutRetVals;


    set<string> omapKeys;
    map<string, bufferlist> omap;
    bufferlist header;


    map<string, bufferlist> xattrs;

public:
    RadosObjectReadOp(const string& pool, const string &oid)
      : RadosObjectOperation(BATCH_READ_OP, pool, oid) {

    }

    RadosObjectReadOp(const int64_t poolId, const string &oid)
      : RadosObjectOperation(BATCH_READ_OP, poolId, oid) {

    }

    ~RadosObjectReadOp() {

    }
};

class RadosObjectWriteOp : public RadosObjectOperation {
public:
    librados::ObjectWriteOperation op;
    bool isRemove;
    bufferlist bl;
public:
    RadosObjectWriteOp(const string& pool, const string& oid)
      : RadosObjectOperation(BATCH_WRITE_OP, pool, oid), isRemove(false) {

    }

    RadosObjectWriteOp(const int64_t poolId, const string &oid)
      : RadosObjectOperation(BATCH_WRITE_OP, poolId, oid), isRemove(false) {

    }

    ~RadosObjectWriteOp() {

    }
};

struct Completion {
    userCallback_t fn;
    void *cbArg;
public:
    Completion(userCallback_t _fn, void *arg) : fn(_fn),cbArg(arg) {

    }

    virtual ~Completion() {

    }
};

completion_t CompletionInit(userCallback_t fn, void *cbArg);
void CompletionDestroy(completion_t c);

#endif
