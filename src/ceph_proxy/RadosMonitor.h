#ifndef PROXY_RADOS_MONITOR_H
#define PROXY_RADOS_MONITOR_H

#include <stdint.h>
#include <vector>
#include <regex>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include "CcmAdaptor.h"
#include "CephProxy.h"

#define KIB_NUM (1024ULL)
#define MIB_NUM (KIB_NUM * 1024ULL)
#define GIB_NUM (MIB_NUM * 1024ULL)
#define TIB_NUM (GIB_NUM * 1024ULL)
#define PIB_NUM (TIB_NUM * 1024ULL)

#define DEFAULT_UPDATE_TIME_INTERVAL 3

typedef struct {
    uint64_t storedSize;
    uint64_t objectsNum;
    uint64_t usedSize;
    float    useRatio;
    uint64_t maxAvail;
    uint64_t version;
    uint32_t k;
    uint32_t m;
    uint32_t stripeUnit;
    bool isEC;
} PoolUsageInfo;

struct RecordInfo {
    uint32_t poolId;
    double storedSize;
    uint64_t storedSizeUnit;
    double objectsNum;
    uint64_t numUnit;
    double usedSize;
    uint64_t usedSizeUnit;
    double maxAvail;
    uint64_t maxAvailUnit;
    double useRatio;
    std::string poolName;
    std::string profile;
    bool isEC;
    uint32_t ec_k;
    uint32_t ec_m;
    uint32_t stripeUnit;
    double rep;
};

class CephProxy;

class PoolUsageStat {
private:
    std::shared_mutex mutex;
    CephProxy *proxy;
    std::thread timer;
    ClusterManagerAdaptor *ccm_adaptor;
    bool stop;
    uint32_t timeInterval;
    std::map<uint32_t, PoolUsageInfo> poolInfoMap;
    std::map<uint32_t, PoolUsageInfo> tmpPoolInfoMap;
    std::vector<uint32_t> poolList;
    uint32_t globalStripeSize = 0;

    int32_t ReportPoolNewAndDel(std::vector<uint32_t> &newPools);
public:
    PoolUsageStat(CephProxy *_proxy): proxy(_proxy), ccm_adaptor(NULL), stop(false), 
                                      timeInterval(DEFAULT_UPDATE_TIME_INTERVAL) { }
    ~PoolUsageStat() { }

    int32_t GetPoolUsageInfo(uint32_t poolId, PoolUsageInfo *poolInfo);
    int32_t GetPoolAllUsedAndAvail(uint64_t &usedSize, uint64_t &maxAvail);
    int32_t Record(std::smatch &result);
    void Compare();
    int32_t GetPoolReplicationSize(uint32_t poolId, double &rep);
    int32_t GetPoolInfo(uint32_t poolId, struct PoolInfo *info);
    int32_t IsECPool(string poolName, string &ecProfile, bool &isEC);
    void ParseECProfile(string &infoVector, uint32_t &k, uint32_t &m, uint32_t &stripeUnit);
    int32_t ParseToRecordInfo(std::smatch &result, struct RecordInfo &info);
    int32_t CalPoolSize(struct RecordInfo &info);
    int32_t GetECProfileSize(std::string profileName, uint32_t &k, uint32_t &m, uint32_t &stripeUnit);
    uint32_t GetDefaultECStripeUnit();
    int32_t UpdatePoolUsage(void);
    int32_t UpdatePoolList(void);
    int32_t RegisterPoolNewNotifyFn(NotifyPoolEventFn fn);

    uint32_t GetTimeInterval();
    bool isStop();
    void Start(void);
    void Stop();
};

#endif