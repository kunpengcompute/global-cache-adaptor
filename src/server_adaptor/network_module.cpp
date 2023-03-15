/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#include "network_module.h"

#include <sys/types.h>
#include <iostream>
#include <string>
#include <sys/prctl.h>
#include <ctime>

#include "common/config.h"
#include "common/Timer.h"
#include "common/ceph_argparse.h"
#include "global/signal_handler.h"
#include "perfglue/heap_profiler.h"
#include "common/address_helper.h"
#include "auth/DummyAuth.h"
#include "msg/msg_types.h"
#include "messages/MPing.h"
#include "common/common_init.h"
#include "messages/MOSDOpReply.h"
#include "salog.h"
#include "sa_ftds_osa.h"

#define dout_subsys ceph_subsys_simple_client

using namespace std;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

namespace {
const string LOG_TYPE = "NETWORK";
const char *SA_THREAD_NAME = "gc_sa";
const int NUM_3 = 3;
const int NUM_32 = 32;
#ifdef SA_PERF
MsgPerfRecord *g_msgPerf { nullptr };
#endif
const uint32_t SA_THOUSAND_DEC = 1000;
const uint32_t COMMON_SLEEP_TIME_MS = 100;
}

struct Time {
    union {
        uint64_t nanoTime;
        struct {
            uint32_t s;
            uint32_t ns;
        } __attribute__ ((packed)) externTime;
    } time;
} __attribute__ ((packed));

static NetworkModule * g_networkModule = nullptr;

void *ThreadServer(void *arg)
{
    static_cast<NetworkModule *>(arg)->ThreadFuncBodyServer();
    return nullptr;
}

static int easy_readdir(const std::string &dir, std::set<std::string> *out)
{
    DIR *h = ::opendir(dir.c_str());
    if (!h) {
        return -errno;
    }
    struct dirent *de = nullptr;
    while ((de = ::readdir(h))) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }
        out->insert(de->d_name);
    }
    closedir(h);
    return 0;
}

void *ThreadFunc(NetworkModule *arg, int threadNum)
{
    arg->OpHandlerThread(threadNum);
    return nullptr;
}

int NetworkModule::InitNetworkModule(const std::string &rAddr, const std::vector<std::string> &rPort,
    uint32_t msgr, int *bind, bool readStatFlag)
{
    Salog(LV_DEBUG, LOG_TYPE, "Init network module.");
    int ret;
    if (ptrMsgModule == nullptr) {
        ptrMsgModule = new(std::nothrow) MsgModule();
        if (ptrMsgModule == nullptr) {
            Salog(LV_ERROR, LOG_TYPE, "memory alloc failed");
            return -ENOMEM;
        }
        Salog(LV_INFORMATION, LOG_TYPE, "read stat flag %d", readStatFlag);
        ptrMsgModule->SetReadStatFlag(readStatFlag);
    }
    recvAddr = rAddr;
    vecPorts = rPort;
    bindSuccess = bind;
    msgrNum = msgr;

    ret = InitMessenger();
    if (ret) {
        Salog(LV_DEBUG, LOG_TYPE, "error : Init messenger ret is %d", ret);
        return ret;
    }
#ifdef SA_PERF
    if (msgPerf == nullptr) {
        msgPerf = new(std::nothrow) MsgPerfRecord();
        if (msgPerf == nullptr) {
            Salog(LV_ERROR, LOG_TYPE, "memory alloc failed");
            return -ENOMEM;
        }
    }
    if (g_msgPerf == nullptr) {
        g_msgPerf = msgPerf;
    }
    msgPerf->start();
    Salog(LV_WARNING, LOG_TYPE, "SA_PERF open");
#endif
    return ret;
}

int NetworkModule::FinishNetworkModule()
{
    int ret = 0;
    ret = FinishMessenger();
    if (ret) {
        Salog(LV_DEBUG, LOG_TYPE, "FinishMessenger is failed ret=%d", ret);
    }
    Salog(LV_DEBUG, LOG_TYPE, "Finish network module.");
#ifdef SA_PERF
    msgPerf->stop();
    g_msgPerf = nullptr;
#endif
    return ret;
}

int NetworkModule::InitMessenger()
{
    int ret = 0;
    startServerThread = true;
    try {
        ret = pthread_create(&serverThread, nullptr, ThreadServer, this);
        if (ret) {
            Salog(LV_ERROR, LOG_TYPE, "Creating ThreadServer is failed ret=%d", ret);
            startServerThread = false;
            return ret;
        }
    } catch (const std::system_error& e) {
        ret = 200;
        Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
    } catch (const std::exception& e) {
        ret = 200;
        Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
    }
    return ret;
}

int NetworkModule::FinishMessenger()
{
    int ret = 0;
    for (auto &i : vecSvrMessenger) {
        i->shutdown();
        // 可能需要wait i->wait();
    }
    for (auto i : vecMsgThrottler) {
        delete i;
    }
    vecMsgThrottler.clear();

    for (auto i : vecByteThrottler) {
        delete i;
    }
    vecByteThrottler.clear();
    if (clientMessenger) {
        clientMessenger->shutdown();
        clientMessenger->wait();
    }
    Salog(LV_INFORMATION, LOG_TYPE, "Wait serverThread finish.");
    pthread_join(serverThread, nullptr);
    Salog(LV_INFORMATION, LOG_TYPE, "FinishMessenger ret=%d", ret);
    return ret;
}

