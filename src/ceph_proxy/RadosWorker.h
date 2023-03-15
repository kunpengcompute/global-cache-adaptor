/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef _CEPH_PROXY_RADOS_WORKER_H_
#define _CEPH_PROXY_RADOS_WORKER_H_

#include "CephProxyInterface.h"
#include "CephProxy.h"
#include "CephProxyLog.h"
#include "CephProxyOp.h"
#include "utils.h"

#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

#define WORKER_MAX_NUM 8

const unsigned PROXY_PAGE_SIZE = sysconf(_SC_PAGESIZE);
const unsigned long PAGE_MASK = ~(unsigned long)(PROXY_PAGE_SIZE - 1);

typedef enum {
    RADOSWORKER_INITED = 0,
    RADOSWORKER_RUNING = 1,
    RADOSWORKER_DOWNED = 2,
} WorkerState;

typedef enum {
    IOWORKER_INITED = 0,
    IOWORKER_RUNNING = 1,
    IOWORKER_STOP = 2,
} IOWorkerState;

class CephProxy;

pid_t proxy_gettid(void);

class MyThread {
private:
    pthread_t threadId;
    pid_t pid;
    vector<uint32_t> cpuIds;
    const char *threadName;

    void *entryWrapper()
    {
        int p = proxy_gettid();
        if (p > 0) {
            pid = p;
        }

        pthread_setname_np(pthread_self(), threadName);
        _set_affnitify(cpuIds);
        return entry();
    }
public:
    MyThread(const MyThread&) = delete;
    MyThread& operator=(const MyThread&) = delete;

    MyThread():threadId(0), threadName(NULL)
    {

    }

    virtual ~MyThread()
    {

    }

protected:
    virtual void *entry() = 0;
private:
    static void *_entryFunc(void *arg)
    {
        void *r = ((MyThread*)arg)->entryWrapper();
        return r;
    }
public:
    const pthread_t &GetThreadId() const
    {
        return threadId;
    }

    bool IsStarted() const
    {
        return threadId != 0;
    }

    bool AmSelf() const
    {
        return (pthread_self() == threadId);
    }

    int TryCreate(size_t stacksize)
    {

        pthread_attr_t *threadAttr = nullptr;
        pthread_attr_t threadAttrLoc;

        stacksize &= PAGE_MASK;
        if (stacksize) {
            threadAttr = &threadAttrLoc;
            pthread_attr_init(threadAttr);
            pthread_attr_setstacksize(threadAttr, stacksize);
        }

        int r = pthread_create(&threadId, nullptr, _entryFunc, (void *)this);

        if (threadAttr) {
            pthread_attr_destroy(threadAttr);
        }

        return r;
    }

    void Create(const char *name, size_t stacksize = 0)
    {
        threadName = name;
        int ret = TryCreate(stacksize);
        if (ret != 0) {
            ProxyDbgLogErr("Thread::try_create(): pthread_create failed with error: %d", ret);
        }
    }

    int Join(void **prval = 0)
    {
        if (threadId == 0) {
            return -EINVAL;
        }

        int status = pthread_join(threadId, prval);
        if (status != 0) {
            ProxyDbgLogErr("Thread::Join: pthread_jon failed with error: %d", status);
        }
        threadId = 0;
        return status;
    }

    int Detach()
    {
        return pthread_detach(threadId);
    }

    int _set_affnitify(const vector<uint32_t> &id)
    {
        cpu_set_t cpuSet;
        CPU_ZERO(&cpuSet);
        for (auto i : id) {
            CPU_SET(i, &cpuSet);
        }
        if (pthread_setaffinity_np(threadId, sizeof(cpuSet), &cpuSet) < 0) {
            return -errno;
        }

        return 0;
    }

    int SetAffinity(const vector<uint32_t> &id)
    {
        int r = _set_affnitify(id);
        return r;
    }

    bool IsStarted()
    {
        return threadId != 0;
    }

    bool AmSelf()
    {
        return (pthread_self() == threadId);
    }
};

struct RequestCtx {
    rados_ioctx_t ioctx;
    ceph_proxy_op_t op;
    completion_t comp;
    uint64_t ts;
public:
    RequestCtx() {

    }

    RequestCtx(rados_ioctx_t _ioctx, ceph_proxy_op_t _op, completion_t _comp):
      ioctx(_ioctx), op(_op), comp(_comp) {
    }

    RequestCtx(const RequestCtx &ctx) {
        ioctx = ctx.ioctx;
        op = ctx.op;
        comp = ctx.comp;
        ts = ctx.ts;
    }

    ~RequestCtx() {

    }
};

class RadosIOWorker {
public:
    std::condition_variable ioworkerEmptyCond;

    atomic<bool> ioworkerStop;

    CephProxy *proxy;
    uint32_t threadNum;
    ConcurrentQueue<RequestCtx *> **queue;
    Semaphore **sem;
    std::string workerName;

