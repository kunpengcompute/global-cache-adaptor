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

#ifndef SA_EXPORT_H
#define SA_EXPORT_H

#include "sa_def.h"

class OphandlerModule;
class SaExport {
    std::string confPath;

public:
    // libosa DO NOT CALL Init.
    void Init(OphandlerModule &p);

    void DoOneOps(SaOpReq &saOp);

    void WriteLog(const int logLevel, const std::string &fileName, const int fLine, const std::string &funcName,
        const std::string &format);
    void WriteLogLimit(const int logLevel, const std::string &fileName, const int fLine, const std::string &funcName,
        const std::string &format);
    void WriteLogLimit2(const int logLevel, const std::string &fileName, const int fLine, const std::string &funcName,
        const std::string &format);
    void WriteDataLog(const std::string &fileName, const int fLine, const std::string &funcName,
        const std::string &format);

    void SetConfPath(const std::string &path);
    std::string GetConfPath();

    void FtdsStartNormal(unsigned int id, const char *idName, uint64_t &ts);
    void FtdsEndNormal(unsigned int id, const char *idName, uint64_t &ts, int ret);
    void FtdsStartHigh(unsigned int id, const char *idName, uint64_t &ts);
    void FtdsEndHigt(unsigned int id, const char *idName, uint64_t &ts, int ret);

    void GetWriteQuota(unsigned int poolId, SaWcacheQosInfo &info);
    uint64_t GetWriteOpThrottle();
    uint64_t GetReadOpThrottle();
    uint64_t GetWriteBWThrottle();
    uint64_t GetReadBWThrottle();
    uint64_t GetWriteIopsThrottle();
};

#endif
