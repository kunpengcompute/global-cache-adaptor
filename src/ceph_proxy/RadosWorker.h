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
const unsigned long PAGE_MASK = ~(unsigned long)(PROXY_PAGE_SIZE -1);

typedef enum {
    RADOSWORKER_INITED = 0,
    RADOSWORKER_RUNING = 1,
    RADOSWORKER_DOWNED = 2,
} WorkerState;

typedef enum {
    IOWORKER_INITED   = 0,
    IOWORKER_RUNNING  = 1,
    IOWORKER_STOP     = 2,
} IOWorkerState;

class CephProxy;

pid_t proxy_gettid(void);
int _set_affnitify(int id);

class MyThread {
private:
    pthread_t threadId;
    pid_t pid;
    int cpuId;
    const char *threadName;

    void *entryWrapper() {
	int p = proxy_gettid();
	if (p > 0) {
	    pid = p;
	}

	if (pid && cpuId >= 0) {
	    _set_affnitify(cpuId);
	}
	pthread_setname_np(pthread_self(), threadName);
	return entry();
    }
public:
    MyThread(const MyThread&) = delete;
    MyThread& operator=(const MyThread&) = delete;

    MyThread():threadId(0), cpuId(-1), threadName(NULL) {

    }

    virtual ~MyThread() {

    }

protected:
    virtual void *entry() = 0;
private:
    static void *_entryFunc(void *arg) {
	void *r = ((MyThread*)arg)->entryWrapper();
	return r;
    }
public:
    const pthread_t &GetThreadId() const {
	return threadId;
    }

    bool IsStarted() const {
	return threadId != 0;
    }

    bool AmSelf() const {
     	return (pthread_self() ==threadId);
    }

    int TryCreate(size_t stacksize) {

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

    void Create(const char *name, size_t stacksize = 0 ) {
	threadName = name;
	int ret = TryCreate(stacksize);
	if (ret != 0) {
	    ProxyDbgLogErr("Thread::try_create(): pthread_create failed with error: %d", ret);
	}
    }

   int Join(void **prval = 0) {
	if(threadId == 0) {
	    return -EINVAL;
	}

	int status = pthread_join(threadId, prval);
	if (status != 0) {
	    ProxyDbgLogErr("Thread::Join: pthread_jon failed with error: %d", status);
	}
	threadId = 0;
	return status;
    }

    int Detach() {
	return pthread_detach(threadId);
    }

    int SetAffinity(int id) {
	int r = 0;
	cpuId = id;
	if (pid && proxy_gettid() == pid) {
	    r = _set_affnitify(id);
	}
	return r;
    }

    bool IsStarted() {
	return threadId != 0;
    }

    bool AmSelf() {
	return (pthread_self() == threadId);
    }
};

struct RequestCtx {
    rados_ioctx_t ioctx;
    ceph_proxy_op_t op;
    completion_t comp;
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
    }

    ~RequestCtx() {
    
    }
};

class RadosIOWorker {
public:
    std::mutex ioworkerLock;
    std::condition_variable ioworkerCond;
    std::condition_variable ioworkerEmptyCond;

    bool ioworkerStop;
    bool ioworkerRunning;
    bool ioworkerEmptyWait;

    CephProxy *proxy;
    std::vector<RequestCtx> Ops;
    std::string workerName;

    class IOWorker : public MyThread {
    public:
	RadosIOWorker *ioworker;
	IOWorker(RadosIOWorker *w): ioworker(w) {

	}
	
	~IOWorker() {

	}
    public:
	void *entry() override {
	    return ioworker->OpHandler();
	}
    } ioThread;

public:
    RadosIOWorker(CephProxy *_proxy):
	ioworkerStop(false),ioworkerRunning(false),
	ioworkerEmptyWait(false), proxy(_proxy),
	workerName("ioworker"), ioThread(this){
    }

    ~RadosIOWorker() {

    }

    void StartProc() {
	ioThread.Create(workerName.c_str());
    }

    void StopProc() {
	ioworkerLock.lock();
	ioworkerStop = true;
	ioworkerCond.notify_all();
	ioworkerLock.unlock();
	ioThread.Join();
    }

    void SetAffinitify(int cpuId) {
	ioThread.SetAffinity(cpuId);
    }

    void WaitForEmpty() {
	std::unique_lock<std::mutex> ul(ioworkerLock);
	while(!Ops.empty() || ioworkerRunning) {
	    ioworkerEmptyWait = true;
	    ioworkerEmptyCond.wait(ul);
	}
	ioworkerEmptyWait = false;
    }

    int32_t Queue(ceph_proxy_op_t op, completion_t c);
    void *OpHandler();
};

class RadosWorker {
private:
    CephProxy *proxy;
    std::shared_mutex ioworkersLock;
    std::map<int64_t, RadosIOWorker *> ioWorkers;
public:
    RadosWorker(CephProxy *proxy): proxy(proxy) {
    }

    ~RadosWorker() {
	proxy = nullptr;
    }

    void Start(std::vector<uint32_t> cpuIDs) {
	ioWorkers.clear();
    }

    void Stop() {
	for ( std::map<int64_t, RadosIOWorker *>::iterator iter = ioWorkers.begin();
		       iter != ioWorkers.end(); iter++)	{
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

	auto radosIOWorker = new(std::nothrow) RadosIOWorker(this->proxy);
	if (radosIOWorker == nullptr) {
	    ProxyDbgLogErr("Allocate IOWorker Failed.");
	} else {
	    ioWorkers[poolId] = radosIOWorker;
	    radosIOWorker->StartProc();
	}

	return radosIOWorker;
   }

    int32_t Queue(ceph_proxy_op_t op, completion_t c) {
		RadosObjectOperation *operation = reinterpret_cast<RadosObjectOperation *>(op);
		if (operation == nullptr || c == nullptr) {
			ProxyDbgLogErr("operation %p or c %p is invalid", operation, c);
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
