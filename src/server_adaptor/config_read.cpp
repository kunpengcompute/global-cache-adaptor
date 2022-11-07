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

#include "config_read.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <salog.h>
#include <iostream>

#define CPU_NUM_64 (64)
#define CPU_NUM_96 (96)
#define CPU_NUM_128 (128)
#define MAX_CPU_NUM (256)

#ifndef RETURN_OK
#define RETURN_OK 0
#endif

#ifndef RETURN_ERROR
#define RETURN_ERROR (-1)
#endif

typedef struct {
	char listenIp[MAX_IPV4_ADDR_LEN];
	char listenPort[MAX_PORT_SIZE];
	char coreNumber[MAX_PORT_SIZE];
	uint64_t queueAmount { 0 };
	uint64_t msgrAmount { 0 };
	uint64_t bindCore { 0 };
	uint64_t bindQueueCore { 0 };
	uint64_t queueMaxCapacity { 0 };

	uint64_t writeOoS{0};
	uint64_t getQuotaCyc{0};
	uint64_t getMessengerThrottle {0};
	uint64_t saOpThrottle{ 0 };
} SA_ClusterControlCfg;

SA_ClusterControlCfg g_SaClusterControlCfg = { 0 };

//
static int32_t GetSysCpuNum()
{
	int32_t cpuNum = get_nprocs_conf();
	Salog(LV_INFORMATION, LOG_TYPE, "GetCpuNum %d", cpuNum);
	return cpuNum;
}

extern "C" {
	int GetCfgItemCstr(char *dest, size_t destSize, const char *unit, const char *key);
	int GetCfgItemInt32(int32_t *dest, const char *unit, const char *key);
	int GetCfgItemUint32(uint32_t *dest, const char *unit, const char *key);
	int GetCfgItemUint64(uint64_t *dest, const char *unit, const char *key);
}
static int32_t SetCfgCorenum()
{
	int cpuNum = GetSysCpuNum();
	int keyCpuNum;
	int ret = 0;

	if (cpuNum < CPU_NUM_96) {
		ret = GetCfgItemCstr(
			g_SaClusterControlCfg.coreNumber, sizeof(g_SaClusterControlCfg.coreNumber), "sa", "core_number_64");
		keyCpuNum = CPU_NUM_64;
	} else if (cpuNum < CPU_NUM_128) {
		ret = GetCfgItemCstr(
			g_SaClusterControlCfg.coreNumber, sizeof(g_SaClusterControlCfg.coreNumber), "sa", "core_number_96");
		keyCpuNum = CPU_NUM_96;
	} else if (cpuNum < MAX_CPU_NUM) {
		ret = GetCfgItemCstr(
			g_SaClusterControlCfg.coreNumber, sizeof(g_SaClusterControlCfg.coreNumber), "sa", "core_number_128");
		keyCpuNum = CPU_NUM_128;
	} else {
		ret = GetCfgItemCstr(
			g_SaClusterControlCfg.coreNumber, sizeof(g_SaClusterControlCfg.coreNumber), "sa", "core_number_256");
		keyCpuNum = MAX_CPU_NUM;
	}

	if (ret != RETURN_OK) {
		fprintf(stderr, "config get config para [unit:sa, key:core_number_%d] failed.\n", keyCpuNum);
		return RETURN_ERROR;
	}
	return RETURN_OK;
}



static int32_t ReadConfig()
{
#define GET_CFG_ITEM_U64(dest, unit, key)				\
	do {												\
		int res = GetCfgItemUint64(dest, unit, key);	\
		if (res != RETURN_OK) {							\
			fprintf(stderr, "config get config para [unit:%s, key:%s] failed.\n", unit, key);	\
			return RETURN_ERROR;						\
		}												\
	} while (0)
	int32_t ret = RETURN_OK;
	ret = GetCfgItemCstr(g_SaClusterControlCfg.listenIp, MAX_IPV4_ADDR_LEN, "communicate", "public_ipv4_addr");
	if (ret != RETURN_OK) {
		std::cerr << "config get config para [unit:communicate, key:public_ipv4_addr] failed.\n";
		return RETURN_ERROR;
	}
	ret = GetCfgItemCstr(g_SaClusterControlCfg.listenPort, MAX_IPV4_ADDR_LEN, "communicate", "local_port");
	if (ret != RETURN_OK) {
		std::cerr << "config get config para [unit:communicate, key:public_ipv4_addr] failed.\n";
		return RETURN_ERROR;
	}
	ret = SetCfgCorenum();
	if (ret != RETURN_OK) {
		return ret;
	}
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.queueAmount, "sa", "queue_amount");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.queueMaxCapacity, "sa", "queue_max_capacity");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.msgrAmount, "sa", "msgr_amount");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.bindCore, "sa", "bind_core");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.bindQueueCore, "sa", "bind_queue_core");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.writeOoS, "sa", "write_qos");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.getQuotaCyc, "sa", "get_quota_cyc");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.getMessengerThrottle, "sa", "enable_messenger_throttle");
	GET_CFG_ITEM_U64(&g_SaClusterControlCfg.saOpThrottle, "sa", "sa_op_throttle");
#undef GET_CFG_ITEM_U64
	return ret;
}

int32_t OsaConfigRead::CacheClusterConfigInit()
{
	int ret;
	memset(&g_SaClusterControlCfg, 0, sizeof(SA_ClusterControlCfg));
	ret = ReadConfig();
	if (ret != RETURN_OK) {
		fprintf(stderr, "read cluster config failed, ret %d.\n",ret);
		return ret;
	}
	return RETURN_OK;
}

char *OsaConfigRead::GetListenIp()
{
	return g_SaClusterControlCfg.listenIp;
}
char *OsaConfigRead::GetListenPort()
{
	return g_SaClusterControlCfg.listenPort;
}
char *OsaConfigRead::GetCoreNumber()
{
	return g_SaClusterControlCfg.coreNumber;
}
uint32_t OsaConfigRead::GetQueueAmount()
{
	return g_SaClusterControlCfg.queueAmount;
}

uint32_t OsaConfigRead::GetMsgrAmount()
{
	return g_SaClusterControlCfg.msgrAmount;
}
uint32_t  OsaConfigRead::GetBindCore()
{
	return g_SaClusterControlCfg.bindCore;
}

uint32_t OsaConfigRead::GetBindQueueCore()
{
	return g_SaClusterControlCfg.bindQueueCore;
}

uint32_t OsaConfigRead::GetQueueMaxCapacity()
{
	return g_SaClusterControlCfg.queueMaxCapacity;
}
uint32_t OsaConfigRead::GetWriteQoS()
{
	return g_SaClusterControlCfg.writeOoS;
}
uint32_t OsaConfigRead::GetQuotCyc()
{
	return g_SaClusterControlCfg.getQuotaCyc;
}
uint32_t OsaConfigRead::GetMessengerThrottle()
{
	return g_SaClusterControlCfg.getMessengerThrottle;
}
uint64_t OsaConfigRead::GetSaOpThrottle()
{
	return g_SaClusterControlCfg.saOpThrottle;
}