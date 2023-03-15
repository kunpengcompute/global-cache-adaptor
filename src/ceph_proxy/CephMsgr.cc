/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 */

#include <string>
#include <vector>
#include <sstream>
#include <atomic>
#ifdef WITH_TESTS_ONLY
#include <iostream>
#endif
#include <shared_mutex>
#include <arpa/inet.h>

#include "include/buffer.h"
#include "common/ceph_json.h"
#include "common/common_init.h"
#include "common/config.h"
#include "common/address_helper.h"
#include "common/ceph_argparse.h"
#include "common/Mutex.h"
#include "common/Cond.h"
#include "msg/Messenger.h"
#include "msg/Dispatcher.h"
#include "messages/MOSDMap.h"
#include "mon/MonClient.h"
#include "mgr/MgrClient.h"
#include "osd/OSDMap.h"

#include "CephProxyLog.h"
#include "ConfigRead.h"
#include "CcmAdaptor.h"
#include "CephProxyInterface.h"
#include "CephMsgr.h"

const uint32_t DECIMAL_NOTATION = 10;
const uint32_t DEFAULT_NM_SLEEP_TIME = 10;
const uint32_t DEFAULT_ABNM_SLEEP_TIME = 2;
const uint32_t DEFAULT_M = 2;

class ClusterData {
public:
    mutable std::shared_mutex dataLock;
    uint64_t totalSizeKB;
    uint64_t rawUsedSizeKB;
    uint64_t dataUsedSizeKB;

    uint64_t allPoolUsed;
    uint64_t allPoolAvail;
    ClusterData() : totalSizeKB(0), rawUsedSizeKB(0), dataUsedSizeKB(0), allPoolUsed(0), allPoolAvail(0) {}
    ~ClusterData() {};
};

class ProxyDispatcher : public Dispatcher {
public:
    ProxyMsgr* proxyMsgr { nullptr };
    MonClient monclient;
    MgrClient mgrclient;
    Mutex ms_lock;
    ProxyDispatcher() = delete;
    explicit ProxyDispatcher(CephContext *cct, ProxyMsgr* proxyMsgr) : Dispatcher(cct), proxyMsgr(proxyMsgr),
        monclient(cct), mgrclient(cct, nullptr), ms_lock("proxy_dispatcher") {}
    ~ProxyDispatcher() override {};

    bool ms_dispatch(Message *m) override;
    void ms_handle_connect(Connection *con) override {};
    void ms_handle_accept(Connection *con) override {};
    bool ms_handle_reset(Connection *con) override
    {
        return true;
    }
    void ms_handle_remote_reset(Connection *con) override {};

    bool ms_handle_refused(Connection *con) override
    {
        return false;
    }
    bool ms_get_authorizer(int destType, AuthAuthorizer **a) override
    {
        return false;
    }
    int ms_handle_authentication(Connection *con) override
    {
        return 1;
    }
    int32_t SendMonCommand(const std::vector<std::string>& cmd, JSONParser *parser);
    int32_t SendMgrCommand(const std::vector<std::string>& cmd, JSONParser *parser);
};

class ProxyMsgr {
public:
    ClusterManagerAdaptor *ccmAdaptor;
    Messenger *msger;
    ProxyDispatcher* dispatcher;
    CephContext *m_cct;
    OSDMap *osdmap;
    bool needSubContinuous;
    mutable std::shared_mutex osdmapMutex;
    std::thread pollThread;
    std::atomic<bool> pollThreadFlag;
    C_SaferCond threadCV;

    ClusterData clusterData;

