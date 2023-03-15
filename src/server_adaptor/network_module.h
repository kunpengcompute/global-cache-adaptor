/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef NETWORK_MODULE_H
#define NETWORK_MODULE_H

#include <queue>
#include <pthread.h>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <messages/MOSDOp.h>
#include <messages/MOSDOpReply.h>

#include "sa_server_dispatcher.h"
#include "sa_def.h"
#include "client_op_queue.h"
#include "msg_perf_record.h"
#include "sa_export.h"

struct QosParam {
    uint32_t limitWrite { 0 };
    uint32_t getQuotaCycle { 200 }; //单位是ms
    uint32_t enableThrottle { 0 };
    uint64_t saOpThrottle { 5000 };
};

typedef struct CloneInfo {
    uint32_t cloneid;
    uint32_t objSize;
    uint32_t snapNum;
    uint32_t *snaps;
    uint32_t overlapNum;
    uint32_t (*overlaps)[2]; // 0-offset, 1-len
} CloneInfo;

typedef struct ObjSnaps {
    uint32_t seq;
    uint32_t cloneInfoNum;
    CloneInfo* cloneInfos;
} ObjSnaps;

class NetworkModule {
    SaExport *sa { nullptr };
    pthread_t serverThread { 0 };
    pthread_t clientThread { 0 };
    pthread_t transToOpreqThread { 0 };
    pthread_t sendOpreplyThread { 0 };

    bool startServerThread { false };
    bool startClientThread { false };
    bool startTranToOpreqThread { false };
    bool startSendOpreplyThread { false };

    entity_addr_t recvBindAddr;
    Messenger *clientMessenger { nullptr };

    entity_addr_t sendBindAddr;

    std::string recvAddr { "localhost" };
    std::string recvPort { "1234" };

    MsgModule *ptrMsgModule { nullptr };
    std::queue<MOSDOp *> qReadyTransToOpreq {};
    std::queue<MOSDOpReply *> qSendToClientAdaptor {};

    bool testPing { false };
    bool testMosdop { false };

    uint64_t queueNum { 0 };
    uint32_t queueMaxCapacity { 0 };
    std::vector<std::thread> doOpThread {};
    std::vector<ClientOpQueue *> opDispatcher {};
    std::vector<bool> finishThread {};
    std::vector<int> coreId;
    std::vector<int> workerCore;
    uint32_t msgrNum { 5 };
    uint32_t bindMsgrCore { 0 };
    uint32_t bindSaCore { 0 };

    MsgPerfRecord *msgPerf { nullptr };

    std::vector<Throttle *> vecMsgThrottler;
    std::vector<Throttle *> vecByteThrottler;

    std::vector<std::string> vecPorts;
    std::vector<Messenger *> vecSvrMessenger;
    std::vector<SaServerDispatcher *> vecDispatcher;

    int *bindSuccess { nullptr };

    QosParam qosParam;
    std::chrono::high_resolution_clock::time_point cycleBegin { };  // 周期开始时间
    std::chrono::high_resolution_clock::time_point iopsCycleBegin { };  // 周期开始时间
    uint64_t periodBW { 0 };  // 累加的带宽，单位是kb
    uint64_t wcacheBW { ULLONG_MAX };  // wcache可承受的带宽，单位是kb
    uint64_t periodIops { 0 }; // 累计iops
    SaWcacheQosInfo qosInfo { 0 };
    std::mutex limitWriteMtx;
    std::mutex limitWriteIopsMtx;
    std::condition_variable limitWriteCond {};

    volatile uint64_t lwtCount { 0 };
    std::mutex lwtCountMtx;
    volatile uint64_t lwtWriteCount { 0 };
    std::mutex lwtWriteCountMtx;
    volatile uint64_t lwtReadCount { 0 };
    std::mutex lwtReadCountMtx;
    volatile uint64_t writeBW { 0 };
    std::mutex writeBWMtx;
    volatile uint64_t readBW { 0 };
    std::mutex readBWMtx;
    volatile uint64_t writeIops { 0 };
    std::mutex writeIopsMtx;
 
    /* ****************************************************************************
     brief: 创建两个线程。一个用于接收ClientAdaptor的MOSDOp，一个用于发送MOSDOpReply。

     param: 无

     return:    zero success；
                non-zero failed。
    **************************************************************************** */
    int InitMessenger();

    /* ****************************************************************************
     brief: 结束与messenger之间的交互。

     param: 无

     return:    zero success；
                non-zero failed。
    **************************************************************************** */
    int FinishMessenger();

