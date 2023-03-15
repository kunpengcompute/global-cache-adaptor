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

#include "salog.h"

#include <string>
#include <ctime>
#include <fstream>
#include <ctime>
#include <sys/time.h>
#include <cstdarg>

using namespace std;

std::ofstream outfile;

namespace {
constexpr uint32_t ONE_LOG_MAX_LEN = 2048;
SaExport *sa = nullptr;
}

int InitSalog(SaExport &p)
{
    if (sa == nullptr) {
        sa = &p;
    }
    return 0;
}

void OsaWriteLog(const int log_level, string file_name, int f_line, const string func_name,
    const char *szFmt, ...)
{
    if (sa != nullptr) {
        va_list ap;
        va_start(ap, szFmt);
        char log[ONE_LOG_MAX_LEN + 1] = {0};
        int sRet = vsnprintf(log, ONE_LOG_MAX_LEN, szFmt, ap);
        if (sRet < 0) {
            va_end(ap);
            return;
        }
        va_end(ap);

        sa->WriteLog(log_level, file_name, f_line, func_name, log);
    }
}

void OsaWriteDataLog(string file_name, int f_line, const string func_name,
    const char *szFmt, ...)
{
    if (sa != nullptr) {
        va_list ap;
        va_start(ap, szFmt);
        char log[ONE_LOG_MAX_LEN + 1] = {0};
        int sRet = vsnprintf(log, ONE_LOG_MAX_LEN, szFmt, ap);
        if (sRet < 0) {
            va_end(ap);
            return;
        }
        va_end(ap);

        sa->WriteDataLog(file_name, f_line, func_name, log);
    }
}

void OsaWriteLogLimit(const int log_level, std::string file_name, int f_line, const std::string func_name,
    const char *szFmt, ...)
{
    if (sa != nullptr) {
        va_list ap;
        va_start(ap, szFmt);
        char log[ONE_LOG_MAX_LEN + 1] = {0};
        int sRet = vsnprintf(log, ONE_LOG_MAX_LEN, szFmt, ap);
        if (sRet < 0) {
            va_end(ap);
            return;
        }
        va_end(ap);

        sa->WriteLogLimit(log_level, file_name, f_line, func_name, log);
    }
}

void OsaWriteLogLimit2(const int log_level, std::string file_name, int f_line, const std::string func_name,
    const char *szFmt, ...)
{
    if (sa != nullptr) {
        va_list ap;
        va_start(ap, szFmt);
        char log[ONE_LOG_MAX_LEN + 1] = {0};
        int sRet = vsnprintf(log, ONE_LOG_MAX_LEN, szFmt, ap);
        if (sRet < 0) {
            va_end(ap);
            return;
        }
        va_end(ap);

        sa->WriteLogLimit2(log_level, file_name, f_line, func_name, log);
    }
}

int FinishSalog(const std::string &name)
{
    return 0;
}
