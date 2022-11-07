#include "CcmAdaptor.h"
#include "CephProxyLog.h"
#include <vector>

int ClusterManagerAdaptor::ReportCreatePool(std::vector<uint32_t> &pools)
{
    uint32_t* poolArr = NULL;
    uint32_t len = pools.size();
    if (len != 0) {
        poolArr = (uint32_t*)malloc(sizeof(uint32_t) * len);
        if (poolArr == NULL) {
            ProxyDbgLogErr("malloc poolArr failed.");
            return -1;
        }
    }

    for (uint32_t i = 0; i < len; i++) {
        poolArr[i] = pools[i];
        ProxyDbgLogDebug("notify pool[%u] create.", pools[i]);
    }
    if (notifyCreateFunc != NULL) {
        int32_t ret = notifyCreateFunc(poolArr,len);
        if (poolArr) {
            free(poolArr);
        }
        if (ret != 0) {
            ProxyDbgLogErr("notify poolcreate failed, ret=%d", ret);
            return -1;
        }
        return 0;
    }

    if (poolArr) {
        free(poolArr);
    }

    return 0;
}

int ClusterManagerAdaptor::RegisterPoolCreateReportFn(NotifyPoolEventFn fn)
{
    if (fn == NULL) {
        ProxyDbgLogErr("input argument is NULL");
        return -1;
    }

    if (notifyCreateFunc != NULL) {
        ProxyDbgLogErr("createFunc has already registered.");
        return -1;
    }

    notifyCreateFunc = fn;
    return 0;
}