    ProxyMsgr() : ccmAdaptor(nullptr), msger(nullptr), dispatcher(nullptr), m_cct(nullptr),
        osdmap(new OSDMap), needSubContinuous(true) {}
    ~ProxyMsgr()
    {
        if (pollThreadFlag) {
            pollThreadFlag = false;
            threadCV.complete(0);
            pollThread.join();
        }
        if (dispatcher) {
            dispatcher->mgrclient.shutdown();
            dispatcher->monclient.shutdown();
            delete dispatcher;
            dispatcher = nullptr;
        }
        if (msger) {
            msger->shutdown();
            msger->wait();
            delete msger;
            msger = nullptr;
        }
        if (osdmap) {
            delete osdmap;
        }
        if (ccmAdaptor) {
            ccmAdaptor->RegisterPoolCreateReportFn(nullptr);
            delete ccmAdaptor;
            ccmAdaptor = nullptr;
        }
    }
    int32_t InitCCMAdaptor();
    int32_t InitCephContext();
    int32_t InitMessenger();
    int32_t InitDispatcher();
    void PollThread();
    int32_t InitPollThread();
    int32_t Start();
    void InitOsdMap(MOSDMap *m);
    void UpdateOSDMap(MOSDMap *m);
    void handle_osd_map(MOSDMap *m);
#ifdef WITH_TESTS_ONLY
    void debug_osdmap();
#endif
    void RenewOSDMap(epoch_t e);
    int32_t GetOsdDf();
    int32_t CephDf();

    int PoolChangeNotify();
    int RegisterPoolNotifyFn(NotifyPoolEventFn fn);
    int GetPoolAllUsedAndAvail(uint64_t &usedSize, uint64_t &maxAvail);
    int GetPoolBaseInfo(uint32_t poolId, struct PoolInfo *info);
    int32_t JsonParseCephDF(const std::string &pool, uint64_t &poolTotalUsed, uint64_t &poolTotalAvail);
};

bool ProxyDispatcher::ms_dispatch(Message *m)
{
    bool ret = true;
    ms_lock.Lock();         // need lock
    switch (m->get_type()) {
        case CEPH_MSG_OSD_MAP:
            this->proxyMsgr->handle_osd_map(static_cast<MOSDMap *>(m));
            m->put();
            break;
        default:
            ret = false;
            break;
    }
    ms_lock.Unlock();
    return ret;
}

int32_t ProxyDispatcher::SendMonCommand(const std::vector<std::string>& cmd, JSONParser *parser)
{
    C_SaferCond ctx;
    bufferlist outs;
    ms_lock.Lock();     // need lock
    monclient.start_mon_command(cmd, {}, &outs, NULL, &ctx);
    ms_lock.Unlock();
    int32_t ret = 0;
    if (proxyMsgr->m_cct->_conf->rados_mon_op_timeout) {
        ret = ctx.wait_for(proxyMsgr->m_cct->_conf->rados_mon_op_timeout);
    } else {
        ret = ctx.wait();
    }
    if (ret < 0) {
        return ret;
    }
    if (!outs.length()) {
        ProxyDbgLogErr("get none");
        return -ENOENT;
    }

    if (!parser->parse(outs.c_str(), outs.length())) {
        ProxyDbgLogErr("parse json failed, outs=%s", outs.c_str());
        return -EINVAL;
    }

    return ret;
}

int32_t ProxyDispatcher::SendMgrCommand(const std::vector<std::string>& cmd, JSONParser *parser)
{
    C_SaferCond ctx;
    bufferlist outs;
    ms_lock.Lock();     // need lock
    int r = mgrclient.start_command(cmd, {}, &outs, NULL, &ctx);
    ms_lock.Unlock();
    if (r < 0) {
        return r;
    }
    int32_t ret = 0;
    if (proxyMsgr->m_cct->_conf->rados_mon_op_timeout) {
        ret = ctx.wait_for(proxyMsgr->m_cct->_conf->rados_mon_op_timeout);
    } else {
        ret = ctx.wait();
    }
    if (ret < 0) {
        return ret;
    }
    if (!outs.length()) {
        ProxyDbgLogErr("get none");
        return -ENOENT;
    }

    if (!parser->parse(outs.c_str(), outs.length())) {
        ProxyDbgLogErr("parse json failed, outs=%s", outs.c_str());
        return -EINVAL;
    }

    return ret;
}