int NetworkModule::ThreadFuncBodyServer()
{
    int r = 0;
    pid_t pid = getpid();
    DummyAuthClientServer dummy_auth(g_ceph_context);
    uint64_t messageSize = g_conf().get_val<Option::size_t>("osd_client_message_size_cap");
    uint64_t messageCap = g_conf().get_val<uint64_t>("osd_client_message_cap");
    Salog(LV_WARNING, LOG_TYPE, "messageSize=%lu messageCap=%lu", messageSize, messageCap);
    for (auto &i : vecPorts) {
        entity_addr_t bind_addr;
        string strPort = i;
        Salog(LV_WARNING, LOG_TYPE, "Server messanger is starting...");

        string dest_str = "tcp://";
        dest_str += recvAddr;
        dest_str += ":";
        dest_str += strPort;
        entity_addr_from_url(&bind_addr, dest_str.c_str());
        Salog(LV_WARNING, LOG_TYPE, "Messenger type is %s", g_conf().get_val<std::string>("ms_type").c_str());
        // async+posix
        Messenger *svrMessenger = Messenger::create(g_ceph_context, g_conf().get_val<std::string>("ms_type"),
            entity_name_t::OSD(-1), "simple_server", 0 /* nonce */, 0 /* flags */);

        svrMessenger->set_auth_server(&dummy_auth);
        svrMessenger->set_magic(MSG_MAGIC_TRACE_CTR);
        Throttle *clientByteThrottler = nullptr;
        Throttle *clientMsgThrottler = nullptr;
        clientByteThrottler = new(std::nothrow) Throttle(g_ceph_context, "osd_client_bytes", messageSize);
        clientMsgThrottler = new(std::nothrow) Throttle(g_ceph_context, "osd_client_messages", messageCap);
        if (clientByteThrottler == nullptr || clientMsgThrottler == nullptr) {
            Salog(LV_ERROR, LOG_TYPE, "Throttle memory alloc failed");
            r = -ENOMEM;
            *bindSuccess = 0;
            if (clientByteThrottler) {
                delete clientByteThrottler;
            }
            if (clientMsgThrottler) {
                delete clientMsgThrottler;
            }
            goto out;
        }
        svrMessenger->set_default_policy(Messenger::Policy::stateless_server(0));
        if (qosParam.enableThrottle) {
            Salog(LV_WARNING, LOG_TYPE, "set messenger throttlers.");
            svrMessenger->set_policy_throttlers(entity_name_t::TYPE_CLIENT, clientByteThrottler, nullptr);
            svrMessenger->set_policy_throttlers(entity_name_t::TYPE_CLIENT, clientMsgThrottler, nullptr);
        }
        bind_addr.set_type(entity_addr_t::TYPE_MSGR2); // another entity_addr_t::TYPE_LEGACY
        r = svrMessenger->bind(bind_addr);
        if (r < 0) {
            Salog(LV_ERROR, LOG_TYPE, "bind error %s:%s", recvAddr.c_str(), strPort.c_str());
            *bindSuccess = 0;
            delete clientByteThrottler;
            delete clientMsgThrottler;
            goto out;
        }

        SaServerDispatcher *svrDispatcher = nullptr;
        svrDispatcher = new(std::nothrow) SaServerDispatcher(svrMessenger, ptrMsgModule, this);
        if (svrDispatcher == nullptr) {
            Salog(LV_ERROR, LOG_TYPE, "memory alloc failed");
            r = -ENOMEM;
            *bindSuccess = 0;
            delete clientByteThrottler;
            delete clientMsgThrottler;
            goto out;
        }
        svrDispatcher->ms_set_require_authorizer(false);
        svrMessenger->add_dispatcher_head(svrDispatcher);
        svrMessenger->start();
        vecSvrMessenger.push_back(svrMessenger);
        vecDispatcher.push_back(svrDispatcher);
        vecByteThrottler.push_back(clientByteThrottler);
        vecMsgThrottler.push_back(clientMsgThrottler);
    }
    common_init_finish(g_ceph_context);

    if (bindMsgrCore) {
        BindMsgrWorker(pid);
        Salog(LV_WARNING, LOG_TYPE, "msgr-worker and ms_dispatch bind cores.");
    }

    if (!vecPorts.empty()) {
        Salog(LV_WARNING, LOG_TYPE, "ServerMessenger wait");
        *bindSuccess = 1;
        vecSvrMessenger[0]->wait();
    }
out:
    Salog(LV_WARNING, LOG_TYPE, "Server exit");
    return r;
}

void NetworkModule::BindMsgrWorker(pid_t pid)
{
    std::set<std::string> ls;
    char path[128] = {0};
    sprintf(path, "/proc/%d/task", pid);
    (void)easy_readdir(path, &ls);
    vector<uint64_t> vecBindMsgr;
    vector<uint64_t> vecBindDispatch;
    for (auto &i : ls) {
        string readPath = path;
        readPath += "/" + i + "/status";
        FILE *fp = fopen(readPath.c_str(), "r");
        if (NULL == fp) {
            Salog(LV_ERROR, LOG_TYPE, "open file error:%s", readPath.c_str());
            return;
        }
        if (!feof(fp)) {
            char line[200] = {0};
            memset(line, 0, sizeof(line));
            if (fgets(line, sizeof(line) - 1, fp) == NULL) {
                fclose(fp);
                continue;
            }
            string strLine = line;
            if (strLine.find("msgr-worker-") != string::npos) {
                // atoll i中保存的是线程号，上下文保证atoll不会出错
                vecBindMsgr.push_back(atoll(i.c_str()));
            }
            if (strLine.find("ms_dispatch") != string::npos) {
                // atoll i中保存的是线程号，上下文保证atoll不会出错
                vecBindDispatch.push_back(atoll(i.c_str()));
            }
        }
        fclose(fp);
    }
    sort(vecBindMsgr.rbegin(), vecBindMsgr.rend());
    sort(vecBindDispatch.rbegin(), vecBindDispatch.rend());
    vector<uint64_t> bindThreads;
    for (uint32_t i = 0; i < msgrNum; i++) {
        if (i < vecBindMsgr.size()) {
            bindThreads.push_back(vecBindMsgr[i]);
            Salog(LV_INFORMATION, LOG_TYPE, "SA bind msgr-worker : %llu", vecBindMsgr[i]);
        }
    }
    for (uint32_t i = 0; i < vecPorts.size(); i++) {
        if (i < vecBindDispatch.size()) {
            bindThreads.push_back(vecBindDispatch[i]);
            Salog(LV_INFORMATION, LOG_TYPE, "SA bind ms_dispatch : %llu", vecBindDispatch[i]);
        }
    }
    for (auto tid : bindThreads) {
        BindCore(tid);
    }
}

void NetworkModule::BindCore(uint64_t id)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (auto c : coreId) {
        CPU_SET(c, &mask);
    }
    pid_t tid = id;
    if (sched_setaffinity(tid, sizeof(mask), &mask) == -1) {
        Salog(LV_ERROR, LOG_TYPE, "setaffinity failed %ld", tid);
        return;
    }
}

void NetworkModule::CreateWorkThread(uint32_t qnum, uint32_t qmaxcapacity)
{
    if (g_networkModule == nullptr) {
        g_networkModule = this;
    }
    finishThread.clear();
    opDispatcher.clear();
    doOpThread.clear();
    queueNum = qnum;
    queueMaxCapacity = qmaxcapacity;

    for (uint64_t i = 0; i < queueNum; i++) {
        finishThread.push_back(false);
        ClientOpQueue *cq = nullptr;
        do {
            cq = new(std::nothrow) ClientOpQueue();
            if (cq) {
                break;
            }
            Salog(LV_ERROR, LOG_TYPE, "memory alloc failed");
            usleep(COMMON_SLEEP_TIME_MS * SA_THOUSAND_DEC);
        } while (cq == nullptr);
        opDispatcher.push_back(cq);
    }
    for (uint64_t i = 0; i < queueNum; i++) {
        try {
            doOpThread.push_back(thread(ThreadFunc, this, i));
        } catch (const std::system_error& e) {
            Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
            ceph_assert("Create thread catch std::system_error" == nullptr);
        } catch (const std::exception& e) {
            Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
            ceph_assert("Create thread catch std::exception" == nullptr);
        }
    }
    Salog(LV_WARNING, LOG_TYPE, "CreateWorkThread %d %d", queueNum, qmaxcapacity);
}

