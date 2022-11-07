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

#ifndef SA_DEF_H
#define SA_DEF_H

#include <string>
#include <vector>
#include <atomic>

constexpr int MY_PID = 666;

constexpr uint32_t MAX_PT_NUMBER = 128;

constexpr int INDEX_ZERO = 0;
constexpr int INDEX_ONE = 1;
constexpr int INDEX_TWO = 2;
constexpr int INDEX_THREE = 3;

constexpr int OBJECT_OP = 0;
constexpr int POOL_OP = 1;

constexpr int GCACHE_DEFAULT = 0;
constexpr int GCACHE_WRITE = 1;
constexpr int GCACHE_READ = 2;

constexpr int SA_QOS_MAX_NUM = 200;

constexpr int SA_VERSION = 1;
enum {
    FORMAT_UNSPECIFY_MD_DATE_POOL = 0,
    FORMAT_SPECIFY_MD_DATE_POOL = 1,
};

struct OptionsType {
    uint32_t write;
    uint32_t read;
};
struct OptionsLength {
    uint64_t write;
    uint64_t read;
};

typedef struct {
    uint32_t poolId;
    uint64_t head;
    uint64_t seq : 40;
    uint64_t format : 8;
    uint64_t version : 8;
    uint64_t reserve : 8;
} __attribute__((packed, aligned(4))) RbdObjid;

struct OpRequestOps {
    uint64_t opSubType {0};

    std::string objName {};
    bool isRbd { false };
    RbdObjid rbdObjId;

    char *inData { nullptr };

    uint32_t inDataLen { 0 };

    char *outData { nullptr };

    uint32_t outDataLen { 0 };

    uint32_t objOffset { 0 };

    uint32_t objLength { 0 };

    uint32_t opFlags {0};

    std::vector<std::string> keys {};
    
    std::vector<std::string> values {};
    std::vector<int> subops {};

    std::vector<uint64_t> u64vals {};
    std::vector<int> cmpModes {};
};


struct SaOpReq {
    std::vector<OpRequestOps> vecOps;

    uint32_t optionType { 0 };
    uint64_t optionLength { 0 };
    uint64_t opType { 0 };

    uint64_t snapId { 0 };

    long tid { 0 };

    uint64_t opsSequence { 0 };
    void *ptrMosdop { nullptr };

    uint32_t ptId { 0 };

    uint64_t poolId { 0 };

    uint64_t ts { 0 };

    uint64_t snapSeq { 0 };
    std::vector<uint64_t> snaps {};

    int exitsCopyUp { 0 };

    std::atomic<int> *copyupFlag { nullptr };

    uint64_t ptVersion { 0 };
    SaOpReq &operator = (const struct SaOpReq &other)
    {
        if (this == &other) {
            return *this;
        }
        optionType = other.optionType;
        optionLength = other.optionLength;
        opType = other.opType;
        snapId = other.snapId;
        opsSequence = other.opsSequence;
        ptrMosdop = other.ptrMosdop;
        tid = other.tid;
        ptId = other.ptId;
        poolId = other.poolId;
        ts = other.ts;
        snapSeq = other.snapSeq;
        snaps = other.snaps;
        exitsCopyUp = other.exitsCopyUp;
        copyupFlag = other.copyupFlag;
        ptVersion = other.ptVersion;
        return *this;
    }

    SaOpReq(const struct SaOpReq &other)
    {
        optionType=other.optionType;
        optionLength = other.optionLength;
        opType=other.opType;
        snapId=other.snapId;
        opsSequence=other.opsSequence;
        ptrMosdop=other.ptrMosdop;
        tid = other.tid;
        ptId=other.ptId;
        poolId=other.poolId;
        ts=other.ts;
        snapSeq = other.snapSeq;
        snaps = other.snaps;
        exitsCopyUp = other.exitsCopyUp;
        copyupFlag = other.copyupFlag;
        ptVersion = other.ptVersion;
    }
    SaOpReq() {}
};

struct SaOpContext {
    struct SaOpReq *opReq;
    int opId;
    int (*cbFunc)(struct SaOpReq &opReq);
};

using PREFETCH_FUNC = int (*)(struct SaOpReq &opReq, OpRequestOps &osdop);

struct SaStr {
    uint32_t len;
    char *buf;
};

struct SaBatchKv {
    SaStr *keys;
    SaStr *values;
    uint32_t kvNum;
};

struct SaKeyValue {
    uint32_t klen;
    char *kbuf;
    uint32_t vlen;
    char *vbuf;
};

struct SaBatchKeys {
    SaStr *keys;
    uint32_t nums;
};

struct SaWcacheQosInfo {
    uint16_t ptNum;
    uint32_t writeRatio;
    uint16_t ptList[SA_QOS_MAX_NUM];
    uint32_t ptUseRatio[SA_QOS_MAX_NUM];
};

constexpr int GetQosMaxNum()
{
    return SA_QOS_MAX_NUM;
}

#endif