int32_t ProxyMsgr::InitCCMAdaptor()
{
    ccmAdaptor = new (std::nothrow) ClusterManagerAdaptor();
    if (ccmAdaptor == nullptr) {
        ProxyDbgLogErr("Allocate ClusterManagerAdaptor faild.");
        return -1;
    }
    return 0;
}

int ProxyMsgr::RegisterPoolNotifyFn(NotifyPoolEventFn fn)
{
    if (ccmAdaptor == nullptr) {
        ProxyDbgLogErr("ProxyMsgr is not inited.");
        return -1;
    }

    int ret = ccmAdaptor->RegisterPoolCreateReportFn(fn);
    if (ret != 0) {
        ProxyDbgLogErr("register poolCreateReport Fn failed");
        return -1;
    }

    return 0;
}

int ProxyMsgr::PoolChangeNotify()
{
    std::vector<uint32_t> pools;
    if (ccmAdaptor == nullptr) {
        return 0;
    }
    std::shared_lock<std::shared_mutex> lock(osdmapMutex);
    for (std::map<int64_t, pg_pool_t>::const_iterator it = osdmap->get_pools().begin();
        it != osdmap->get_pools().end(); it++) {
        if (it->first > std::numeric_limits<uint32_t>::max()) {
            ProxyDbgLogErr("poolId(%lld) overflow, skip", it->first);
            continue;
        }
        pools.push_back(static_cast<uint32_t>(it->first));
        ProxyDbgLogInfo("full notify poolId(%llu)", it->first);
    }
    lock.unlock();

    int ret = ccmAdaptor->ReportCreatePool(pools);
    if (ret != 0) {
        ProxyDbgLogErr("report pool failed, ret=%d", ret);
        return -1;
    }

    return 0;
}

int32_t ProxyMsgr::InitCephContext()
{
    CephInitParameters iparams(CEPH_ENTITY_TYPE_CLIENT);
    m_cct = common_preinit(iparams, CODE_ENVIRONMENT_LIBRARY, 0);

#ifdef WITH_TESTS_ONLY
    const string confPath = "/etc/ceph/ceph.conf";
    const string keyringPath = "/etc/ceph/ceph.client.admin.keyring";
#else
    const string confPath = ProxyGetCephConf();
    const string keyringPath = ProxyGetCephKeyring();
#endif
    int ret = m_cct->_conf.parse_config_files(confPath.c_str(), &cerr, 0);
    if (ret) {
        ProxyDbgLogErr("parse conf failed, ret=%d", ret);
        return ret;
    }
    m_cct->_conf.set_val("keyring", keyringPath.c_str());
    m_cct->_conf.parse_env(m_cct->get_module_type());
    m_cct->_conf.apply_changes(nullptr);
    m_cct->_conf.complain_about_parse_errors(m_cct);

    common_init_finish(m_cct);

    return ret;
}

int32_t ProxyMsgr::InitMessenger()
{
    msger = Messenger::create_client_messenger(m_cct, "proxyClient");
    if (msger == nullptr) {
        ProxyDbgLogErr("alloc messager failed");
        return -ENOMEM;
    }
    msger->set_default_policy(Messenger::Policy::lossy_client(CEPH_FEATURE_OSDREPLYMUX));

    return 0;
}