void NetworkModule::StopThread()
{
    for (uint32_t i = 0; i < finishThread.size(); i++) {
        try {
            std::unique_lock<std::mutex> opReqLock(opDispatcher[i]->opQueueMutex);
        } catch (const std::system_error& e) {
            if (e.code() == std::errc::operation_not_permitted) {
                Salog(LV_ERROR, LOG_TYPE, "there is no associated mutex");
            }
            if (e.code() == std::errc::resource_deadlock_would_occur) {
                Salog(LV_ERROR, LOG_TYPE, "the mutex is already locked by this unique_lock (in other words, owns_lock is true)");
            }
            Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
        } catch (const std::exception& e) {
            Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
        }
        finishThread[i] = true;
    }

    for (uint32_t i = 0; i < opDispatcher.size(); i++) {
        opDispatcher[i]->condOpReq.notify_all();
    }

    for (uint32_t i = 0; i < doOpThread.size(); i++) {
        doOpThread[i].join();
    }
}

void NetworkModule::OpHandlerThread(int threadNum)
{
    if (bindSaCore) {
        // 设置亲核
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for (auto c : workerCore) {
            CPU_SET(c, &mask);
        }
        if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
            Salog(LV_WARNING, LOG_TYPE, "setaffinity failed");
        }
        Salog(LV_INFORMATION, LOG_TYPE, "SA bind gc_sa : %d", threadNum);
    }
    prctl(PR_SET_NAME, SA_THREAD_NAME);
    int threadId = threadNum;
    ClientOpQueue *opDispatch = opDispatcher[threadId];
    std::queue<MOSDOp *> dealQueue;
    std::queue<uint64_t> dealTs;
    std::queue<uint64_t> periodTs;
    std::unique_lock<std::mutex> opReqLock;
    try {
        opReqLock = std::unique_lock<std::mutex>(opDispatch->opQueueMutex, std::defer_lock);
        if (opReqLock.owns_lock()) {
            Salog(LV_ERROR, LOG_TYPE, "owns_lock, no_lock");
        } else {
            opReqLock.lock();
        }
    } catch (const std::system_error& e) {
        Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
        if (e.code() == std::errc::operation_not_permitted) {
            Salog(LV_ERROR, LOG_TYPE, "there is no associated mutex");
        }
        if (e.code() == std::errc::resource_deadlock_would_occur) {
            Salog(LV_ERROR, LOG_TYPE, "the mutex is already locked by this unique_lock (in other words, owns_lock is true)");
        }
        sleep(1);
        ceph_assert("Lock queue mutex catch std::system_error 1" == nullptr);
    } catch (const std::exception& e) {
        Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
        sleep(1);
        ceph_assert("Lock queue mutex catch std::exception 1" == nullptr);
    }
    std::atomic<int> copyupFlag = 0;
    while (!finishThread[threadId]) {
        if (!opDispatch->Empty()) {
            opDispatch->reqQueue.swap(dealQueue);
            SaDatalog("queue_size_swaped %d", dealQueue.size());
            opDispatch->tsQueue.swap(dealTs);
            opDispatch->period.swap(periodTs);
            opDispatch->cond.notify_all();
            try {
                opReqLock.unlock();
            } catch (const std::system_error& e) {
                Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
                if (e.code() == std::errc::operation_not_permitted) {
                    Salog(LV_ERROR, LOG_TYPE, "there is no associated mutex");
                }
                if (e.code() == std::errc::resource_deadlock_would_occur) {
                    Salog(LV_ERROR, LOG_TYPE, "the mutex is already locked by this unique_lock (in other words, owns_lock is true)");
                }
                sleep(1);
                ceph_assert("Unlock queue mutex catch std::system_error 1" == nullptr);
            } catch (const std::exception& e) {
                Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
                sleep(1);
                ceph_assert("Unlock queue mutex catch std::exception 1" == nullptr);
            }
            // dealQueue 里面的MOSDOp已经经过finish_decode(), 并且经过ptId(op->get_pg().m_seed)区分
            while (!dealQueue.empty()) {
                SaOpReq *opreq = new(std::nothrow) SaOpReq;
                if (opreq == nullptr) {
                    SalogLimit(LV_ERROR, LOG_TYPE, "new nullptr");
                    usleep(COMMON_SLEEP_TIME_MS * SA_THOUSAND_DEC);
                    continue;
                }
                uint64_t ts = dealTs.front();
                dealTs.pop();
                if (ProcessOpReq(dealQueue, periodTs, opreq) != 0) {
                    delete opreq;
                    sa->FtdsEndHigt(SA_FTDS_OP_LIFE, "SA_FTDS_OP_LIFE", ts, 0);
                    continue;
                }
                GetlwtCas();// 增加lwt计数。如果达到上限，等若干ms。
                if (opreq->optionType == GCACHE_WRITE) {
                    GetWritelwtCas();
                    GetWriteIopsCas(1);
                    GetWriteBWCas(opreq->optionLength);
                }
                if (opreq->optionType == GCACHE_READ) {
                    GetReadlwtCas();
                    GetReadBWCas(opreq->optionLength);
                }
                while (copyupFlag.load() == 1) {
                    usleep(50);
                }
                if (unlikely(opreq->exitsCopyUp == 1)) {
                    SaDatalog("exists copyup, set copyupFlag, tid=%ld", opreq->tid);
                    copyupFlag = 1;
                    opreq->copyupFlag = &copyupFlag;
                }
                sa->DoOneOps(*opreq);
                // FinishCacheOps(opreq->ptrMosdop, 0); 用于测试 pass lwt、index、cache
                sa->FtdsEndHigt(SA_FTDS_OP_LIFE, "SA_FTDS_OP_LIFE", ts, 0);
            }
            uint64_t lockTsOne = 0;
            sa->FtdsStartHigh(SA_FTDS_LOCK_ONE, "SA_FTDS_LOCK_ONE", lockTsOne);
            try {
                opReqLock.lock();
            } catch (const std::system_error& e) {
                if (e.code() == std::errc::operation_not_permitted) {
                    Salog(LV_ERROR, LOG_TYPE, "there is no associated mutex");
                }
                if (e.code() == std::errc::resource_deadlock_would_occur) {
                    Salog(LV_ERROR, LOG_TYPE, "the mutex is already locked by this unique_lock (in other words, owns_lock is true)");
                }
                Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
                sleep(1);
                ceph_assert("Lock queue mutex catch std::system_error 2" == nullptr);
            } catch (const std::exception& e) {
                Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
                sleep(1);
                ceph_assert("Lock queue mutex catch std::exception 2" == nullptr);
            }
            sa->FtdsEndHigt(SA_FTDS_LOCK_ONE, "SA_FTDS_LOCK_ONE", lockTsOne, 0);
            continue;
        }
        try {
            opDispatch->condOpReq.wait(opReqLock);
        } catch (const std::system_error& e) {
            if (e.code() == std::errc::operation_not_permitted) {
                Salog(LV_ERROR, LOG_TYPE, "there is no associated mutex");
            }
            if (e.code() == std::errc::resource_deadlock_would_occur) {
                Salog(LV_ERROR, LOG_TYPE, "the mutex is already locked by this unique_lock (in other words, owns_lock is true)");
            }
            Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
            sleep(1);
            ceph_assert("Wait queue mutex catch std::system_error 1" == nullptr);
        } catch (const std::exception& e) {
            Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
            sleep(1);
            ceph_assert("Wait queue mutex catch std::exception 1" == nullptr);
        }
    }
    Salog(LV_WARNING, "OpHandler", "OpHandlerThread  Finish");
}

