/* 
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include "CephProxyFtds.h"
#include <unistd.h>
#include <stdint.h>

typedef struct {
    dpax_atomic64_t in;
    dpax_atomic64_t out;
    dpax_atomic64_t fail;
} ftds_counter;

typedef struct {
    ftds_counter counter[PROXY_FTDS_COUNT];
} proxy_ftds_t;

#define FTDS_NAME(ID) (#ID)

static proxy_ftds_t g_proxy_ftds;

void proxy_ftds_in(uint32_t id)
{
    dpax_atomic64_inc(&g_proxy_ftds.counter[id].in);
}

void proxy_ftds_out(uint32_t id, int32_t ret)
{
    dpax_atomic64_inc(&g_proxy_ftds.counter[id].out);
    if (ret != 0) {
        dpax_atomic64_inc(&g_proxy_ftds.counter[id].fail);
    }
}

void proxy_ftds_counter(uint32_t id)
{
    dpax_atomic64_inc(&g_proxy_ftds.counter[id].in);
}