int32_t ProxyMsgr::InitDispatcher()
{
    dispatcher = new(std::nothrow) ProxyDispatcher(m_cct, this);
    if (dispatcher == nullptr) {
        ProxyDbgLogErr("dispatcher alloc failed");
        return -ENOMEM;
    }

    dispatcher->ms_set_require_authorizer(false);
    int32_t ret = dispatcher->monclient.build_initial_monmap();
    if (ret < 0) {
        ProxyDbgLogErr("build init monmap failed %d", ret);
        goto out;
    }
    dispatcher->monclient.set_messenger(msger);
    dispatcher->mgrclient.set_messenger(msger);
    msger->add_dispatcher_head(&dispatcher->mgrclient);
    msger->add_dispatcher_tail(dispatcher);
    msger->start();

    dispatcher->monclient.set_want_keys(CEPH_ENTITY_TYPE_OSD | CEPH_ENTITY_TYPE_MGR | CEPH_ENTITY_TYPE_MON);
    ret = dispatcher->monclient.init();
    if (ret) {
        ProxyDbgLogErr("init monc failed %d", ret);
        goto out;
    }
    ret = dispatcher->monclient.authenticate(m_cct->_conf->client_mount_timeout);
    if (ret) {
        ProxyDbgLogErr("auth monc error %d", ret);
        goto out;
    }

    dispatcher->mgrclient.set_mgr_optional(
        !dispatcher->monclient.monmap.get_required_features().contains_all(
            ceph::features::mon::FEATURE_LUMINOUS));

    dispatcher->monclient.sub_want("osdmap", 0, 0);
    dispatcher->monclient.sub_want("mgrmap", 0, 0);
    dispatcher->monclient.renew_subs();

    dispatcher->mgrclient.init();

    return 0;
out:
    if (dispatcher) {
        dispatcher->monclient.shutdown();
        delete dispatcher;
    }
    if (msger) {
        msger->shutdown();
        msger->wait();
        delete msger;
        msger = nullptr;
    }
    return ret;
}

int32_t ProxyMsgr::GetOsdDf()
{
    std::vector<std::string> cmd = {
        "{\"prefix\": \"osd df\", \"target\": [\"mgr\", \"\"], \"format\": \"json\"}"
        };
    bufferlist outs;
    JSONParser parser;
    int32_t ret = dispatcher->SendMgrCommand(cmd, &parser);
    if (ret < 0) {
        ProxyDbgLogErr("get osd stat failed, ret=%d", ret);
        return ret;
    }
    JSONObj *summaryObj = parser.find_obj("summary");
    if (!summaryObj) {
        ProxyDbgLogErr("parse summary failed, internal error, json=%s", parser.get_json());
        return 0;
    }
    string totalKB;
    string totalRawUsedKB;
    string dataUsedKB;
    JSONDecoder::decode_json("total_kb", totalKB, summaryObj);
    JSONDecoder::decode_json("total_kb_used", totalRawUsedKB, summaryObj);
    JSONDecoder::decode_json("total_kb_used_data", dataUsedKB, summaryObj);

    ProxyDbgLogInfo("total_kb=%s, total_kb_used=%s, total_kb_used_data=%s",
        totalKB.c_str(), totalRawUsedKB.c_str(), dataUsedKB.c_str());
    std::unique_lock<std::shared_mutex> lock(clusterData.dataLock);
    clusterData.totalSizeKB = stoull(totalKB, nullptr, DECIMAL_NOTATION);
    clusterData.rawUsedSizeKB = stoull(totalRawUsedKB, nullptr, DECIMAL_NOTATION);
    clusterData.dataUsedSizeKB = stoull(dataUsedKB, nullptr, DECIMAL_NOTATION);
    lock.unlock();
    return ret;
}

int32_t ProxyMsgr::JsonParseCephDF(const std::string &pool, uint64_t &poolTotalUsed, uint64_t &poolTotalAvail)
{
    JSONParser poolJson;
    if (!poolJson.parse(pool.c_str(), pool.length())) {
        ProxyDbgLogErr("parse pool failed, internal error, pool=%s", pool.c_str());
        return -EINVAL;
    }
    string poolIdStr;
    uint64_t poolId;
    struct PoolInfo info;
    JSONDecoder::decode_json("id", poolIdStr, &poolJson);
    poolId = stoull(poolIdStr, nullptr, DECIMAL_NOTATION);
    ProxyDbgLogInfo("poolId = %llu", poolId);
    int ret = GetPoolBaseInfo(poolId, &info);
    if (ret == -ENOENT) {
        info.k = 1;
        info.m = DEFAULT_M;
    }
    JSONObj *poolStatsObj = poolJson.find_obj("stats");
    if (!poolStatsObj) {
        ProxyDbgLogErr("parse pool failed, internal error, pool=%s", pool.c_str());
        return -EINVAL;
    }
    string used;
    string avail;
    JSONDecoder::decode_json("bytes_used", used, poolStatsObj);
    JSONDecoder::decode_json("max_avail", avail, poolStatsObj);
    poolTotalUsed += stoull(used, nullptr, DECIMAL_NOTATION);
    uint64_t tmpPoolTotalAvail = stoull(avail, nullptr, DECIMAL_NOTATION);
    tmpPoolTotalAvail = tmpPoolTotalAvail * (info.m + info.k) / info.k;
    poolTotalAvail = poolTotalAvail >= tmpPoolTotalAvail ? poolTotalAvail : tmpPoolTotalAvail;
    return 0;
}