int NetworkModule::ProcessOpReq(std::queue<MOSDOp *> &dealQueue, std::queue<uint64_t> &periodTs, SaOpReq *opreq)
{
    MOSDOp *op = dealQueue.front();
    dealQueue.pop();

    uint64_t pts = periodTs.front();
    periodTs.pop();
    sa->FtdsEndHigt(SA_FTDS_QUEUE_PERIOD, "SA_FTDS_QUEUE_PERIOD", pts, 0);
    uint64_t transTs = 0;
    sa->FtdsStartHigh(SA_FTDS_TRANS_OPREQ, "SA_FTDS_TRANS_OPREQ", transTs);
    opreq->opType = OBJECT_OP;
    opreq->tid = op->get_tid();
    opreq->snapId = op->get_snapid();
    opreq->poolId = op->get_pg().pool() & 0xFFFFFFFFULL;
    opreq->ptVersion = op->get_pg().pool() >> 32;
    opreq->opsSequence = op->get_header().seq;
    opreq->ptrMosdop = op;
    opreq->ptId = op->get_pg().m_seed;
    opreq->snapSeq = op->get_snap_seq();
    for (auto &i : op->get_snaps()) {
        opreq->snaps.push_back(i.val);
    }

    SaDatalog("converted opreq :tid=%ld obj=%s poolId=%lu snapId=%lu snapSeq=%lu ptId=%u",
        opreq->tid, op->get_oid().name.c_str(), opreq->poolId, opreq->snapId, opreq->snapSeq, opreq->ptId);

    vector<char *> vecObj;
    const char *delim = ".";
    std::unique_ptr<char[]> tmp = std::make_unique<char[]>(op->get_oid().name.size() + 1);
    strcpy(tmp.get(), op->get_oid().name.c_str());
    char *p;
    char *savep;
    p = strtok_r(tmp.get(), delim, &savep);
    while (p) {
        vecObj.push_back(p);
        p = strtok_r(nullptr, delim, &savep);
    }
    bool isRbd = false;
    if (vecObj.empty() == false && strcmp(vecObj[0], "rbd_data") == 0) {
        if (vecObj.size() >= 3) {
            isRbd = true;
        } else {
            Salog(LV_CRITICAL, LOG_TYPE, "rbd_obj_id is %s, %d sections, this op return -EINVAL",
                op->get_oid().name.c_str(), vecObj.size());
            FinishCacheOps(op, opreq->optionType, opreq->optionLength, -EINVAL);
            return 1;
        }
    }
    OptionsType optionType = { 0 };
    OptionsLength optionLength = { 0 };
    string imageId = "1";
    if (isRbd) {
        imageId.append(vecObj[vecObj.size() - 2]);
    }
    for (auto &i : op->ops) {
        OpRequestOps oneOp;
        oneOp.objName = op->get_oid().name.c_str();
        if (isRbd) {
            oneOp.isRbd = isRbd;
            oneOp.rbdObjId.head = strtoull(imageId.c_str(), 0, 16);     // hex
            oneOp.rbdObjId.seq = strtoull(vecObj[vecObj.size() - 1], 0, 16); // hex
            oneOp.rbdObjId.version = SA_VERSION;
            oneOp.rbdObjId.format = FORMAT_UNSPECIFY_MD_DATE_POOL;
            oneOp.rbdObjId.poolId = 0;
            oneOp.rbdObjId.reserve = 0;
            if (vecObj.size() > NUM_3) {        // for specify data_pool
                uint64_t poolId = strtoul(vecObj[vecObj.size() - NUM_3], 0, 10);
                if ((poolId >> NUM_32) > 0) {
                    Salog(LV_ERROR, LOG_TYPE, "poolId %ld overflow", poolId);
                    return -1;
                }
                oneOp.rbdObjId.poolId = poolId;
                oneOp.rbdObjId.format = FORMAT_SPECIFY_MD_DATE_POOL;
            }
        }
        int exists_copy_up = GetMsgModule()->ConvertClientopToOpreq(i, oneOp, optionType, optionLength, opreq->tid);
        if (unlikely(exists_copy_up == 1)) {
            opreq->exitsCopyUp = 1;
        }
        opreq->vecOps.push_back(oneOp);
        SaDatalog("converted op :tid=%ld obj=%s head=%llu sequence=%llu isRbd=%d format=%u md_poolId=%u ptid=%d",
            opreq->tid, op->get_oid().name.c_str(), oneOp.rbdObjId.head, oneOp.rbdObjId.seq,
            isRbd, oneOp.rbdObjId.format, oneOp.rbdObjId.poolId, opreq->ptId);
    }

    if (optionType.write == 0) {
        SaDatalog("converted optype :tid=%ld obj=%s => READ", opreq->tid, op->get_oid().name.c_str());
        opreq->optionType = GCACHE_READ;
        opreq->optionLength = optionLength.read;
    } else {
        SaDatalog("converted optype :tid=%ld obj=%s => WRITE", opreq->tid, op->get_oid().name.c_str());
        opreq->optionType = GCACHE_WRITE;
        opreq->optionLength = optionLength.write;
    }
    sa->FtdsEndHigt(SA_FTDS_TRANS_OPREQ, "SA_FTDS_TRANS_OPREQ", transTs, 0);
    return 0;
}

