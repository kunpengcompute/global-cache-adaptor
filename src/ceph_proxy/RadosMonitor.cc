#include <vector>
#include <regex>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <cstring>
#include <string>
#include <algorithm>
#include <unistd.h>

#include "CephProxyLog.h"
#include "RadosMonitor.h"

using namespace std;
using namespace librados;

std::string poolNamePattern = "\\s*(\\S*)\\s*";
std::string poolIdPattern = "(\\d+)\\s+";
std::string storedPattern = "([0-9]+\\.?[0-9]*)\\s(TiB|GiB|MiB|KiB|B)*\\s*";
std::string objectsPattern = "([0-9]+\\.?[0-9]*)(k|M)*\\s*";
std::string usedPattern = "([0-9]*\\.?[0-9]*)\\s(TiB|GiB|MiB|KiB|B)*\\s*";
std::string usedRatioPattern = "([0-9]*\\.?[0-9]*)\\s*";
std::string availPattern = "([0-9]*\\.?[0-9]*)\\s(TiB|GiB|MiB|KiB|B).*";
uint32_t defaultStripeUnit = 4096;
int defaultECKInReplica = 1;
int defaultECMInReplica = 2;

uint64_t TransStrUnitToNum(const char *strUnit)
{
    if (strcmp("PiB", strUnit) == 0) {
        return PIB_NUM;
    } else if (strcmp("TiB", strUnit) == 0) {
        return TIB_NUM;
    } else if (strcmp("GiB", strUnit) == 0 || strcmp("G", strUnit) == 0) {
        return GIB_NUM;
    } else if (strcmp("MiB", strUnit) == 0 || strcmp("M", strUnit) == 0) {
        return MIB_NUM;
    } else if (strcmp("KiB", strUnit) == 0 || strcmp("K", strUnit) == 0) {
        return KIB_NUM;
    } else if (strcmp("B", strUnit) == 0) {
        return 1;
    }

    return 1;
}

void TransStrToNum(std::string strNum, uint64_t &num)
{
    std::regex pattern("(\\d+)\\s?(\\w+)?");
    std::smatch result;
    bool flag = std::regex_match(strNum, result, pattern);
    if (flag) {
        errno = 0;
        char *end = nullptr;
        const char *p = result[1].str().c_str();
        uint32_t size = (uint32_t)strtol(p, &end, 10);
        if (errno == ERANGE || end == p) {
            ProxyDbgLogErr("get size failed.");
            return;
        }
        uint64_t unit = TransStrUnitToNum(result[2].str().c_str());
        num = size * unit;
    } else {
        ProxyDbgLogErr("trans str to num failed, str=%s", strNum.c_str());
    }
}

int32_t PoolUsageStat::GetPoolUsageInfo(uint32_t poolId, PoolUsageInfo *poolInfo)
{
    if (poolInfo == NULL) {
        ProxyDbgLogErr("poolInfo is NULL");
        return -22;
    }

    mutex.lock_shared();
    std::map<uint32_t, PoolUsageInfo>::iterator iter = poolInfoMap.find(poolId);
    if (iter == poolInfoMap.end()) {
        mutex.unlock_shared();
        return -1;
    }

    poolInfo->storedSize = iter->second.storedSize;
    poolInfo->objectsNum = iter->second.objectsNum;
    poolInfo->usedSize = iter->second.usedSize;
    poolInfo->useRatio = iter->second.useRatio;
    poolInfo->maxAvail = iter->second.maxAvail;

    mutex.unlock_shared();
    return 0;
}


