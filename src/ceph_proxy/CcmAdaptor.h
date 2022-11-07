#ifndef CLUSTER_MGR_ADAPTOR_H
#define CLUSTER_MGR_ADAPTOR_H

#include <vector>
#include "CephProxyInterface.h"

class ClusterManagerAdaptor {
private:
    NotifyPoolEventFn notifyCreateFunc;
public:
    ClusterManagerAdaptor():notifyCreateFunc(NULL) { }
    ~ClusterManagerAdaptor() {}

    int ReportCreatePool(std::vector<uint32_t> &pools);

    int RegisterPoolCreateReportFn(NotifyPoolEventFn fn);
};

#endif