uint32_t NetworkModule::EnqueueClientop(MOSDOp *opReq)
{
    string source;
    StatOpStart(opReq);
    int ret = 0;
    uint64_t ts = 0;
    sa->FtdsStartHigh(SA_FTDS_OP_LIFE, "SA_FTDS_OP_LIFE", ts);
    uint64_t enqueTs = 0;
    sa->FtdsStartHigh(SA_FTDS_MOSDOP_ENQUEUE, "SA_FTDS_MOSDOP_ENQUEUE", enqueTs);

    size_t idx = std::hash<std::string> {}(opReq->get_oid().name) % queueNum;
    std::unique_lock<std::mutex> opReqLock;
    try {
        opReqLock = std::unique_lock<std::mutex>(opDispatcher[idx]->opQueueMutex, std::defer_lock);
        if (opReqLock.owns_lock()) {
            Salog(LV_ERROR, LOG_TYPE, "owns_lock, no_lock");
        } else {
            opReqLock.lock();
        }
    } catch (const std::system_error& e) {
        Salog(LV_ERROR, LOG_TYPE, "std::system_error %s", e.what());
        if (e.code() == std::errc::operation_not_permitted) {
            Salog(LV_ERROR, LOG_TYPE, "there is no associated mutex");
        }
        if (e.code() == std::errc::resource_deadlock_would_occur) {
            Salog(LV_ERROR, LOG_TYPE, "the mutex is already locked by this unique_lock (in other words, owns_lock is true)");
        }
        sleep(1);
        ceph_assert("Lock queue mutex catch std::system_error 1" == nullptr);
    } catch (const std::exception& e) {
        Salog(LV_ERROR, LOG_TYPE, "std::exception %s", e.what());
        sleep(1);
        ceph_assert("Lock queue mutex catch std::exception 1" == nullptr);
    }

    if (opDispatcher[idx]->GetSize() > queueMaxCapacity) {
        SalogLimit(LV_WARNING, LOG_TYPE, "%d queue_capacity_is_large. %d", idx, opDispatcher[idx]->GetSize());
        opDispatcher[idx]->cond.wait(opReqLock);
    }
    uint64_t periodTs = 0;
    sa->FtdsStartHigh(SA_FTDS_QUEUE_PERIOD, "SA_FTDS_QUEUE_PERIOD", periodTs);
    opDispatcher[idx]->EnQueue(opReq, ts, periodTs);
    SaDatalog("MOSDOp is in the queue. tid=%ld obj=%s vec_index=%ld",
        opReq->get_tid(), opReq->get_oid().name.c_str(), idx);
    sa->FtdsEndHigt(SA_FTDS_MOSDOP_ENQUEUE, "SA_FTDS_MOSDOP_ENQUEUE", enqueTs, 0);

    uint64_t qosTs = 0;
    sa->FtdsStartHigh(SA_FTDS_WRITE_QOS, "SA_FTDS_WRITE_QOS", qosTs);
    if (qosParam.limitWrite && ContainWriteOp(*opReq)) {
        LimitWrite(*opReq);
    }
    sa->FtdsEndHigt(SA_FTDS_WRITE_QOS, "SA_FTDS_WRITE_QOS", qosTs, 0);
    return ret;
}

void NetworkModule::StatOpStart(MOSDOp *opReq)
{
    Time tm;
    uint64_t ts = 0;
    if (opReq->get_flags() & CEPH_OSD_FLAG_WRITE) {
        sa->FtdsStartHigh(SA_FTDS_WRITE_OP_TOTAL, "SA_FTDS_WRITE_OP_TOTAL", ts);
    } else {
        sa->FtdsStartHigh(SA_FTDS_READ_OP_TOTAL, "SA_FTDS_READ_OP_TOTAL", ts);
    }
    tm.time.nanoTime = ts;
    utime_t cephTime;
    cephTime.tv.tv_sec = tm.time.externTime.s;
    cephTime.tv.tv_nsec = tm.time.externTime.ns;
    opReq->set_mtime(cephTime);
}

void NetworkModule::StatOpEnd(MOSDOp *opReq, int ret)
{
    utime_t cephTime = opReq->get_mtime();
    Time tm;
    tm.time.externTime.s = cephTime.tv.tv_sec;
    tm.time.externTime.ns = cephTime.tv.tv_nsec;
    uint64_t ts = tm.time.nanoTime;
    if (opReq->get_flags() & CEPH_OSD_FLAG_WRITE) {
        sa->FtdsEndHigt(SA_FTDS_WRITE_OP_TOTAL, "SA_FTDS_WRITE_OP_TOTAL", ts, ret);
    } else {
        sa->FtdsEndHigt(SA_FTDS_READ_OP_TOTAL, "SA_FTDS_READ_OP_TOTAL", ts, ret);
    }
}

bool NetworkModule::ContainWriteOp(const MOSDOp &op)
{
    for (auto &i : op.ops) {
        if (i.op.op == CEPH_OSD_OP_WRITEFULL || i.op.op == CEPH_OSD_OP_WRITE) {
            return true;
        }
    }
    return false;
}