int32_t ProxyMsgr::CephDf()
{
    std::vector<std::string> cmd = {
        "{\"prefix\": \"df\", \"format\": \"json\"}"
        };
    bufferlist outs;
    JSONParser parser;
    int32_t ret = dispatcher->SendMonCommand(cmd, &parser);
    if (ret < 0) {
        ProxyDbgLogErr("get osd stat failed, ret=%d", ret);
        return ret;
    }
    JSONObj *poolsObj = parser.find_obj("pools");
    if (!poolsObj) {
        ProxyDbgLogErr("parse pools failed, internal error, json=%s", parser.get_json());
        return 0;
    }

    uint64_t poolTotalUsed = 0;
    uint64_t poolTotalAvail = 0;
    if (poolsObj->is_array()) {
        std::vector<std::string> poolList = poolsObj->get_array_elements();
        for (auto pool : poolList) {
            ret = JsonParseCephDF(pool, poolTotalUsed, poolTotalAvail);
            if (ret < 0) {
                continue;
            }
        }
    } else {
        ProxyDbgLogErr("parse pool list failed, internal error, poolLIst=%s", poolsObj->get_data().c_str());
        return -EINVAL;
    }
    ProxyDbgLogInfo("all pool totalUse=%llu, maxAvail=%llu", poolTotalUsed, poolTotalAvail);
    std::unique_lock<std::shared_mutex> lock(clusterData.dataLock);
    clusterData.allPoolUsed = poolTotalUsed;
    clusterData.allPoolAvail = poolTotalAvail;
    lock.unlock();
    return 0;
}

void ProxyMsgr::PollThread()
{
    pollThreadFlag = true;
    while (pollThreadFlag) {
        int ret = CephDf();
        if (ret < 0) {
            threadCV.wait_for(DEFAULT_ABNM_SLEEP_TIME);
            continue;
        }
        threadCV.wait_for(DEFAULT_NM_SLEEP_TIME);
    }
}

int32_t ProxyMsgr::InitPollThread()
{
    pollThread = std::thread(&ProxyMsgr::PollThread, this);
    return 0;
}

int32_t ProxyMsgr::Start()
{
    // cct init
    int ret = InitCCMAdaptor();
    if (ret) {
        ProxyDbgLogErr("init ccm adaptor failed %d", ret);
        return -1;
    }

    ret = InitCephContext();
    if (ret) {
        ProxyDbgLogErr("init ceph context failed %d", ret);
        return -1;
    }

    ret = InitMessenger();
    if (ret) {
        ProxyDbgLogErr("init proxy msgr failed %d", ret);
        return -1;
    }

    ret = InitDispatcher();
    if (ret) {
        ProxyDbgLogErr("init dispatcher failed %d", ret);
        return -1;
    }

    ret = InitPollThread();
    if (ret) {
        ProxyDbgLogErr("init poll thread %d", ret);
        return -1;
    }

    return ret;
}

