/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#include "RadosWorker.h"
#include "CephProxyOp.h"
#include "RadosWrapper.h"
#include "CephProxyFtds.h"

#include <pthread.h>
#include <string>
#include <vector>
#include <utility>

#ifdef HAVE_SCHED
#include <sched.h>
#endif

#ifdef __linux__
#include <sys/syscall.h>
#endif

#define IO_WORKER_QUEUE_MAX_COUNT 65535

pid_t proxy_gettid(void)
{
#ifdef __linux__
	return syscall(SYS_gettid);
#else
	return -ENOSYS;
#endif
}

int _set_affnitify(int id)
{
#ifdef HAVE_SCHED
    if (id >= 0 && id < CPU_SETSIZE) {
	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);

	CPU_SET(id, &cpuSet);
	if (sched_setaffinity(0, sizeof(cpuSet), &cpuSet) < 0) {
		return -errno;
	}
	sched_yield();
    }
#endif
    return 0;
}

int32_t RadosIOWorker::Queue(ceph_proxy_op_t op, completion_t c)
{
    std::unique_lock ul(ioworkerLock);

    RequestCtx reqCtx;
    reqCtx.op = op;
    reqCtx.comp = c;

    RadosObjectOperation *operation = reinterpret_cast<RadosObjectOperation *>(op);
    if (Ops.size() > IO_WORKER_QUEUE_MAX_COUNT) {
	ProxyDbgLogErr("Too many requests are stacked in the IOWorker Queue, ops.size = %d, poolId: %d.",
			Ops.size(), operation->poolId);
	return -1;
    }

    if (Ops.empty()) {
	ioworkerCond.notify_all();
    }

    Ops.push_back(reqCtx);
    return 0;
}

void* RadosIOWorker::OpHandler() {
    std::unique_lock ul(ioworkerLock);

    while(!ioworkerStop) {
	while(!Ops.empty()) {
		uint64_t ts = 0;
	    int32_t ret = 0;
		PROXY_FTDS_START_HIGH(PROXY_FTDS_OPS_WAITQ, ts);
	    std::vector<RequestCtx> ls;
	    ls.swap(Ops);
	    ioworkerRunning = true;
    	    ul.unlock();

	    for (auto opair : ls) {
  		Completion *c = static_cast<Completion *>(opair.comp);
    		RadosObjectOperation *operation = reinterpret_cast<RadosObjectOperation *>(opair.op);
		rados_ioctx_t ioctx = proxy->GetIoCtx2(operation->poolId);
		if (ioctx == NULL) {
		    ProxyDbgLogWarnLimit1("Get IOCtx(%u) Failed.", operation->poolId);
		    c->fn(-ENOENT, c->cbArg);
		    continue;
		}

		ret = RadosOperationAioOperate(proxy->radosClient, opair.op, ioctx, c->fn, c->cbArg);
		if (ret < 0 ) {
		    ProxyDbgLogErr("Rados Aio operate failed: %d", ret);
		    c->fn(ret, c->cbArg);
		    continue;
		}
	    }
	    ls.clear();

	    ul.lock();
	    ioworkerRunning = false;
		PROXY_FTDS_END_HIGH(PROXY_FTDS_OPS_WAITQ, ts, ret);
	}
	
	if (ioworkerEmptyWait) {
	    ioworkerEmptyCond.notify_all();
	}

	if (ioworkerStop == true) {
	    break;
	}

	ioworkerCond.wait(ul);
    }
    ioworkerEmptyCond.notify_all();
    ioworkerStop = false;
  
    return (void *)NULL;
}