void NetworkModule::SetQosParam(const QosParam &p)
{
    qosParam = p;
}
void NetworkModule::LimitWriteIops(const MOSDOp &op)
{
    // 多个Dispatch线程会同时调用该函数
    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(limitWriteIopsMtx);
    if (qosInfo.isGc == 0) {
        return;
    }
    high_resolution_clock::time_point nowTime = std::chrono::high_resolution_clock::now();
    milliseconds timeInterval;
    if (likely(nowTime >= iopsCycleBegin)) {
        timeInterval = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - iopsCycleBegin);
        Salog(LV_DEBUG,
            LOG_TYPE,
            "timeInterval=%ld cycleBegin=%ld nowTime=%ld",
            timeInterval.count(),
            iopsCycleBegin,
            nowTime);
    }
    uint64_t limitIops = sa->GetWriteIopsThrottle();
    while (timeInterval.count() >= qosParam.getQuotaCycle) {
        limitIops = sa->GetWriteIopsThrottle();
        if (qosParam.getQuotaCycle < SA_THOUSAND_DEC) {
            limitIops = limitIops / (SA_THOUSAND_DEC / qosParam.getQuotaCycle);
        } else {
            limitIops = limitIops * (qosParam.getQuotaCycle / SA_THOUSAND_DEC);
        }

        if (limitIops == 0) {
            Salog(LV_INFORMATION, LOG_TYPE, "writeRatio==0, stop, sleep %u ms", qosParam.getQuotaCycle);
            // 不阻塞锁 limitWriteCond.wait_for(lock, std::chrono::milliseconds(iWait));
            usleep(qosParam.getQuotaCycle * SA_THOUSAND_DEC);
            Salog(LV_INFORMATION, LOG_TYPE, "writeRatio==0, stop, finish sleep");
            continue;
        } else {
            periodIops = 0;
            iopsCycleBegin = std::chrono::high_resolution_clock::now();
            Salog(LV_DEBUG, LOG_TYPE, "writeRatio=%ld, collect write op len", qosInfo.writeRatio);
            break;
        }
    }
    for (auto &i : op.ops) {
        if (i.op.op == CEPH_OSD_OP_WRITEFULL || i.op.op == CEPH_OSD_OP_WRITE) {
            periodIops += 1;  // 转换成kb
        }
    }
    Salog(LV_DEBUG, LOG_TYPE, "periodBW=%lu,wcacheBW=%lu", periodIops, limitIops);
    if (periodIops >= limitIops) {
        nowTime = std::chrono::high_resolution_clock::now();
        timeInterval = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - iopsCycleBegin);
        Salog(LV_DEBUG,
            LOG_TYPE,
            "nowTime=%lu,cycleBegin=%lu,timeInterval=%lu",
            nowTime,
            iopsCycleBegin,
            timeInterval.count());
        if (timeInterval.count() <= qosParam.getQuotaCycle) {
            uint iWait = qosParam.getQuotaCycle - timeInterval.count();
            if (iWait != 0) {
                Salog(LV_INFORMATION, LOG_TYPE, "after collect, sleep %lu ms", iWait);
                // 不阻塞锁 limitWriteCond.wait_for(lock, std::chrono::milliseconds(iWait));
                usleep(iWait * SA_THOUSAND_DEC);
                Salog(LV_INFORMATION, LOG_TYPE, "after collect, finish sleep");
            }
        }
    }
}
void NetworkModule::LimitWriteBw(const MOSDOp &op)
{
    // 多个Dispatch线程会同时调用该函数
    std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>(limitWriteMtx);

    high_resolution_clock::time_point nowTime = std::chrono::high_resolution_clock::now();
    milliseconds timeInterval;
    if (likely(nowTime >= cycleBegin)) {
        timeInterval = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - cycleBegin);
        Salog(LV_DEBUG, LOG_TYPE, "timeInterval=%ld cycleBegin=%ld nowTime=%ld", timeInterval.count(),
            cycleBegin, nowTime);
    }
    while (timeInterval.count() >= qosParam.getQuotaCycle) {
        unsigned int poolId = op.get_pg().pool() & 0xFFFFFFFFULL;
        sa->GetWriteQuota(poolId, qosInfo);    // 调用wcache接口，MB转换成byte，转换成每个周期的带宽
        if (qosParam.getQuotaCycle < SA_THOUSAND_DEC) {
            wcacheBW = qosInfo.writeRatio / (SA_THOUSAND_DEC / qosParam.getQuotaCycle);
        } else {
            wcacheBW = qosInfo.writeRatio * (qosParam.getQuotaCycle / SA_THOUSAND_DEC);
        }

        if (qosInfo.writeRatio == 0) {
            Salog(LV_INFORMATION, LOG_TYPE, "writeRatio==0, stop, sleep %u ms", qosParam.getQuotaCycle);
            // 不阻塞锁 limitWriteCond.wait_for(lock, std::chrono::milliseconds(iWait));
            usleep(qosParam.getQuotaCycle * SA_THOUSAND_DEC);
            Salog(LV_INFORMATION, LOG_TYPE, "writeRatio==0, stop, finish sleep");
            continue;
        } else {
            periodBW = 0;
            cycleBegin = std::chrono::high_resolution_clock::now();
            Salog(LV_DEBUG, LOG_TYPE, "writeRatio=%ld, collect write op len", qosInfo.writeRatio);
            break;
        }
    }
    for (auto &i : op.ops) {
        if (i.op.op == CEPH_OSD_OP_WRITEFULL || i.op.op == CEPH_OSD_OP_WRITE) {
            periodBW += i.op.extent.length / 1024;  // 转换成kb
        }
    }
    Salog(LV_DEBUG, LOG_TYPE, "periodBW=%lu,wcacheBW=%lu", periodBW, wcacheBW);
    if (periodBW >= wcacheBW) {
        nowTime = std::chrono::high_resolution_clock::now();
        timeInterval = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - cycleBegin);
        Salog(LV_DEBUG, LOG_TYPE, "nowTime=%lu,cycleBegin=%lu,timeInterval=%lu", nowTime, cycleBegin, timeInterval.count());
        if (timeInterval.count()  <=  qosParam.getQuotaCycle) {
            uint iWait = qosParam.getQuotaCycle - timeInterval.count();
            if (iWait != 0) {
                Salog(LV_INFORMATION, LOG_TYPE, "after collect, sleep %lu ms", iWait);
                // 不阻塞锁 limitWriteCond.wait_for(lock, std::chrono::milliseconds(iWait));
                usleep(iWait * SA_THOUSAND_DEC);
                Salog(LV_INFORMATION, LOG_TYPE, "after collect, finish sleep");
            }
        }
    }
}
void NetworkModule::LimitWrite(const MOSDOp &op)
{
    LimitWriteIops(op);
    LimitWriteBw(op);
}
void NetworkModule::GetlwtCas(unsigned int c)
{
    if (qosParam.saOpThrottle == 0) {
        return;
    }
    uint64_t oldCount = lwtCount;
    while (!__sync_bool_compare_and_swap(&lwtCount, oldCount, oldCount + c)) {
        oldCount = lwtCount;
    }
    if (unlikely((oldCount % 500 == 0) && (oldCount > 0))) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "get lwtCount=%llu", oldCount);
    }
    while (unlikely(oldCount > qosParam.saOpThrottle)) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "%llu > %llu, sleep 10ms", oldCount, qosParam.saOpThrottle);
        usleep(10 * SA_THOUSAND_DEC);   // 等待其他op完成，等待期间lwtCount可能减少
        oldCount = lwtCount;
    }
}
void NetworkModule::PutlwtCas()
{
    if (qosParam.saOpThrottle == 0) {
        return;
    }
    uint64_t oldCount = lwtCount;
    while (!__sync_bool_compare_and_swap(&lwtCount, oldCount, oldCount - 1)) {
        oldCount = lwtCount;
    }
    if (unlikely((oldCount % 500 == 0) && (oldCount > 0))) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "put lwtCount=%llu", oldCount);
    }
}

void NetworkModule::GetWritelwtCas(unsigned int c)
{
    uint64_t writeOpThrottle = sa->GetWriteOpThrottle();
    if (writeOpThrottle == 0) {
        return;
    }
    uint64_t oldWriteCount = lwtWriteCount;
    while (!__sync_bool_compare_and_swap(&lwtWriteCount, oldWriteCount, oldWriteCount + c)) {
        oldWriteCount = lwtWriteCount;
    }

    while (unlikely(oldWriteCount > writeOpThrottle)) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "%llu > %llu, sleep 10ms", oldWriteCount, writeOpThrottle);
        usleep(10 * SA_THOUSAND_DEC);   // 等待其他op完成
        oldWriteCount = lwtWriteCount;
        writeOpThrottle = sa->GetWriteOpThrottle();
        if (writeOpThrottle == 0) {
            return;
        }
    }
}
void NetworkModule::PutWritelwtCas()
{
    if (lwtWriteCount == 0) {
        return;
    }
    uint64_t oldWriteCount = lwtWriteCount;
    while (!__sync_bool_compare_and_swap(&lwtWriteCount, oldWriteCount, oldWriteCount - 1)) {
        if (lwtWriteCount == 0) {
            return;
        }
        oldWriteCount = lwtWriteCount;
    }
}

