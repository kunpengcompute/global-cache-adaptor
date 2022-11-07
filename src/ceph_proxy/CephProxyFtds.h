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

#ifndef __CEPH_PROXY_FTDS_H__
#define __CEPH_PROXY_FTDS_H__

#include "dpax_atomic.h"
#include "dpax_time.h"
#include "ftds.h"
#include <stdint.h>

typedef enum tagPROXY_FTDS_E {
    PROXY_FTDS_OPS_QUEUE = 100000,
    PROXY_FTDS_OPS_WRITEOP_INIT,
    PROXY_FTDS_OPS_READOP_INIT,
    PROXY_FTDS_OPS_OPINIT_WRITE,
    PROXY_FTDS_OPS_OPINIT_READ,
    PROXY_FTDS_OPS_OPINIT_READSTAT,
    PROXY_FTDS_OPS_OPINIT_APPEND,
    PROXY_FTDS_OPS_OPINIT_WRITEBL,
    PROXY_FTDS_OPS_OPINIT_READBL,
    PROXY_FTDS_OPS_OPINIT_APPENDBL,
    PROXY_FTDS_OPS_OPINIT_REMOVE,
    PROXY_FTDS_OPS_OPINIT_TRUNCATE,
    PROXY_FTDS_OPS_OPINIT_ZERO,
    PROXY_FTDS_OPS_GETCLUSTER_STAT,
    PROXY_FTDS_OPS_GETPOOL_STAT,
    PROXY_FTDS_OPS_READ,
    PROXY_FTDS_OPS_WRITE,
    PROXY_FTDS_OPS_WAITQ,
    PROXY_FTDS_OPS_OPERATE,
    PROXY_FTDS_OPS_WRCB,
    PROXY_FTDS_OPS_RDCB,
    PROXY_FTDS_OPS_GETIOCTX,
    PROXY_FTDS_COUNT
} PROXY_FTDS_E;

#define FTDS_BASE       (400)
#define FTDS_ID(id)     (FTDS_BASE + id)
#define FTDS_H16B(levl) (((levl) << 24) | (6 << 16))
#define FTDS_RET(ret)   (((ret) != 0) ? -1 : 0)

void proxy_ftds_in(uint32_t id);
void proxy_ftds_out(uint32_t id, int32_t ret);
void proxy_ftds_counter(uint32_t id);

#define PROXY_FTDS_ST(id, ts, lev)                                                          \
    do {                                                                                    \
        ts = dpax_time_getnanosec();                                                        \
        FTDS_DELAY_START(#id, NULL, (FTDS_H16B(lev) | FTDS_ID(id)), STAT_MODE_DELAY, NULL); \
    } while(0)

#define PROXY_FTDS_ED(id, ts, ret, lev)                                                                           \
    do {                                                                                                          \
        uint64_t delay = dpax_time_getnanosec() - ts;                                                             \
        FTDS_DELAY_END(#id, NULL, (FTDS_H16B(lev) | FTDS_ID(id)), STAT_MODE_DELAY, (void*)&delay, FTDS_RET(ret)); \
        ts = delay;                                                                                               \
    } while(0)

#define PROXY_FTDS_START_NORMAL(id, ts)     PROXY_FTDS_ST(id, ts, TRACE_LEVEL_MODULE_NORMAL)
#define PROXY_FTDS_END_NORMAL(id, ts, ret)     PROXY_FTDS_ED(id, ts, ret, TRACE_LEVEL_MODULE_NORMAL)

#define PROXY_FTDS_START_HIGH(id, ts)     PROXY_FTDS_ST(id, ts, TRACE_LEVEL_MODULE_HIGH)
#define PROXY_FTDS_END_HIGH(id, ts, ret)     PROXY_FTDS_ED(id, ts, ret, TRACE_LEVEL_MODULE_HIGH)

#define  PROXY_FTDS_COUNTER(ID) \
    do {                        \
        proxy_ftds_counter(ID); \
    } while(0)

static inline void proxy_ftds_init(void)
{
    initFtdsClient();
}

#endif