    class IOWorker : public MyThread
    {
    public:
        RadosIOWorker *ioworker;
        uint32_t index;
        IOWorker(RadosIOWorker *w, uint32_t idx): ioworker(w), index(idx) {

        }

        ~IOWorker() {

        }
    public:
        void *entry() override {
            return ioworker->OpHandler(index);
        }
    };

    std::vector<IOWorker *> ioThreads;

public:
    RadosIOWorker(CephProxy *_proxy, uint32_t tNum):
        ioworkerStop(false),
        proxy(_proxy),
        threadNum(tNum),
        queue(new ConcurrentQueue<RequestCtx *> *[threadNum]), sem(new Semaphore *[threadNum]),
        workerName("proxy_worker") {
        for (uint32_t i = 0; i < threadNum; i++) {
            ioThreads.push_back(new IOWorker(this, i));
            queue[i] = new ConcurrentQueue<RequestCtx *>();
            sem[i] = new Semaphore(0);
        }
    }

    ~RadosIOWorker() {
        for (auto t : ioThreads) {
            delete t;
        }
        for (uint32_t i = 0; i < threadNum; i++) {
            delete queue[i];
            delete sem[i];
        }
        if (queue) {
            delete queue;
        }
        if (sem) {
            delete sem;
        }
    }

    void StartProc() {
        for (auto ioThread : ioThreads) {
            ioThread->Create(workerName.c_str());
        }
    }

    void StopProc() {
        ioworkerStop = true;
        for (auto ioThread : ioThreads) {
            ioThread->Join();
        }
    }

    void SetAffinitify(vector<uint32_t> cpuIds) {
        if (!cpuIds.size()) {
            return;
        }
        ProxyDbgLogInfo("ioWorker bind %d core(s)", cpuIds.size());
        for (auto ioThread : ioThreads) {
            ioThread->SetAffinity(cpuIds);
        }
    }

    void WaitForEmpty() {
        ioworkerStop = true;
        for (uint32_t i = 0; i < threadNum; i++) {
            sem[i]->PostOne();
        }
    }

    int32_t Queue(ceph_proxy_op_t op, completion_t c);
    void *OpHandler(uint32_t index);
};

class RadosWorker {
private:
    CephProxy *proxy;
    std::shared_mutex ioworkersLock;
    std::map<int64_t, RadosIOWorker *> ioWorkers;
    std::vector<uint32_t> cpus;
    int workerNum;
public:
    RadosWorker(CephProxy *proxy, int wNum): proxy(proxy), workerNum(wNum) {
    }

    ~RadosWorker() {
        proxy = nullptr;
    }

    void Start(std::vector<uint32_t> cpuIDs) {
        ioWorkers.clear();
        cpus = cpuIDs;
    }

    void Stop() {
        std::shared_lock<std::shared_mutex> lock(ioworkersLock);
        for (std::map<int64_t, RadosIOWorker *>::iterator iter = ioWorkers.begin();
             iter != ioWorkers.end(); iter++) {
            iter->second->WaitForEmpty();
            iter->second->StopProc();
            delete (iter->second);
        }

        ioWorkers.clear();
    }

    RadosIOWorker *GetIOWorker(int64_t poolId) {
        std::shared_lock<std::shared_mutex> lock(ioworkersLock);
        auto radosIOWorkerIter = ioWorkers.find(poolId);
        if (radosIOWorkerIter != ioWorkers.end()) {
            return radosIOWorkerIter->second;
        }

        return nullptr;
    }

    RadosIOWorker *CreateIOWorker(int64_t poolId) {
        std::unique_lock<std::shared_mutex> lock(ioworkersLock);
        auto radosIOWorkerIter = ioWorkers.find(poolId);
        if (radosIOWorkerIter != ioWorkers.end()) {
            return radosIOWorkerIter->second;
        }

        auto radosIOWorker = new(std::nothrow) RadosIOWorker(this->proxy, workerNum);
        if (radosIOWorker == nullptr) {
            ProxyDbgLogErr("Allocate IOWorker Failed.");
        } else {
            ioWorkers[poolId] = radosIOWorker;
            radosIOWorker->StartProc();
            radosIOWorker->SetAffinitify(cpus);
        }

        return radosIOWorker;
    }

    int32_t Queue(ceph_proxy_op_t op, completion_t c) {
        RadosObjectOperation *operation = static_cast<RadosObjectOperation *>(op);
        if (operation == nullptr || c == nullptr) {
            ProxyDbgLogErr("operation %p or c %p is invalid", operation, c);
            return -1;
        }
        if (operation->poolId < 0) {
            ProxyDbgLogErr("invalid poolId: %ld", operation->poolId);
            return -1;
        }

        RadosIOWorker *radosIOWorker = GetIOWorker(operation->poolId);
        if (radosIOWorker == nullptr) {
            radosIOWorker = CreateIOWorker(operation->poolId);
            if (radosIOWorker == nullptr) {
                ProxyDbgLogErr("CreateIOWorker Failed.");
                return -1;
            }
        }

        return radosIOWorker->Queue(op, c);
    }
};

#endif