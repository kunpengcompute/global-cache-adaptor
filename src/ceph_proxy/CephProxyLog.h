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

#ifndef __CEPH_PROXY_LOG_H__
#define __CEPH_PROXY_LOG_H__

#ifndef MY_PID
#define MY_PID 882
#endif 

#ifdef DPAX_LOG

#include "dplog.h"
#include "dpax_dbg_redef.h"

#define ProxyDbgCrit(format, ...)             \
        do {                                \
            GCI_LOGGER_CRITICAL(MY_PID, "[PROXY]" format, ## __VA_ARGS__);   \
        } while (0)
#ifdef WITH_TESTS_ONLY
#define ProxyDbgLogErr(format, ...)           \
        do {                                \
            printf(format "\n", ## __VA_ARGS__); \
        } while (0)
#else
#define ProxyDbgLogErr(format, ...)           \
        do {                                \
            GCI_LOGGER_ERROR(MY_PID, "[PROXY]" format, ## __VA_ARGS__); \
        } while (0)
#endif
#define ProxyDbgLogWarn(format, ...)          \
        do {                                \
            GCI_LOGGER_WARN(MY_PID, "[PROXY]" format, ## __VA_ARGS__); \
        } while (0)

#ifdef WITH_TESTS_ONLY
#define ProxyDbgLogInfo(format, ...)           \
        do {                                \
            printf(format "\n", ## __VA_ARGS__); \
        } while (0)
#else
#define ProxyDbgLogInfo(format, ...)           \
        do {                                 \
            GCI_LOGGER_INFO(MY_PID, "[PROXY]" format, ## __VA_ARGS__); \
        } while (0)
#endif
#define ProxyDbgLogDebug(format, ...)         \
        do {                                \
            GCI_LOGGER_DEBUG(MY_PID, "[PROXY]" format, ## __VA_ARGS__); \
        } while (0)

#define ProxyDbgCritLimit(peroid, frequency, format, ...)             \
        do {                                \
            bool bLogErrorPrint = false;                                                         \
            PRINT_LIMIT_PERIOD(DBG_LOG_CRI, MY_PID, period, frequency, bLogErrorPrint); \
            if (bLogErrorPrint) {                                                                 \
                GCI_LOGGER_CRITICAL(MY_PID, "[PROXY]" format, ## __VA_ARGS__);                       \
            } \
        } while (0)

#define ProxyDbgLogErrLimit(peroid, frequency, format, ...)           \
        do {                                \
            bool bLogErrorPrint = false;                                                         \
            PRINT_LIMIT_PERIOD(DBG_LOG_ERROR, MY_PID, period, frequency, bLogErrorPrint); \
            if (bLogErrorPrint) {                                                                 \
                GCI_LOGGER_ERR(MY_PID, "[PROXY]" format, ## __VA_ARGS__);                       \
            }   \
        } while (0)

#define ProxyDbgLogWarnLimit(peroid, frequency, format, ...)          \
        do {                                \
            bool bLogErrorPrint = false;                                                         \
            PRINT_LIMIT_PERIOD(DBG_LOG_WARNING, MY_PID, period, frequency, bLogErrorPrint); \
            if (bLogErrorPrint) {                                                                 \
                GCI_LOGGER_WARN(MY_PID, "[PROXY]" format, ## __VA_ARGS__);                       \
            }   \
        } while (0)

#define ProxyDbgLogInfoLimit(peroid, frequency, format, ...)           \
        do {                                \
            bool bLogErrorPrint = false;                                                         \
            PRINT_LIMIT_PERIOD(DBG_LOG_INFO, MY_PID, period, frequency, bLogErrorPrint); \
            if (bLogErrorPrint) {                                                                 \
                GCI_LOGGER_INFO(MY_PID, "[PROXY]" format, ## __VA_ARGS__);                       \
            }   \
        } while (0)

#define ProxyDbgLogDebugLimit(peroid, frequency, format, ...)                                   \
        do {                                                                                    \
            bool bLogErrorPrint = false;                                                        \
            PRINT_LIMIT_PERIOD(DBG_LOG_DEBUG, MY_PID, period, frequency, bLogErrorPrint);         \
            if (bLogErrorPrint) {                                                               \
                GCI_LOGGER_DEBUG(MY_PID, "[PROXY]" format, ## __VA_ARGS__);                       \
            }                                                                                   \
        } while (0)

#define ProxyDbgLogWarnLimit1(format, ...)          \
        do {                                \
            bool bLogErrorPrint = false;                                                         \
            PRINT_LIMIT_PERIOD(DBG_LOG_WARNING, MY_PID, 60 * HZ, 10, bLogErrorPrint); \
            if (bLogErrorPrint) {                                                                 \
                GCI_LOGGER_WARN(MY_PID, "[PROXY]" format, ## __VA_ARGS__);                       \
            }   \
        } while (0)
#else

#define ProxyDbgCrit(fmt, ...)  \
        fprintf(stderr, "[%s][%d][%s][CRI][PROXY][" fmt "]\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define ProxyDbgLogErr(fmt, ...)    \
        fprintf(stderr, "[%s][%d][%s][Err][PROXY][" fmt "]\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define ProxyDbgLogWarn(fmt, ...)   \
        fprintf(stderr, "[%s][%d][%s][Warn][PROXY][" fmt "]\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define ProxyDbgLogInfo(fmt, ...)   \
        fprintf(stderr, "[%s][%d][%s][Info][PROXY][" fmt "]\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define ProxyDbgLogDebug(fmt, ...) \
        fprintf(stderr, "[%s][%d][%s][Debug][PROXY][" fmt "]\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__)

#endif

#endif