int32_t PoolUsageStat::GetPoolAllUsedAndAvail(uint64_t &usedSize, uint64_t &maxAvail)
{
    usedSize = 0;
    maxAvail = 0;
    mutex.lock_shared();
    std::map<uint32_t, PoolUsageInfo>::iterator iter = poolInfoMap.begin();
    if (iter != poolInfoMap.end()) {
        maxAvail = iter->second.maxAvail; // 取第一个
    }

    for (; iter != poolInfoMap.end(); iter++) {
        usedSize += iter->second.usedSize; // 叠加
        if (maxAvail == 0 && iter->second.maxAvail != 0) {
            maxAvail = iter->second.maxAvail;
        }
    }
    mutex.unlock_shared();

    return 0;
}

int32_t PoolUsageStat::GetPoolInfo(uint32_t poolId, struct PoolInfo *info)
{
    mutex.lock_shared();
    std::map<uint32_t, PoolUsageInfo>::iterator iter = poolInfoMap.find(poolId);
    if (iter != poolInfoMap.end()) {
        info->k = iter->second.k;
        info->m = iter->second.m;
        info->stripeUnit = iter->second.stripeUnit;
    } else {
        ProxyDbgLogErr("poolId not Exists poolId=%u", poolId);
        mutex.unlock_shared();
        return -ENOENT;
    }
    mutex.unlock_shared();
    return 0;
}

int32_t PoolUsageStat::IsECPool(string poolName, string &ecProfile, bool &isEC)
{
    librados::Rados *rados = static_cast<librados::Rados *>(proxy->radosClient);
    std::string cmd("{\"var\": \"erasure_code_profile\", \"prefix\": \"osd pool get\", \"pool\": \"");
    std::string outs;
    bufferlist inbl;
    bufferlist outbl;
    cmd.append(poolName);
    cmd.append(string("\"}"));

    int ret = rados->mon_command(cmd, inbl, &outbl, &outs);
    if (ret < 0) {
        if (ret == -EACCES) {
            isEC = false;
            return 0;
        }
        ProxyDbgLogErr("Get erasure code profile failed, name=%s, ret=%d", poolName.c_str(), ret);
        return ret;
    }

    isEC = true;
    std::regex expression("erasure_code_profile:\\s(\\S+)\\n?");
    std::smatch result;
    std::string outStr(outbl.to_str());
    bool flag = std::regex_match(outStr, result, expression);
    if (flag && result.size() > 1) {
        ecProfile = result[1].str().c_str();
    } else {
        ProxyDbgLogErr("Get erasure code profile failed, name=%s, out=%s", poolName.c_str(), outbl.c_str());
        return -1;
    }

    return 0;
}

int32_t PoolUsageStat::GetECProfileSize(std::string profileName, uint32_t &k, uint32_t &m, uint32_t &stripeUnit)
{
    librados::Rados *rados = static_cast<librados::Rados *>(proxy->radosClient);
    std::string cmd("{\"prefix\": \"osd erasure-code-profile get\", \"name\": \"");
    std::string outs;
    bufferlist inbl;
    bufferlist outbl;
    char *strs = nullptr;
    cmd.append(profileName);
    cmd.append(string("\"}"));

    int ret = rados->mon_command(cmd, inbl, &outbl, &outs);
    if (ret != 0) {
        ProxyDbgLogErr("Get erasure code profile failed, profile=%s, ret=%d", profileName.c_str(), ret);
        return ret;
    }

    if (globalStripeSize == 0) {
        stripeUnit = GetDefaultECStripeUnit();
        globalStripeSize = stripeUnit;
    } else {
        stripeUnit = globalStripeSize;
    }

    std::vector<string> infoVector;
    strs = new(std::nothrow) char[outbl.to_str().size() + 1];
    if (strs == nullptr) {
        ProxyDbgLogErr("malloc failed");
        return -ENOMEM;
    }
    strcpy(strs, outbl.c_str());

    char *savep;
    char *p = strtok_r(strs, "\n", &savep);
    while (p) {
        string s = p;
        infoVector.push_back(s);
        p = strtok_r(nullptr, "\n", &savep);
    }

    for (uint32_t i = 0; i < infoVector.size(); i++) {
        ParseECProfile(infoVector[i], k, m, stripeUnit);
    }
    if (strs != nullptr) {
        delete[] strs;
        strs = nullptr;
    }
    return 0;
}

