/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd. All rights reserved.
 *
 */

#include <string> 
#include <iostream>
#include <vector>
#include <netdb.h>
#include <ifaddrs.h>

#include <global/global_init.h>

#include "network_module.h"
#include "config_read.h"
#include "salog.h" 
#include "osa.h"

using namespace std;

NetworkModule *g_ptrNetwork = nullptr;
namespace {
const string LOG_TYPE = "SAO_INTERFACE";
const int ERROR_PORT = 101;
const int ERROR_BIND = 102;
const uint32_t SA_QUEUE_MAX_NUM = 5000;
const uint32_t SA_QUEUE_MIN_NUM = 4;
const uint32_t SA_QUEUE_MAX_CAPACITY = 1024;
const uint32_t SA_QUEUE_MIN_CAPACITY = 1;
const uint32_t SA_MSGR_MAX_NUM = 16;
const uint32_t SA_MSGR_MIN_NUM = 3;
}

ClassHandler *rpc_handler = nullptr;
void cls_initialize(ClassHandler *ch);

int rpc_init()
{
	ClassHandler::ClassData *cls = nullptr;
	int ret;
	ret = rpc_handler->open_class(string("rgw"), &cls);
	if (ret) {
	   Salog(LV_WARNING, LOG_TYPE, "open cls_rgw failed");
	   return ret;
        }
	cls = nullptr;
	ret = rpc_handler->open_class(string("lock"), &cls);
	if (ret) {
	   Salog(LV_WARNING, LOG_TYPE, "open cls_lock failed");
	   return ret;
	}
	return 0;
}

bool IsDigit(const char *c, uint32_t length)
{
   for (uint32_t i = 0; i < length; i++) {
	if (c[i] < '0' || c[i] > '9') {
	    return false;
	}
   }
   return true;
}

static bool CheckClassList(const string &cname)
{
    static vector<string> ClassList = { "rgw", "lock" };
    return find(ClassList.begin(), ClassList.end(), cname) != ClassList.end();
}

