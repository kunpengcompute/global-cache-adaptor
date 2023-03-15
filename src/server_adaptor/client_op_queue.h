/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef CLIENT_OP_QUEUE_H
#define CLIENT_OP_QUEUE_H
#include <queue>
#include <mutex>
#include <condition_variable>
#include <messages/MOSDOp.h>

class ClientOpQueue {
public:
    ClientOpQueue() {};
    ~ClientOpQueue() {};

    std::mutex opQueueMutex;
    std::condition_variable cond {};
    std::queue<MOSDOp *> reqQueue;
    std::queue<uint64_t> tsQueue;
    std::queue<uint64_t> period;
    void EnQueue(MOSDOp *opReq, uint64_t ts, uint64_t p)
    {
        reqQueue.push(opReq);
        tsQueue.push(ts);
        period.push(p);
        condOpReq.notify_all();
    }
    void DeQueue();
    bool Empty()
    {
        return reqQueue.empty();
    }

    size_t GetSize()
    {
        return reqQueue.size();
    }

    std::condition_variable condOpReq;
};
#endif
