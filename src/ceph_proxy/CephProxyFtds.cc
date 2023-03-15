/*
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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

/*
static void sa_ftds_diagnose(int32_t argc, char *argv[])
{
    uint32_t indx;
    uint64_t start;
    uint64_t backall;
    uint64_t backgood;
    uint64_t backbad;
    uint64_t notback;

    diagnose_buf("%-36s: %-10s%-10s%-10s%-10s\n",
                 "name", "start", "backgood", "backbad", "notback");
    for (indx = 0; indx < STREAM_FTDS_COUNT; indx++) {
        start = (uint64_t)dpax_atomic64_read(&g_stream_ftds.counter[indx].in);
        backall = (uint64_t)dpax_atomic64_read(&g_stream_ftds.counter[indx].out);
        backbad = (uint64_t)dpax_atomic64_read(&g_stream_ftds.counter[indx].fail);
        backgood = backall - backbad;
        notback = start - backall;
        diagnose_buf("%-36s: %-10lu%-10lu%-10lu%-10lu\n",
                     s_ftds_name[indx], start, backgood, backbad, notback);
    }
    UNREFERENCE_PARAM(argc);
    UNREFERENCE_PARAM(argv);
}

static diagnose_command s_ftds_debug = {
    "ftds",
    "stream ftds diagnose command.",
    stream_ftds_diagnose,
    NULL,
    FALSE,
    TRUE
};

static int32_t ftds_diag_register(void)
{
    if (!diagnose_register(&s_ftds_debug)) {
        return DP_ERROR;
    }
    return DP_OK;
}

static void ftds_diag_unregister(void)
{
    diagnose_unregister(&s_ftds_debug);
}

static extend_module_t s_stream_ftds_mod = {
    "ftds",
    ftds_diag_register,
    ftds_diag_unregister,
};

DATA_INIT stream_ftds_mod_init(void)
{
    uint32_t indx;

    for (indx = 0; indx < STREAM_FTDS_COUNT; indx++) {
        dpax_atomic64_set(&g_stream_ftds.counter[indx].in, 0L);
        dpax_atomic64_set(&g_stream_ftds.counter[indx].out, 0L);
        dpax_atomic64_set(&g_stream_ftds.counter[indx].fail, 0L);
    }
    extend_module_register(&s_stream_ftds_mod);
}
*/