void NetworkModule::GetReadlwtCas(unsigned int c)
{
    uint64_t readOpThrottle = sa->GetReadOpThrottle();
    if (readOpThrottle == 0) {
        return;
    }
    uint64_t oldReadCount = lwtReadCount;
    while (!__sync_bool_compare_and_swap(&lwtReadCount, oldReadCount, oldReadCount + c)) {
        oldReadCount = lwtReadCount;
    }

    while (unlikely(oldReadCount > readOpThrottle)) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "%llu > %llu, sleep 10ms", oldReadCount, readOpThrottle);
        usleep(10 * SA_THOUSAND_DEC);   // 等待其他op完成
        oldReadCount = lwtReadCount;
        readOpThrottle = sa->GetReadOpThrottle();
        if (readOpThrottle == 0) {
            return;
        }
    }
}
void NetworkModule::PutReadlwtCas()
{
    if (lwtReadCount == 0) {
        return;
    }
    uint64_t oldReadCount = lwtReadCount;
    while (!__sync_bool_compare_and_swap(&lwtReadCount, oldReadCount, oldReadCount - 1)) {
        if (lwtReadCount == 0) {
            return;
        }
        oldReadCount = lwtReadCount;
    }
}
void NetworkModule::GetWriteIopsCas(unsigned long int c)
{
    uint64_t writIopsThrottle = sa->GetWriteIopsThrottle();
    if (writIopsThrottle == 0) {
        return;
    }
    if (qosInfo.isGc == 0) {
        return;
    }
    uint64_t oldWriteIops = writeIops;
    while (!__sync_bool_compare_and_swap(&writeIops, oldWriteIops, oldWriteIops + c)) {
        oldWriteIops = writeIops;
    }

    while (unlikely(oldWriteIops > writIopsThrottle)) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "%llu > %llu, sleep 10ms", oldWriteIops, writIopsThrottle);
        usleep(10 * SA_THOUSAND_DEC);   // 等待其他op完成
        oldWriteIops = writeIops;
        writIopsThrottle = sa->GetWriteIopsThrottle();
        if (writIopsThrottle == 0) {
            return;
        }
    }
}
void NetworkModule::PutWriteIopsCas(unsigned long int c)
{
    if (writeIops < c) {
        return;
    }
    if (qosInfo.isGc == 0) {
        return;
    }
    uint64_t oldWriteIops = writeIops;
    while (!__sync_bool_compare_and_swap(&writeIops, oldWriteIops, oldWriteIops - c)) {
        if (writeIops < c) {
            return;
        }
        oldWriteIops = writeIops;
    }
}
void NetworkModule::GetWriteBWCas(unsigned long int c)
{
    uint64_t writeBWThrottle = sa->GetWriteBWThrottle();
    if (writeBWThrottle == 0) {
        return;
    }
    uint64_t oldWriteBW = writeBW;
    while (!__sync_bool_compare_and_swap(&writeBW, oldWriteBW, oldWriteBW + c)) {
        oldWriteBW = writeBW;
    }

    while (unlikely(oldWriteBW > writeBWThrottle)) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "%llu > %llu, sleep 10ms", oldWriteBW, writeBWThrottle);
        usleep(10 * SA_THOUSAND_DEC);   // 10: 等待其他op完成ms
        oldWriteBW = writeBW;
        writeBWThrottle = sa->GetWriteBWThrottle();
        if (writeBWThrottle == 0) {
            return;
        }
    }
}
void NetworkModule::PutWriteBWCas(unsigned long int c)
{
    if (writeBW < c) {
        return;
    }
    uint64_t oldWriteBW = writeBW;
    while (!__sync_bool_compare_and_swap(&writeBW, oldWriteBW, oldWriteBW - c)) {
        if (writeBW < c) {
            return;
        }
        oldWriteBW = writeBW;
    }
}

void NetworkModule::GetReadBWCas(unsigned long int c)
{
    uint64_t readBWThrottle = sa->GetReadBWThrottle();
    if (readBWThrottle == 0) {
        return;
    }
    uint64_t oldReadBW = readBW;
    while (!__sync_bool_compare_and_swap(&readBW, oldReadBW, oldReadBW + c)) {
        oldReadBW = readBW;
    }

    while (unlikely(oldReadBW > readBWThrottle)) {
        SalogLimit(LV_INFORMATION, LOG_TYPE, "%llu > %llu, sleep 10ms", oldReadBW, readBWThrottle);
        usleep(10 * SA_THOUSAND_DEC);   // 等待其他op完成
        oldReadBW = readBW;
        readBWThrottle = sa->GetReadBWThrottle();
        if (readBWThrottle == 0) {
            return;
        }
    }
}
void NetworkModule::PutReadBWCas(unsigned long int c)
{
    if (readBW < c) {
        return;
    }
    uint64_t oldReadBW = readBW;
    while (!__sync_bool_compare_and_swap(&readBW, oldReadBW, oldReadBW - c)) {
        if (readBW < c) {
            return;
        }
        oldReadBW = readBW;
    }
}

void FinishCacheOps(void *op, uint32_t optionType, uint64_t optionLength, int32_t r)
{
    MOSDOp *ptr = (MOSDOp *)(op);
    MOSDOpReply *reply = nullptr;
    if (ptr == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " finish. but mosdop is null");
        return;
    }
    do {
        reply = new(std::nothrow) MOSDOpReply(ptr, 0, 0, 0, false);
        if (reply) {
            break;
        }
        SalogLimit(LV_ERROR, LOG_TYPE, " memory alloc failed");
    } while (reply == nullptr);  // in LWT
    reply->claim_op_out_data(ptr->ops);
    reply->set_result(r);
    reply->add_flags(CEPH_OSD_FLAG_ACK | CEPH_OSD_FLAG_ONDISK);
    ConnectionRef con = ptr->get_connection();
    con->send_message(reply);
    if (likely(g_networkModule != nullptr)) {
        g_networkModule->StatOpEnd(ptr, r);
    }
    ptr->put();
    // 释放lwt计数
    if (likely(g_networkModule != nullptr)) {
        if (optionType == GCACHE_WRITE) {
            g_networkModule->PutWriteBWCas(optionLength);
            g_networkModule->PutWriteIopsCas(1);
            g_networkModule->PutWritelwtCas();
        }
        if (optionType == GCACHE_READ) {
            g_networkModule->PutReadBWCas(optionLength);
            g_networkModule->PutReadlwtCas();
        }
        g_networkModule->PutlwtCas();
    }
}

void SetOpResult(int i, int32_t ret, MOSDOp *op)
{
    if (op == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p is null, skip", op);
        return;
    }
    if ((uint32_t)i >= op->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, op->ops.size());
        return;
    }
    op->ops[i].rval = ret;
}

void ProcessBuf(const char *buf, uint32_t len, int cnt, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    if (ptr == nullptr || buf == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or buf %p is null, skip", ptr, buf);
        return;
    }
    encode(std::string_view(buf, len), ptr->ops[cnt].outdata);
}