bool CheckLocalIp(const string &ipaddr)
{
    bool ret = false;
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    int apiRet = getifaddrs(&ifaddr);
    if (apiRet == -1) {
	    Salog(LV_ERROR, LOG_TYPE, "getifaddrs error %d", apiRet);
	    return ret;
    }
    for (ifa =ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
	    if (ifa->ifa_addr == NULL) {
		    continue;
	    }
	    family = ifa->ifa_addr->sa_family;
	    if (family == AF_INET || family == AF_INET6) {
		    s = getnameinfo(ifa->ifa_addr,
		        (family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL,
			0, NI_NUMERICHOST);
		    if (s != 0) {
			Salog(LV_ERROR, LOG_TYPE, "getnameinfo error %s", gai_strerror(s));
			return ret;
		    }
		    if (ipaddr == host) {
			    ret = true;
			    break;
	 	    }
	   }
    }
    freeifaddrs(ifaddr);
    return ret;
}

bool CheckPort(const char *pPort, uint64_t strLen)
{
	if (IsDigit(pPort, strLen) == false) {
		Salog(LV_ERROR, LOG_TYPE, "error : port is not digit");
		return false;
	}
	int portNum = atoi(pPort);
	if (portNum > 65535 || portNum < 1024) {
		Salog(LV_ERROR, LOG_TYPE, "error : port number is %d", portNum);
		return false;
	}
	return true;
}

bool CheckCoreId(int coreId)
{
	return (coreId >= 0 && coreId < sysconf(_SC_NPROCESSORS_ONLN)) ? true : false;
}

int OSA_Init(SaExport &sa)
{
    InitSalog(sa);
    char curDate[128] = {0};
    memset(curDate, 0, sizeof(curDate));
    string strDate = "220326-";
    strcpy(curDate, strDate.c_str());
#ifdef NDEBUG
    Salog(LV_WARNING, LOG_TYPE, "OSA_Init %sR", curDate);
#else
    Salog(LV_WARNING, LOG_TYPE, "OSA_Init %sD", curDate);
#endif

    OsaConfigRead readConfig;
    if (readConfig.CacheClusterConfigInit()) {
	    Salog(LV_CRITICAL, LOG_TYPE, "error : read config file.");
	    return ERROR_PORT;
    }
    string rAddr = readConfig.GetListenIp();
    if (CheckLocalIp(rAddr) == false) {
	    Salog(LV_CRITICAL, LOG_TYPE, "error : IP addr %s is illegal.", rAddr.c_str());
	    return ERROR_PORT;
    }

    string rPort = readConfig.GetListenPort();
    vector<string> vecPort;
    const char *delimPort = ",";
    std::unique_ptr<char[]> tmpPort = std::make_unique<char[]>(rPort.size() + 1);
    strcpy(tmpPort.get(), rPort.c_str());
    char *pPort;
    char *savePort;
    pPort = strtok_r(tmpPort.get(), delimPort, &savePort);
    while (pPort) {
	    if (CheckPort(pPort, strlen(pPort)) == false) {
		    Salog(LV_CRITICAL, LOG_TYPE, "error port %s", rPort.c_str());
		    return ERROR_PORT;
    	    }
	    vecPort.push_back(pPort);
	    pPort = strtok_r(nullptr, delimPort, &savePort);
    }
    if (vecPort.size() <= 0 || vecPort.size() > 8) {
	    Salog(LV_CRITICAL, LOG_TYPE, "error : port count is %d", vecPort.size());
	    return ERROR_PORT;
    }

    string coreNumber =  readConfig.GetCoreNumber();
    Salog(LV_INFORMATION, LOG_TYPE, "coreNumber is %s", coreNumber.c_str());
    vector<int> vecCoreId;
    const char *delim = ",";
    std::unique_ptr<char[]> tmp = std::make_unique<char[]>(coreNumber.size() + 1);
    strcpy(tmp.get(), coreNumber.c_str());
    char *p;
    char *savep;
    p = strtok_r(tmp.get(), delim, &savep);
    while (p) {
	    int coreId = atoi(p);
	    if (CheckCoreId(coreId) == false) {
		    Salog(LV_CRITICAL, LOG_TYPE, "error coreNumber %s", coreNumber.c_str());
		    return ERROR_PORT;
    	    }
	    vecCoreId.push_back(coreId);
	    p = strtok_r(nullptr, delim, &savep);
    }

    uint32_t queueAmount = readConfig.GetQueueAmount();
    if (queueAmount > SA_QUEUE_MAX_NUM || queueAmount < SA_QUEUE_MIN_NUM) {
	Salog(LV_CRITICAL, LOG_TYPE, "error : queueAmount number is %d should between %d~%d", queueAmount,
			SA_QUEUE_MIN_NUM, SA_QUEUE_MAX_NUM);
	return ERROR_PORT;
    }
   
    uint32_t queueMaxCapacity = readConfig.GetQueueMaxCapacity();
    if (queueMaxCapacity > SA_QUEUE_MAX_CAPACITY || queueMaxCapacity < SA_QUEUE_MIN_CAPACITY) {
	Salog(LV_CRITICAL, LOG_TYPE, "error : queueMaxCapacity number is %d should between %d~%d", queueMaxCapacity,
			SA_QUEUE_MIN_CAPACITY, SA_QUEUE_MAX_CAPACITY);
	return ERROR_PORT;
    }

    uint32_t msgrAmount = readConfig.GetMsgrAmount();
    if (msgrAmount > SA_MSGR_MAX_NUM || msgrAmount < SA_MSGR_MIN_NUM) {
	Salog(LV_CRITICAL, LOG_TYPE, "error : msgrAmount number is %d should between %d~%d", msgrAmount,
			SA_MSGR_MIN_NUM, SA_MSGR_MAX_NUM);
 	return ERROR_PORT;
    }

    uint32_t bindCore = readConfig.GetBindCore();
    uint32_t bindSaCore = readConfig.GetBindQueueCore();
    Salog(LV_WARNING, LOG_TYPE, "core binding is %d, %d", bindCore, bindSaCore);
    if((msgrAmount + vecPort.size()) > vecCoreId.size() && bindCore) {
        Salog(LV_CRITICAL, LOG_TYPE, "error : SA needs more than %d cores !", (msgrAmount+vecPort.size()));
        return ERROR_PORT;
    }

    QosParam qos;
    qos.limitWrite = readConfig.GetWriteQoS();
    qos.getQuotaCycle = readConfig.GetQuotCyc();
    qos.enableThrottle = readConfig.GetMessengerThrottle();
    qos.saOpThrottle = readConfig.GetSaOpThrottle();
    Salog(LV_WARNING, LOG_TYPE, "SA QoS limitWrite=%d, time_cyc=%d ms, enableThrottle=%d, opThrottle=%lu",
        qos.limitWrite, qos.getQuotaCycle, qos.enableThrottle, qos.saOpThrottle);

    if (qos.getQuotaCycle < 1) {
        Salog(LV_CRITICAL, LOG_TYPE, "error : get quota cycle is %d", qos.getQuotaCycle);
        return ERROR_PORT;
    }
    if ((qos.saOpThrottle != 0) && (qos.saOpThrottle < 2000 || qos.saOpThrottle > 30000)) {
        Salog(LV_CRITICAL, LOG_TYPE, "error : get quota cycle is %d, should in {0, [2000~30000]}", qos.saOpThrottle);
        return ERROR_PORT;
    }

    char szMsgrAmount[4] = {0};
    sprintf(szMsgrAmount, "%d", msgrAmount);
    Salog(LV_INFORMATION, LOG_TYPE, "Server adaptor init queueAmount=%d szMsgrAmount=%s bindCore=%d bindSaCore=%d",
		    queueAmount, szMsgrAmount, bindCore, bindSaCore);

    vector<const char *> args = { "--conf", "/opt/gcache/conf/gcache.conf" };
    map<string, string> defaults = { { "ms_async_op_threads", szMsgrAmount } };
    static auto cct = global_init(&defaults, args, 0xFF /* 0xFF CEPH_ENTITY_TYPE_ANY */,
	    CODE_ENVIRONMENT_LIBRARY /*CODE_ENVIRONMENT_LIBRARY CODE_ENVIRONMENT_DAEMON */,
	    CINIT_FLAG_NO_DEFAULT_CONFIG_FILE);
    
    int ret = 0;
    if (g_ptrNetwork == nullptr) {
        g_ptrNetwork = new NetworkModule(sa, vecCoreId, msgrAmount, bindCore, bindSaCore);
	g_ptrNetwork->CreateWorkThread(queueAmount, vecPort.size(),queueMaxCapacity);
	string sAddr = rAddr;
        string sPort = vecPort[0];
	string testMode = "0";
	Salog(LV_INFORMATION, LOG_TYPE, "Server adaptor init Port=%s" , rPort.c_str());
	if (rPort == "") {
	    Salog(LV_CRITICAL, LOG_TYPE, "error : Server adaptor Listen ip:port is empty.");
	    return 1;
	}
    g_ptrNetwork->SetQosParam(qos);
	int bindSuccess = -1;
	ret = g_ptrNetwork->InitNetworkModule(rAddr, vecPort, sAddr, sPort, &bindSuccess);

	rpc_handler = new ClassHandler(g_ceph_context);
	cls_initialize(rpc_handler);
	rpc_init();
	
	int sleepCnt = 0;
	while (bindSuccess == -1) {
	   SalogLimit(LV_WARNING,  LOG_TYPE, "bindSuccess == -1");
	   sleep(1);
	   if (sleepCnt++ >= 60) {
		ret = ERROR_BIND;
		break;
	    }
	}
	if (bindSuccess == 0) {
	    ret=ERROR_BIND;
	}
    }
    return ret;
}
      
int OSA_Finish()
{
    Salog(LV_WARNING, LOG_TYPE, "SAO_Finish");
    int ret = 0;
    if( g_ptrNetwork == nullptr) {
	return 1;
    }
    ret = g_ptrNetwork->FinishNetworkModule();
    g_ptrNetwork->StopThread();
    delete g_ptrNetwork;
    g_ptrNetwork = nullptr;
    if (rpc_handler) {
	rpc_handler->shutdown();
	rpc_handler = nullptr;
    }
    FinishSalog("sa");
    Salog(LV_WARNING,LOG_TYPE, "SAO_Finish ret=%d",ret);
    return ret;
}

int OSA_FinishCacheOps(void *p, unsigned long int t, unsigned long int l, int r)
{
    int ret= 0;
    FinishCacheOps(p, t, l, r);
    return ret;
}

void OSA_ProcessBuf(const char *buf, unsigned int len, int cnt, void *p)
{
    ProcessBuf(buf, len, cnt, p);
}

void OSA_EncodeOmapGetkeys(const SaBatchKeys *batchKeys, int i, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeOmapGetkeys(batchKeys, i , ptr);
}
void OSA_EncodeOmapGetvals(const SaBatchKv *KVs, int i,void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeOmapGetvals(KVs, i, ptr);
}
void OSA_EncodeOmapGetvalsbykeys(const SaBatchKv *keyValue, int i, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeOmapGetvalsbykeys(keyValue, i, ptr);
}

void OSA_EncodeRead(uint64_t opType, unsigned int offset, unsigned int len, char *buf, unsigned int bufLen, int i,
	       	void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeRead(opType, offset, len, buf, bufLen, i, ptr);
}

void OSA_SetOpResult(int i, int32_t ret, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    SetOpResult(i, ret, ptr);
}

void OSA_EncodeXattrGetxattr(const SaBatchKv *keyValue, int i, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeXattrGetXattr(keyValue, i, ptr);
}

void OSA_EncodeXattrGetxattrs(const SaBatchKv *keyValue, int i, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeXattrGetXattrs(keyValue, i, ptr);
}

void OSA_EncodeGetOpstat(uint64_t psize, time_t ptime, int i, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeGetOpstat(psize, ptime, i, ptr);
}

void OSA_EncodeListSnaps(const ObjSnaps *objSnaps, int i, void *p)
{
    MOSDOp *ptr = (MOSDOp *)(p);
    EncodeListSnaps(objSnaps, i, ptr);
}

int OSA_ExecClass(SaOpContext *pctx, PREFETCH_FUNC prefetch)
{
    if (pctx == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " osa ctx %p is null, skip", pctx);
        return -EINVAL;
    }
    struct SaOpReq * pOpReq = pctx->opReq;
    MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
    if (ptr == nullptr) {
        Salog(LV_ERROR, LOG_TYPE, " mosdop %p is null, skip", ptr);
        return -EINVAL;
    }
    OSDOp &clientop = ptr->ops[pctx->opId];
    string cname, mname;
    bufferlist indata;
    auto bp = clientop.indata.cbegin();
    try {
	bp.copy(clientop.op.cls.class_len, cname);
	bp.copy(clientop.op.cls.method_len, mname);
	bp.copy(clientop.op.cls.indata_len, indata);
    } catch ( buffer::error &e) {
	Salog(LV_ERROR, LOG_TYPE, "unable to decode class [%s] + method[%s] + indata[%d]", cname.c_str(), mname.c_str(), 
			clientop.op.cls.indata_len);
	return -EINVAL;
    }
    if (cname.compare("rpc") == 0 && mname.compare("das_prefetch") == 0) {
        uint64_t offset;
        uint64_t len;
        auto bp = indata.cbegin();
        decode(offset, bp);
        decode(len, bp);
        OpRequestOps &osdop = pOpReq->vecOps[pctx->opId];

        osdop.objOffset = offset;
        osdop.objLength = len;
        return prefetch(*pOpReq, osdop);
    } else if (cname.compare("rbd") == 0 && mname.compare("copyup") == 0) {
        if (cls_cxx_stat2(pctx, NULL, NULL) == 0) {
            Salog(LV_DEBUG, LOG_TYPE, "finish state, tid=%ld", pOpReq->tid);
            *pOpReq->copyupFlag = 0;
            return 0;
        }
        int ret = cls_cxx_write(pctx, 0, indata.length(), &indata);
        Salog(LV_DEBUG, LOG_TYPE, "finish write, tid=%ld", pOpReq->tid);
        *pOpReq->copyupFlag = 0;
        return ret;
    }

    if (!CheckClassList(cname)) {
        Salog(LV_ERROR, LOG_TYPE, "class [%s] not in whitelist ret [%d]", cname.c_str(), -EOPNOTSUPP);
        return -EOPNOTSUPP;
    }
    ClassHandler::ClassData *cls;
    int ret = rpc_handler->open_class(cname, &cls);
    if ( ret) {
        Salog(LV_ERROR,LOG_TYPE, "can't open class [%s] ret [%d]", cname.c_str(), ret);
        return -EOPNOTSUPP;
    }

    ClassHandler::ClassMethod *method = cls->get_method(mname.c_str());
    if (!method) {
	Salog(LV_ERROR,LOG_TYPE, "can't find class [%s] + method[%s]", cname.c_str(), mname.c_str());
	return -EOPNOTSUPP;
    }
  
    bufferlist outdata;
    int result = method->exec(pctx, indata, outdata);
    if ( result == 0) {
	ptr->ops[pctx->opId].op.extent.length = outdata.length();
	ptr->ops[pctx->opId].outdata.claim_append(outdata);
    }
    return result;
}