#ifdef WITH_TESTS_ONLY
void ProxyMsgr::debug_osdmap()
{
    // pool info
    for (map<int64_t, pg_pool_t>::const_iterator it = osdmap->get_pools().begin();
        it != osdmap->get_pools().end(); it++) {
        const pg_pool_t &pool = it->second;
        std::cout << "poolId=" << it->first << " type=" << pool.get_type() << std::endl;
        if (pool.is_replicated()) {
            std::cout << "k=" << 1 << " m=" << (pool.get_size() - 1)
                << " rep=" << pool.get_size() << std::endl;
            std::cout << "default stripe_unit "
                << m_cct->_conf.get_val<Option::size_t>("osd_pool_erasure_code_stripe_unit") << std::endl;
        } else {
            map<string, string> profile = osdmap->get_erasure_code_profile(pool.erasure_code_profile);
            auto ecm = profile.find("m");
            auto eck = profile.find("k");
            if (ecm != profile.end() && eck != profile.end()) {
                std::cout << "profile " << pool.erasure_code_profile
                    << " k=" << stoi(eck->second) << " m=" << stoi(ecm->second)
                    << " rep=" << (stoi(eck->second) + stoi(ecm->second)) * 1.0 / stoi(eck->second) << std::endl;
            }
            auto ecStripe = profile.find("stripe_unit");
            if (ecStripe != profile.end()) {
                std::cout << "stripe_unit " << stoi(ecStripe->second) << std::endl;
            } else {    // default
                std::cout << "default stripe_unit "
                    << m_cct->_conf.get_val<Option::size_t>("osd_pool_erasure_code_stripe_unit") << std::endl;
            }
        }
    }
    // osd info
    for (int32_t i = 0; i < osdmap->get_max_osd(); i++) {
        if (osdmap->is_up(i)) {
            const entity_addr_t& enAddr = osdmap->get_addrs(i).legacy_or_front_addr();
            std::cout << i << " " << osdmap->get_weightf(i) << " "
                << enAddr.ip_only_to_str() << std::endl;
        }
    }
}
#endif

void ProxyMsgr::UpdateOSDMap(MOSDMap *m)
{
    for (epoch_t e = osdmap->get_epoch() + 1; e <= m->get_last(); e++) {
        if (e == osdmap->get_epoch() + 1 && m->incremental_maps.count(e)) {     // normal
            OSDMap::Incremental inc(m->incremental_maps[e]);
            osdmap->apply_incremental(inc);
        } else if (m->maps.count(e)) {          // missing some, replace
            OSDMap *new_osdmap = new OSDMap();
            new_osdmap->decode(m->maps[e]);
            delete osdmap;
            osdmap = new_osdmap;
        } else if (e >= m->get_oldest()) {      // missing maps, renew
            needSubContinuous = false;
            RenewOSDMap(osdmap->get_epoch() ? osdmap->get_epoch() + 1 : 0);
            return;
        } else {            // missing history, skip
            e = m->get_oldest() - 1;
            continue;
        }
    }
    if (!needSubContinuous) {
        needSubContinuous = true;
        RenewOSDMap(osdmap->get_epoch() ? osdmap->get_epoch() + 1 : 0);
    }
}

void ProxyMsgr::InitOsdMap(MOSDMap *m)
{
    if (m->maps.count(m->get_last())) {         // get latest osdmap
        osdmap->decode(m->maps[m->get_last()]);
        if (!needSubContinuous) {
            needSubContinuous = true;
            RenewOSDMap(osdmap->get_epoch() ? osdmap->get_epoch() + 1 : 0);
        }
    } else {    // renew
        needSubContinuous = false;
        RenewOSDMap(0);
    }
}

void ProxyMsgr::handle_osd_map(MOSDMap *m)
{
    if (m->fsid != dispatcher->monclient.get_fsid()) {
        ProxyDbgLogWarnLimit1("another fsid %d", m->fsid);
        return;
    }

    if (m->get_last() <= osdmap->get_epoch()) {
        ProxyDbgLogWarnLimit1("ignore epoch %u current %u", m->get_last(), osdmap->get_epoch());
        if (!needSubContinuous) {
            needSubContinuous = true;
            RenewOSDMap(osdmap->get_epoch() ? osdmap->get_epoch() + 1 : 0);
        }
        dispatcher->monclient.sub_got("osdmap", osdmap->get_epoch());
    } else {
        ProxyDbgLogInfo("handle epoch %u -> [%u, %u]", osdmap->get_epoch(), m->get_first(), m->get_last());
        std::unique_lock<std::shared_mutex> lock(osdmapMutex);
        if (osdmap->get_epoch() == 0) { // first time
            InitOsdMap(m);
        } else {        // update osdmap
            UpdateOSDMap(m);
        }
        lock.unlock();
        dispatcher->monclient.sub_got("osdmap", osdmap->get_epoch());
        PoolChangeNotify();
#ifdef WITH_TESTS_ONLY
        debug_osdmap();
#endif
    }
}