void PoolUsageStat::ParseECProfile(string &profile, uint32_t &k, uint32_t &m, uint32_t &stripeUnit)
{
    std::regex expression("(\\w+)=(\\w+)");
    std::smatch result;
    bool flag = std::regex_match(profile, result, expression);
    if (!flag) {
        return;
    }
    errno = 0;
    char *end = nullptr;
    if (result[1].str().compare("k") == 0) {
        const char *p = result[2].str().c_str();
        k = (uint32_t)strtol(p, &end, 10);
        if (errno == ERANGE || end == p) {
            return;
        }
    } else if (result[1].str().compare("m") == 0) {
        const char *p = result[2].str().c_str();
        m = (uint32_t)strtol(p, &end, 10);
        if (errno == ERANGE || end == p) {
            return;
        }
    } else if (result[1].str().compare("stripe_unit") == 0) {
        uint64_t val = 0;
        TransStrToNum(result[2].str().c_str(), val);
        if (val != 0) {
            stripeUnit = static_cast<uint32_t>(val);
        }
        ProxyDbgLogDebug("EC stripe unit %u", stripeUnit);
    }
}

uint32_t PoolUsageStat::GetDefaultECStripeUnit()
{
    uint32_t stripeUnit = defaultStripeUnit;
    uint64_t num = 0;

    librados::Rados *rados = static_cast<librados::Rados *>(proxy->radosClient);
    std::string cmd("{\"prefix\": \"config get\", \"who\": "    \
                    "\"osd.-1\", \"key\": \"osd_pool_erasure_code_stripe_unit\"}");
    std::string outs;
    bufferlist inbl;
    bufferlist outbl;
    int ret = rados->mon_command(cmd, inbl, &outbl, &outs);
    if (ret < 0) {
        ProxyDbgLogErr("Get default failed, ret=%d", ret);
        return ret;
    }

    string unit = outbl.to_str();
    unit.replace(unit.find("\n"), 1, "");           // replace '\n'

    TransStrToNum(unit, num);
    if (num != 0) {
        stripeUnit = static_cast<uint32_t>(num);
    }
    ProxyDbgLogDebug("default stripe unit %u", stripeUnit);
    return stripeUnit;
}

int32_t PoolUsageStat::GetPoolReplicationSize(uint32_t poolId, double &rep)
{
    rados_ioctx_t ioctx = proxy->GetIoCtx2(poolId);
    if (ioctx == NULL) {
        ProxyDbgLogErr("get ioctx failed, poolid=%u", poolId);
        return -1;
    }

    CephPoolStat stat;
    int32_t ret = proxy->GetPoolStat(ioctx, &stat);
    if (ret != 0) {
        ProxyDbgLogErr("Get pool state failed, poolid=%u", poolId);
        return -1;
    }

    if (stat.numObjects == 0 || stat.numObjectCopies == 0) {
        rep = 3;
    } else {
        rep = (stat.numObjectCopies * 1.0) / stat.numObjects;
    }

    return 0;
}

static int StrToDouble(const char *src, double &dest, const char *print)
{
    errno = 0;
    char *end = nullptr;
    dest = strtod(src, &end);
    if (errno == ERANGE || end == src) {
        ProxyDbgLogErr("get %s from str failed.", print);
        return -1;
    }
    return 0;
}

static int StrToInt(const char *src, int &dest, const char *print)
{
    errno = 0;
    char *end = nullptr;
    dest = (int)strtol(src, &end, 10);
    if (errno == ERANGE || end == src) {
        ProxyDbgLogErr("get %s from str failed.", print);
        return -1;
    }
    return 0;
}