void EncodeOmapGetkeys(const SaBatchKeys *batchKeys, int i, MOSDOp *mosdop)
{
    bufferlist bl;
    if (mosdop == nullptr || batchKeys == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or batchKeys %p is null, skip", mosdop, batchKeys);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    for (uint32_t j = 0; j < batchKeys->nums; j++) {
        encode(std::string_view(batchKeys->keys[j].buf, batchKeys->keys[j].len), bl);
    }
    encode(batchKeys->nums, mosdop->ops[i].outdata);
    Salog(LV_DEBUG, LOG_TYPE, "CEPH_OSD_OP_OMAPGETKEYS get key num=%d", batchKeys->nums);
    mosdop->ops[i].outdata.claim_append(bl);
    encode(false, mosdop->ops[i].outdata);
}

void EncodeOmapGetvals(const SaBatchKv *KVs, int i, MOSDOp *mosdop)
{
    bufferlist bl;
    if (mosdop == nullptr || KVs == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or KVs %p is null, skip", mosdop, KVs);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    Salog(LV_DEBUG, LOG_TYPE, "CEPH_OSD_OP_OMAPGETVALS get key num=%d", KVs->kvNum);
    for (uint32_t j = 0; j < KVs->kvNum; j++) {
        if (KVs->keys[j].buf && KVs->keys[j].len) {
            Salog(LV_DEBUG, LOG_TYPE, "CEPH_OSD_OP_OMAPGETVALS get key KVs->keys[j].buf=%s", KVs->keys[j].buf);
            encode(std::string_view(KVs->keys[j].buf, KVs->keys[j].len), bl);
        }
        if (KVs->values[j].buf && KVs->values[j].len) {
            Salog(LV_DEBUG, LOG_TYPE, "CEPH_OSD_OP_OMAPGETVALS get key KVs->keys[j].buf=%s", KVs->values[j].buf);
            encode(std::string_view(KVs->values[j].buf, KVs->values[j].len), bl);
        }
    }
    encode(KVs->kvNum, mosdop->ops[i].outdata);
    mosdop->ops[i].outdata.claim_append(bl);
    encode(false, mosdop->ops[i].outdata);
}

void EncodeOmapGetvalsbykeys(const SaBatchKv *keyValue, int i, MOSDOp *mosdop)
{
    map<string, bufferlist> out;
    if (mosdop == nullptr || keyValue == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or keyValue %p is null, skip", mosdop, keyValue);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    for (uint32_t j = 0; j < keyValue->kvNum; j++) {
        bufferlist value;
        string keys(keyValue->keys[j].buf, keyValue->keys[j].len);
        value.append(keyValue->values[j].buf, keyValue->values[j].len);
        out.insert(make_pair(keys, value));
    }
    encode(out, mosdop->ops[i].outdata);
}

void EncodeRead(uint64_t opType, unsigned int offset, unsigned int len, const char *buf, unsigned int bufLen, int i,
    MOSDOp *mosdop)
{
    if (mosdop == nullptr || buf == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or buf %p is null, skip", mosdop, buf);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    if (unlikely(opType == CEPH_OSD_OP_SPARSE_READ)) {
        std::map<uint64_t, uint64_t> extents;
        extents[offset] = len;
        encode(extents, mosdop->ops[i].outdata);
        encode(std::string_view(buf, bufLen), mosdop->ops[i].outdata);
    } else {
        mosdop->ops[i].outdata.append(buf, bufLen);
    }
}

void EncodeXattrGetXattr(const SaBatchKv *keyValue, int i, MOSDOp *mosdop)
{
    if (mosdop == nullptr || keyValue == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or keyValue %p is null, skip", mosdop, keyValue);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    mosdop->ops[i].outdata.clear();
    for (uint32_t j = 0; j < keyValue->kvNum; j++) {
        bufferptr ptr(keyValue->values[j].buf, keyValue->values[j].len);
        mosdop->ops[i].outdata.push_back(std::move(ptr));
    }
}

void EncodeXattrGetXattrs(const SaBatchKv *keyValue, int i, MOSDOp *mosdop)
{
    if (mosdop == nullptr || keyValue == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or keyValue %p is null, skip", mosdop, keyValue);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    map<string, bufferlist> out;
    bufferlist bl;
    for (uint32_t j = 0; j < keyValue->kvNum; j++) {
        bufferlist value;
        string keys(keyValue->keys[j].buf, keyValue->keys[j].len);
        value.append(keyValue->values[j].buf, keyValue->values[j].len);
        out.insert(make_pair(keys, value));
    }
    encode(out, bl);
    mosdop->ops[i].outdata.claim_append(bl);
}

void EncodeGetOpstat(uint64_t psize, time_t ptime, int i, MOSDOp *mosdop)
{
    if (mosdop == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p is null, skip", mosdop);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    encode(psize, mosdop->ops[i].outdata);
    encode(ptime, mosdop->ops[i].outdata);
}

void EncodeListSnaps(const ObjSnaps *objSnaps, int i, MOSDOp *mosdop)
{
    if (mosdop == nullptr || objSnaps == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p or objSnaps %p is null, skip", mosdop, objSnaps);
        return;
    }
    if ((uint32_t)i >= mosdop->ops.size()) {
        Salog(LV_ERROR, LOG_TYPE, " index %d >= %u overflow", i, mosdop->ops.size());
        return;
    }
    if (objSnaps->cloneInfoNum < 1) {
        Salog(LV_ERROR, LOG_TYPE, " cloneInfoNum at least 1");
        return;
    }
    obj_list_snap_response_t resp;
    resp.seq = 0;
    if (objSnaps->cloneInfoNum >= 2) {
        resp.seq = snapid_t(objSnaps->cloneInfos[objSnaps->cloneInfoNum - 2].cloneid);
    }

    for (uint32_t j = 0; j < objSnaps->cloneInfoNum - 1; j++) {
        clone_info ci;
        CloneInfo &CI = objSnaps->cloneInfos[j];
        ci.cloneid = snapid_t(CI.cloneid);
        ci.size = CI.objSize;

        for (uint32_t si = 0; si < CI.snapNum; si++) {
            ci.snaps.push_back(snapid_t(CI.snaps[si]));
        }

        for (uint32_t oi = 0; oi < CI.overlapNum; oi++) {
            ci.overlap.push_back(std::make_pair(CI.overlaps[oi][0], CI.overlaps[oi][1]));
        }
        resp.clones.push_back(ci);
    }

    if (objSnaps->cloneInfoNum >= 1) {
        clone_info ci;
        CloneInfo &CI = objSnaps->cloneInfos[objSnaps->cloneInfoNum - 1];
        ci.cloneid = snapid_t(-2);
        ci.size = CI.objSize;
        resp.clones.push_back(ci);
    }

    resp.encode(mosdop->ops[i].outdata);
}

