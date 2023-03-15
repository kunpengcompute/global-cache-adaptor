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

#include "utils.h"

#ifdef HAVE_SCHED
#include <sched.h>
#endif

#ifdef __linux__
#include <sys/syscall.h>
#endif

pid_t proxy_gettid(void)
{
#ifdef __linux__
    return syscall(SYS_gettid);
#else
    return -ENOSYS;
#endif
}

int32_t RadosIOWorker::Queue(ceph_proxy_op_t op, completion_t c)
{
    RadosObjectOperation *operation = static_cast<RadosObjectOperation *>(op);
    RequestCtx *reqCtx = new(std::nothrow) RequestCtx();
    if (reqCtx == nullptr) {
        ProxyDbgLogErr("no memory, return -ENOMEM");
        return -ENOMEM;
    }
    uint64_t ts = 0;
    PROXY_FTDS_START_HIGH(PROXY_FTDS_OPS_WAITQ, ts);
    reqCtx->op = op;
    reqCtx->comp = c;
    uint32_t index = static_cast<uint32_t>(std::hash<std::string>{}(operation->objectId)) % threadNum;
    reqCtx->ts = ts;
    queue[index]->Enqueue(reqCtx);
    sem[index]->PostOne();
    return 0;
}

void* RadosIOWorker::OpHandler(uint32_t index) {
    while (!ioworkerStop) {
        sem[index]->Wait();
        RequestCtx *ctx;
        while (queue[index]->Dequeue(&ctx)) {
            uint64_t ts = 0;
            int32_t ret = 0;
            PROXY_FTDS_END_HIGH(PROXY_FTDS_OPS_WAITQ, ctx->ts, ret);
            PROXY_FTDS_START_HIGH(PROXY_FTDS_OPS_CLIENT_SIDE, ts);
            Completion *c = static_cast<Completion *>(ctx->comp);
            RadosObjectOperation *operation = static_cast<RadosObjectOperation *>(ctx->op);
            delete ctx;
            rados_client_t radosClient;
            rados_ioctx_t ioctx = proxy->GetIoCtxWithClient(operation->poolId, &radosClient);
            if (ioctx == nullptr) {
                ProxyDbgLogWarnLimit1("Get IOCtx(%u) Failed.", operation->poolId);
                c->fn(-ENOENT, c->cbArg);
                PROXY_FTDS_END_HIGH(PROXY_FTDS_OPS_CLIENT_SIDE, ts, -ENOENT);
                continue;
            }

            ret = RadosOperationAioOperate(radosClient, operation, ioctx, c->fn, c->cbArg);
            if (ret < 0) {
                ProxyDbgLogErr("Rados Aio operate failed: %d", ret);
                c->fn(ret, c->cbArg);
            }
            PROXY_FTDS_END_HIGH(PROXY_FTDS_OPS_CLIENT_SIDE, ts, ret);
        }
    }

    return (void *)NULL;
}