int32_t PoolUsageStat::ParseToRecordInfo(std::smatch &result, struct RecordInfo &info)
{
    // 忽略整体匹配的那一个
    for (size_t i = 1; i < result.size(); i++) {
        if (i == 1) {   // poolName
            info.poolName = result[i].str().c_str();
        }
        if (i == 2) { // poolID
            int id;
            if (StrToInt(result[i].str().c_str(), id, "poolId") != 0) {
                return -1;
            }
            if (id < 0) {
                return -1;
            }
            info.poolId = (uint32_t)id;
        } else if (i == 3) { // storedSize
            if(StrToDouble(result[i].str().c_str(), info.storedSize, "storedSize") != 0) {
                return -1;
            }
        } else if (i == 4) { // storedSize 单位
            info.storedSizeUnit = TransStrUnitToNum(result[i].str().c_str());
        } else if (i == 5) { // 对象数量
            if(StrToDouble(result[i].str().c_str(), info.objectsNum, "objectsNum") != 0) {
                return -1;
            }
        } else if (i == 6) { // 单位/量级
            if (result[i].length() == 0) {
                info.numUnit = 1;
            } else if (strcmp(result[i].str().c_str(), "k") == 0) {
                info.numUnit = 1024;
            } else if (strcmp(result[i].str().c_str(), "m") == 0) {
                info.numUnit = 1024 * 1024;
            }
        } else if (i == 7) { // usedSize
            if(StrToDouble(result[i].str().c_str(), info.usedSize, "usedSize") != 0) {
                return -1;
            }
        } else if (i == 8) { // usedSize单位
            info.usedSizeUnit = TransStrUnitToNum(result[i].str().c_str());
        } else if (i == 9) { // useRatio
            if(StrToDouble(result[i].str().c_str(), info.useRatio, "useRatio") != 0) {
                return -1;
            }
        } else if (i == 10) { // maxAvail
            if(StrToDouble(result[i].str().c_str(), info.maxAvail, "maxAvail") != 0) {
                return -1;
            }
        } else if (i == 11) { // maxAvail单位
            info.maxAvailUnit = TransStrUnitToNum(result[i].str().c_str());
        }
    }
    int32_t ret = IsECPool(info.poolName, info.profile, info.isEC);
    if (ret != 0) {
        ProxyDbgLogErr("judge EC pool failed, poolId=%u", info.poolId);
        return -1;
    }
    return 0;
}

int32_t PoolUsageStat::CalPoolSize(struct RecordInfo &info)
{
    int ret = 0;
    if (info.isEC) {
        ret = GetECProfileSize(info.profile, info.ec_k, info.ec_m, info.stripeUnit);
        if (ret != 0) {
            ProxyDbgLogErr("get EC size failed, poolId=%u", info.poolId);
            return -1;
        }
        info.rep = (info.ec_k + info.ec_m) * 1.0 / info.ec_k;
    } else {
        info.ec_k = defaultECKInReplica;
        info.ec_m = defaultECMInReplica;
        if (globalStripeSize == 0) {
            info.stripeUnit = GetDefaultECStripeUnit();
            globalStripeSize = info.stripeUnit;
        } else {
            info.stripeUnit = globalStripeSize;
        }
        ret = GetPoolReplicationSize(info.poolId, info.rep);
        if (ret != 0) {
            ProxyDbgLogErr("get replicaiton size failed, poolId=%u", info.poolId);
            return -1;
        }
    }
    return 0;
}

