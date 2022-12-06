/*
* Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ConfigRead.h"
#include "CephProxyLog.h"

#define CLUSTER_CONFIG_FILE 	"/opt/gcache/conf/proxy.conf"
#define DEFAULT_CEPH_CONF_PATH 	"/etc/ceph/ceph.conf"
#define DEFAULT_ZK_SERVER_LIST	"localhost:2181"
#define DEFAULT_CORE_ARRAY	    "26,27,28,29"

#define DEFAULT_WORKER_NUM	1
#define DEFAULT_MSGR_NUM	3
#define DEFAULT_BIND_CORE	0

#define CONFIG_BUFFSIZE 500

#define MAX_CPU_NUM (256)

static const char *RADOS_MON_OP_TIMEOUT	= "rados_mon_op_timeout";
static const char *RADOS_OSD_OP_TIMEOUT	= "rados_osd_op_timeout";

#ifndef RETURN_OK
#define RETURN_OK 0
#endif

#ifndef RETURN_ERROR
#define RETURN_ERROR (-1)
#endif

typedef struct {
    char zkServerList[ZK_SERVER_LIST_STR_LEN + 1];
    char cephConfPath[MAX_PATH_LEN + 1];
    char cephKeyringPath[MAX_PATH_LEN + 1];
    char coreIds[MAX_CORE_NUMBER + 1];

    uint64_t workerNum;
    uint64_t msgrAmount;
    uint64_t bindCore;
    uint64_t osdTimeout;
    uint64_t monTimeout;
} ProxyControlCfg;

ProxyControlCfg g_ProxyCfg = { 0 };

static void InitClusterCfg()
{
    sprintf(g_ProxyCfg.zkServerList, "%s", DEFAULT_ZK_SERVER_LIST);
    sprintf(g_ProxyCfg.cephConfPath, "%s", DEFAULT_CEPH_CONF_PATH);
    sprintf(g_ProxyCfg.coreIds, "%s", DEFAULT_CORE_ARRAY);

    g_ProxyCfg.bindCore = DEFAULT_BIND_CORE;
    g_ProxyCfg.workerNum = DEFAULT_WORKER_NUM;
    g_ProxyCfg.msgrAmount = DEFAULT_MSGR_NUM;

    g_ProxyCfg.osdTimeout = 0;
    g_ProxyCfg.monTimeout = 0;
}


extern "C" {
int GetCfgItemCstr(char *dest, size_t destSize, const char *unit, const char *key);
int GetCfgItemInt32(int32_t *dest, const char *unit, const char *key);
int GetCfgItemUint32(uint32_t *dest, const char *unit, const char *key);
int GetCfgItemUint64(uint64_t *dest, const char *unit, const char *key);
}
static int32_t ReadConfig()
{
    int32_t ret = RETURN_OK;
    ret = GetCfgItemCstr(g_ProxyCfg.cephConfPath, sizeof(g_ProxyCfg.cephConfPath), "proxy", "ceph_conf_path");
    if (ret != RETURN_OK) {
        ProxyDbgLogErr("Get Config Item [unit:proxy,key:ceph_conf_path] failed");
        return RETURN_ERROR;
    }
    ret = GetCfgItemCstr(g_ProxyCfg.cephKeyringPath, sizeof(g_ProxyCfg.cephKeyringPath), "proxy", "ceph_keyring_path");
    if (ret != RETURN_OK) {
        ProxyDbgLogErr("Get Config Item [unit:proxy,key:ceph_keyring_path] failed");
        return RETURN_ERROR;
    }
    ret = GetCfgItemCstr(g_ProxyCfg.coreIds, sizeof(g_ProxyCfg.coreIds), "proxy", "core_number");
    if (ret != RETURN_OK) {
        ProxyDbgLogErr("Get Config Item [unit:proxy,key:core_number] failed");
        return RETURN_ERROR;
    }
    ret = GetCfgItemUint64(&g_ProxyCfg.bindCore, "proxy", "bind_core");
    if (ret != RETURN_OK) {
        ProxyDbgLogErr("Get Config Item [unit:proxy,key:bind_core] failed");
        return RETURN_ERROR;
    }
    ret = GetCfgItemUint64(&g_ProxyCfg.monTimeout, "proxy", "rados_mon_op_timeout");
    if (ret != RETURN_OK) {
        ProxyDbgLogErr("Get Config Item [unit:proxy,key:rados_mon_op_timeout] failed");
        return RETURN_ERROR;
    }
    ret = GetCfgItemUint64(&g_ProxyCfg.osdTimeout, "proxy", "rados_osd_op_timeout");
    if (ret != RETURN_OK) {
        ProxyDbgLogErr("Get Config Item [unit:proxy,key:rados_osd_op_timeout] failed");
        return RETURN_ERROR;
    }
    return ret;
}

int32_t ProxyConfigInit()
{
	int ret;
	memset(&g_ProxyCfg, 0, sizeof(ProxyControlCfg));
	InitClusterCfg();


	ret = ReadConfig();
	if (ret != RETURN_OK) {
		ProxyDbgLogErr("read cluster config failed, ret %d.\n", ret);
		return ret;
	}

	return RETURN_OK;
}


char *ProxyGetZkServerList()
{
	return g_ProxyCfg.zkServerList;
}

char *ProxyGetCoreNumber()
{
	return g_ProxyCfg.coreIds;
}

char *ProxyGetCephConf() {
	return g_ProxyCfg.cephConfPath;
}

char *ProxyGetCephKeyring()
{
	return g_ProxyCfg.cephKeyringPath;
}

uint32_t ProxyGetWorkerNum()
{
	return g_ProxyCfg.workerNum;
}

uint32_t ProxyGetMsgrAmount()
{
	return g_ProxyCfg.msgrAmount;
}

uint32_t ProxyGetBindCore()
{
	return g_ProxyCfg.bindCore;
}

uint64_t ProxyGetOsdTimeOut()
{
	return g_ProxyCfg.osdTimeout;
}

uint64_t ProxyGetMonTimeOut()
{
	return g_ProxyCfg.monTimeout;
}

const char *ProxyGetOsdTimeOutOption()
{
	return RADOS_OSD_OP_TIMEOUT;
}

const char *ProxyGetMonTimeOutOption()
{
	return RADOS_MON_OP_TIMEOUT;
}