    void BindMsgrWorker(pid_t pid);

    void BindCore(uint64_t tid);

    int ProcessOpReq(std::queue<MOSDOp *> &dealQueue, std::queue<uint64_t> &periodTs, SaOpReq *opreq);

    bool ContainWriteOp(const MOSDOp &op);

public:
    NetworkModule() = delete;

    explicit NetworkModule(SaExport &p, std::vector<int> &vec, std::vector<int> &workerCoreVec,
        uint32_t bind, uint32_t bindsa)
    {
        coreId = vec;
        workerCore = workerCoreVec;
        bindMsgrCore = bind;
        bindSaCore = bindsa;
        sa = &p;
        cycleBegin = std::chrono::high_resolution_clock::now();
        iopsCycleBegin = std::chrono::high_resolution_clock::now();
    }

    ~NetworkModule()
    {
        if (ptrMsgModule) {
            delete ptrMsgModule;
        }
        for (auto &i : vecSvrMessenger) {
            if (i) {
                delete i;
            }
        }
        for (auto &i : vecDispatcher) {
            if (i) {
                delete i;
            }
        }
        if (clientMessenger) {
            delete clientMessenger;
        }
        if (msgPerf) {
            delete msgPerf;
        }
    }

    /* ****************************************************************************
     brief: 初始化Network module。传递参数。

     param: rAddr，输入，服务端地址
            rPort，输入，服务端端口
            sAddr，输入，客户端地址
            sPort，输入，客户端端口

     return:    zero success；
                non-zero failed。
    **************************************************************************** */
    int InitNetworkModule(const std::string &rAddr, const std::vector<std::string> &rPort, uint32_t msgr, int *bind,
        bool readStatFlag);

    /* ****************************************************************************
     brief: 结束Netmodule，释放资源，结束所有线程。

     param: 无

     return:    zero success；
                non-zero failed。
    **************************************************************************** */
    int FinishNetworkModule();

    /* ****************************************************************************
     brief: 接收MOSDOp的messenger。

     param: 无

     return:    zero success；
                non-zero failed。
    **************************************************************************** */
    int ThreadFuncBodyServer();

    void CreateWorkThread(uint32_t queueNum, uint32_t qmaxcapacity);
    void StopThread();
    void OpHandlerThread(int threadNum);
    MsgModule *GetMsgModule()
    {
        return ptrMsgModule;
    }
    uint32_t EnqueueClientop(MOSDOp *opReq);

    void SetQosParam(const QosParam &p);
    void LimitWrite(const MOSDOp &op);
    void LimitWriteBw(const MOSDOp &op);
    void LimitWriteIops(const MOSDOp &op);
    void GetlwtCas(unsigned int c = 1);
    void PutlwtCas();
    void GetWritelwtCas(unsigned int c = 1);
    void PutWritelwtCas();
    void GetReadlwtCas(unsigned int c = 1);
    void PutReadlwtCas();
    void GetWriteBWCas(unsigned long int c);
    void PutWriteBWCas(unsigned long int c);
    void GetReadBWCas(unsigned long int c);
    void PutReadBWCas(unsigned long int c);
    void GetWriteIopsCas(unsigned long int c);
    void PutWriteIopsCas(unsigned long int c);
    void StatOpStart(MOSDOp *opReq);
    void StatOpEnd(MOSDOp *opReq, int ret);
};

void FinishCacheOps(void *op, uint32_t optionType, uint64_t optionLength, int32_t r);
void ProcessBuf(const char *buf, uint32_t len, int cnt, void *p);

void EncodeOmapGetkeys(const SaBatchKeys *batchKeys, int i, MOSDOp *p);
void EncodeOmapGetvals(const SaBatchKv *KVs, int i, MOSDOp *mosdop);
void EncodeOmapGetvalsbykeys(const SaBatchKv *keyValue, int i, MOSDOp *mosdop);
void EncodeRead(uint64_t opType, unsigned int offset, unsigned int len, const char *buf, unsigned int bufLen, int i,
    MOSDOp *mosdop);
void SetOpResult(int i, int32_t ret, MOSDOp *op);
void EncodeXattrGetXattr(const SaBatchKv *keyValue, int i, MOSDOp *mosdop);
void EncodeXattrGetXattrs(const SaBatchKv *keyValue, int i, MOSDOp *mosdop);
void EncodeGetOpstat(uint64_t psize, time_t ptime, int i, MOSDOp *mosdop);
void EncodeListSnaps(const ObjSnaps *objSnaps, int i, MOSDOp *mosdop);
#endif