int32_t PoolUsageStat::Record(std::smatch &result)
{
    struct RecordInfo info;
    memset(&info, 0, sizeof(info));
    info.storedSizeUnit = 1;
    info.numUnit = 1;
    info.usedSizeUnit = 1;
    info.maxAvailUnit = 1;
    info.isEC = false;

    int32_t ret = ParseToRecordInfo(result, info);
    if (ret < 0) {
        return -1;
    }

    ret = CalPoolSize(info);
    if (ret < 0) {
        return ret;
    }

    mutex.lock();
    tmpPoolInfoMap[info.poolId].storedSize = (uint64_t)(info.storedSize * info.storedSizeUnit);
    tmpPoolInfoMap[info.poolId].objectsNum = (uint64_t)(info.objectsNum * info.numUnit);
    tmpPoolInfoMap[info.poolId].usedSize = (uint64_t)(info.usedSize * info.usedSizeUnit);
    tmpPoolInfoMap[info.poolId].useRatio = info.useRatio;
    tmpPoolInfoMap[info.poolId].maxAvail = (uint64_t)(info.maxAvail * info.maxAvailUnit * info.rep);
    tmpPoolInfoMap[info.poolId].k = info.ec_k;
    tmpPoolInfoMap[info.poolId].m = info.ec_m;
    tmpPoolInfoMap[info.poolId].stripeUnit = info.stripeUnit;
    tmpPoolInfoMap[info.poolId].isEC = info.isEC;
    poolList.push_back(info.poolId);
    mutex.unlock();
    ProxyDbgLogDebug("detect pool, poolId=%u, poolName=%s, isEC=%d, k=%u, m=%u, stripeUnit=%u",
        info.poolId, info.poolName.c_str(), info.isEC, info.ec_k, info.ec_m, info.stripeUnit);
    return 0;
}

void PoolUsageStat::Compare()
{
    std::vector<uint32_t> deletedPool;
    mutex.lock();
    // 更新poolinfo表
    for (uint32_t i = 0; i < poolList.size(); i++) {
        uint32_t poolId = poolList[i];
        poolInfoMap[poolId].storedSize = tmpPoolInfoMap[poolId].storedSize;
        poolInfoMap[poolId].objectsNum = tmpPoolInfoMap[poolId].objectsNum;
        poolInfoMap[poolId].usedSize = tmpPoolInfoMap[poolId].usedSize;
        poolInfoMap[poolId].useRatio = tmpPoolInfoMap[poolId].useRatio;
        poolInfoMap[poolId].maxAvail = tmpPoolInfoMap[poolId].maxAvail;
        poolInfoMap[poolId].k = tmpPoolInfoMap[poolId].k;
        poolInfoMap[poolId].m = tmpPoolInfoMap[poolId].m;
        poolInfoMap[poolId].isEC = tmpPoolInfoMap[poolId].isEC;
        poolInfoMap[poolId].stripeUnit = tmpPoolInfoMap[poolId].stripeUnit;
        ProxyDbgLogDebug("update pool info, poolId=%u", poolId);
    }

    // 从pool info中删除已经不存在的pool
    std::map<uint32_t, PoolUsageInfo>::iterator iter = poolInfoMap.begin();
    for (; iter != poolInfoMap.end(); iter++) {
        if (find(poolList.begin(), poolList.end(), iter->first) == poolList.end()) {
            // 加入到删除队列中
            deletedPool.push_back(iter->first);
        }
    }

    for (uint32_t i = 0; i < deletedPool.size(); i++) {
        uint32_t poolId = deletedPool[i];
        poolInfoMap.erase(poolId);
        ProxyDbgLogDebug("old pool delete, poolId=%u", poolId);
    }
    mutex.unlock();
}

static int GetPoolStorageUsage(PoolUsageStat *mgr, const char *input)
{
    std::string pattern = poolNamePattern + poolIdPattern + storedPattern + objectsPattern + usedPattern +
        usedRatioPattern + availPattern;
    char *strs = nullptr;

    std::vector<string> infoVector;
    strs = new(std::nothrow) char[strlen(input) + 1];
    if (strs == nullptr) {
        ProxyDbgLogErr("malloc failed");
        return -ENOMEM;
    }
    strcpy(strs, input);

    char *savep;
    char *p = strtok_r(strs, "\n", &savep);
    while (p) {
        string s = p;
        infoVector.push_back(s);
        p = strtok_r(nullptr, "\n", &savep);
    }

    for (size_t i = 0; i < infoVector.size(); i++) {
        std::regex expression(pattern);
        std::smatch result;
        bool flag = std::regex_match(infoVector[i], result, expression);
        if (flag) {
            int32_t ret = mgr->Record(result);
            if (ret != 0) {
                ProxyDbgLogErr("record pool useage info failed.");
                if (strs != nullptr) {
                    delete[] strs;
                    strs = nullptr;
                }
                return -1;
            }
        }
    }
    mgr->Compare();

    if (strs != nullptr) {
        delete[] strs;
        strs = nullptr;
    }

    return 0;
}