void ProxyMsgr::RenewOSDMap(epoch_t e)
{
    int flag = 0;
    bool needSubOnce = !(needSubContinuous || osdmap->test_flag(CEPH_OSDMAP_PAUSERD) ||
        osdmap->test_flag(CEPH_OSDMAP_PAUSEWR));
    if (needSubOnce) {
        flag = CEPH_SUBSCRIBE_ONETIME;
    }
    ProxyDbgLogInfo("osdmap sub %u, flag %d", e, flag);
    dispatcher->monclient.sub_want("osdmap", e, flag);
    dispatcher->monclient.renew_subs();
}

int ProxyMsgr::GetPoolAllUsedAndAvail(uint64_t &usedSize, uint64_t &maxAvail)
{
    std::shared_lock<std::shared_mutex> lock(clusterData.dataLock);
    usedSize = clusterData.allPoolUsed;
    maxAvail = clusterData.allPoolAvail;
    lock.unlock();
    return 0;
}

int ProxyMsgr::GetPoolBaseInfo(uint32_t poolId, struct PoolInfo *info)
{
    int64_t pool = static_cast<int64_t>(poolId);
    std::shared_lock<std::shared_mutex> lock(osdmapMutex);
    auto it = osdmap->get_pools().find(pool);
    if (it != osdmap->get_pools().end()) {
        const pg_pool_t &pool = it->second;
        if (pool.is_replicated()) {
            info->k = 1;
            info->m = pool.get_size() - 1;
            info->stripeUnit = m_cct->_conf.get_val<Option::size_t>("osd_pool_erasure_code_stripe_unit");
        } else {
            map<string, string> profile = osdmap->get_erasure_code_profile(pool.erasure_code_profile);
            auto ecm = profile.find("m");
            auto eck = profile.find("k");
            if (ecm != profile.end() && eck != profile.end()) {
                info->k = stoi(eck->second);
                info->m = stoi(ecm->second);
            }
            auto ecStripe = profile.find("stripe_unit");
            if (ecStripe != profile.end()) {
                std::cout << "stripe_unit " << stoi(ecStripe->second) << std::endl;
                info->stripeUnit = stoi(ecStripe->second);
            } else {    // default
                info->stripeUnit = m_cct->_conf.get_val<Option::size_t>("osd_pool_erasure_code_stripe_unit");
            }
        }
        ProxyDbgLogInfo("poolId(%u) base info: k=%d m=%d stripe_unit=%d", poolId, info->k, info->m, info->stripeUnit);
    } else {
        ProxyDbgLogErr("poolId(%u) not Exists", poolId);
        return -ENOENT;
    }
    return 0;
}


int32_t InitProxyMsgr(proxyMsgrHandler *handler)
{
    ProxyMsgr* msgr = new(std::nothrow) ProxyMsgr();
    if (msgr == nullptr) {
        ProxyDbgLogErr("msgr alloc failed");
        return -ENOMEM;
    }
    int ret = msgr->Start();
    if (ret) {
        ProxyDbgLogErr("start msgr failed, ret=%d", ret);
        return -1;
    }
    *handler = static_cast<proxyMsgrHandler>(msgr);
    return 0;
}

int32_t DestroyProxyMsgr(proxyMsgrHandler *handler)
{
    ProxyMsgr* msgr = static_cast<ProxyMsgr *>(*handler);
    if (msgr != nullptr) {
        delete msgr;
    }
    return 0;
}

