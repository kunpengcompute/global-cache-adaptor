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

#ifndef _CONFIG_READ_H_
#define _CONFIG_READ_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_IPV4_ADDR_LEN 16
#define MAX_IOD_CORE 8
#define MAX_PORT_SIZE 10
#define MAX_XNET_CORE 4
#define MAX_DPSHM_CORE 8
#define ZK_SERVER_LIST_STR_LEN 128
#define MAX_PATH_LEN 128
#define MAX_CORE_NUMBER 128


int32_t ProxyConfigInit();

char *ProxyGetZkServerList();
char *ProxyGetCoreNumber();
char *ProxyGetCephConf();
char *ProxyGetCephKeyring();
uint32_t ProxyGetWorkerNum();
uint32_t ProxyGetMsgrAmount();
uint32_t ProxyGetBindCore();

const char* ProxyGetOsdTimeOutOption();
uint64_t ProxyGetOsdTimeOut();
const char *ProxyGetMonTimeOutOption();
uint64_t ProxyGetMonTimeOut();

#endif