int32_t PoolUsageStat::ReportPoolNewAndDel(std::vector<uint32_t> &newPools)
{
    // 向CCM上报新创建的Pool
    int32_t ret = ccm_adaptor->ReportCreatePool(newPools);
    if (ret != 0) {
        ProxyDbgLogErr("report create pool failed.");
        return -1;
    }

    return 0;
}

int32_t PoolUsageStat::UpdatePoolList(void)
{
    std::vector<uint32_t> tmpList;
    mutex.lock();
    poolList.swap(tmpList);
    tmpPoolInfoMap.clear();
    mutex.unlock();

    int32_t ret = ReportPoolNewAndDel(tmpList);
    if (ret != 0) {
        ProxyDbgLogErr("report pool new or del failed.");
        return -1;
    }

    return 0;
}

int PoolUsageStat::UpdatePoolUsage(void)
{
    librados::Rados *rados = static_cast<librados::Rados *>(proxy->radosClient);
    std::string cmd("{\"prefix\":\"df\"}");
    std::string outs;
    bufferlist inbl;
    bufferlist outbl;

    int ret = rados->mon_command(cmd, inbl, &outbl, &outs);
    if (ret < 0) {
        ProxyDbgLogErr("get cluster stat failed: %d", ret);
        return ret;
    }

    // 更新PoolUsage表
    ret = GetPoolStorageUsage(this, outbl.c_str());
    if (ret != 0) {
        ProxyDbgLogErr("get pool storage usage failed: %d", ret);
        return -1;
    }

    // 更新PoolList
    ret = UpdatePoolList();
    if (ret != 0) {
        ProxyDbgLogErr("get pool storage usage failed: %d", ret);
        return -1;
    }

    return 0;
}

int PoolUsageStat::RegisterPoolNewNotifyFn(NotifyPoolEventFn fn)
{
    if (ccm_adaptor == NULL) {
        ProxyDbgLogErr("poolUsageStat is not inited.");
        return -1;
    }

    int ret = ccm_adaptor->RegisterPoolCreateReportFn(fn);
    if (ret != 0) {
        ProxyDbgLogErr("register poolCreateReport Fn failed");
        return -1;
    }

    return 0;
}

bool PoolUsageStat::isStop()
{
    return stop;
}

uint32_t PoolUsageStat::GetTimeInterval()
{
    return timeInterval;
}

static void PoolUsageTimer(PoolUsageStat *poolUsageManager)
{
    while (true) {
        if (poolUsageManager->isStop()) {
            break;
        }

        int32_t ret = poolUsageManager->UpdatePoolUsage();
        if (ret != 0) {
            ProxyDbgLogErr("update pool usage failed.");
        }

        sleep(poolUsageManager->GetTimeInterval());
    }
}

void PoolUsageStat::Start()
{
    ccm_adaptor = new (std::nothrow) ClusterManagerAdaptor();
    if (ccm_adaptor == nullptr) {
        ProxyDbgLogErr("Allocate ClusterManagerAdaptor faild.");
        return;
    }

    int32_t ret = UpdatePoolUsage();
    if (ret != 0) {
        ProxyDbgLogErr("get pool usage failed: %d", ret);
    }

    timer = std::thread(PoolUsageTimer, this);
}

void PoolUsageStat::Stop()
{
    stop = true;
    timer.join();
}