int32_t MsgrGetOSDMap(proxyMsgrHandler handler,
    std::map<uint32_t, std::pair<int32_t, std::string>> &osdMap)
{
    ProxyMsgr* msgr = static_cast<ProxyMsgr *>(handler);
    if (msgr == nullptr) {
        return -EINVAL;
    }
    std::shared_lock<std::shared_mutex> lock(msgr->osdmapMutex);
    for (int32_t i = 0; i < msgr->osdmap->get_max_osd(); i++) {
        if (msgr->osdmap->is_up(i)) {
            const entity_addr_t& enAddr = msgr->osdmap->get_addrs(i).legacy_or_front_addr();
            int32_t w = msgr->osdmap->get_weightf(i);
            osdMap[i] = std::make_pair(w, enAddr.ip_only_to_str());
        }
    }
    return 0;
}

int32_t MsgrGetPGMap(proxyMsgrHandler handler, int64_t poolId,
    std::map<uint32_t, std::vector<int>> &pgMap)
{
    ProxyMsgr* msgr = static_cast<ProxyMsgr *>(handler);
    if (msgr == nullptr) {
        return -EINVAL;
    }
    std::shared_lock<std::shared_mutex> lock(msgr->osdmapMutex);
    auto iter = msgr->osdmap->get_pools().find(poolId);
    if (iter == msgr->osdmap->get_pools().end()) {
        ProxyDbgLogErr("poolId %lld not exists", poolId);
        return -ENOENT;
    }
    const pg_pool_t &pool = iter->second;
    for (uint32_t i = 0; i < pool.get_pg_num(); i++) {
        pg_t rawpg(i, poolId);
        pg_t pgid = msgr->osdmap->raw_pg_to_pg(rawpg);
        vector<int> acting;
        int acting_primary;
        msgr->osdmap->pg_to_up_acting_osds(pgid, nullptr, nullptr,
            &acting, &acting_primary);
        pgMap[i] = acting;
    }

    return 0;
}

int32_t MsgrCalBatchPGId(proxyMsgrHandler handler,
    int64_t poolId, std::vector<std::string> oid, std::vector<uint32_t> &pgs)
{
    ProxyMsgr* msgr = static_cast<ProxyMsgr *>(handler);
    if (msgr == nullptr) {
        return -EINVAL;
    }
    std::shared_lock<std::shared_mutex> lock(msgr->osdmapMutex);
    const pg_pool_t *p = msgr->osdmap->get_pg_pool(poolId);
    if (!p) {
        ProxyDbgLogErr("poolId %lld not exists", poolId);
        return -ENOENT;
    }
    pgs.resize(oid.size());
    int ret = 0;
    for (uint32_t i = 0; i < oid.size(); i++) {
        pgs[i] = p->raw_hash_to_pg(p->hash_key(oid[i], ""));
    }

    return ret;
}

int32_t GetPoolUsedAndAvail(proxyMsgrHandler handler, uint64_t &usedSize, uint64_t &maxAvail)
{
    ProxyMsgr* msgr = static_cast<ProxyMsgr *>(handler);
    if (msgr == nullptr) {
        return -EINVAL;
    }
    return msgr->GetPoolAllUsedAndAvail(usedSize, maxAvail);
}

int32_t CCMRegisterPoolNotifyFn(proxyMsgrHandler handler, NotifyPoolEventFn fn)
{
    ProxyMsgr* msgr = static_cast<ProxyMsgr *>(handler);
    if (msgr == nullptr) {
        return -EINVAL;
    }
    return msgr->RegisterPoolNotifyFn(fn);
}

int32_t GetPoolBaseInfo(proxyMsgrHandler handler, uint32_t poolId, struct PoolInfo *info)
{
    ProxyMsgr* msgr = static_cast<ProxyMsgr *>(handler);
    if (msgr == nullptr || info == nullptr) {
        return -EINVAL;
    }
    return msgr->GetPoolBaseInfo(poolId, info);
}
