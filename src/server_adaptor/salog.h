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

#ifndef SA_LOG_H
#define SA_LOG_H

#include <climits>
#include <memory>
#include <string>
#include <algorithm>
#include <iostream>

#include "sa_def.h"
#include "sa_export.h"

enum LogLevel {
    LV_CRITICAL = 0,
    LV_ERROR = 1,
    LV_WARNING = 2,
    LV_INFORMATION = 3,
    LV_DEBUG = 4
};

int InitSalog(SaExport &sa);

int FinishSalog(const std::string &name);

void OsaWriteLog(const int log_level, std::string file_name, int f_line, const std::string func_name,
		const char *format, ...);
void OsaWriteLogLimit(const int log_level, std::string file_name, int f_line, const std::string func_name,
		const char *format, ...);
void OsaWriteLogLimit2(const int log_level, std::string file_name, int f_line, const std::string func_name,
		const char *format, ...);
void OsaWriteDataLog(std::string file_name, int f_line, const std::string func_name,
		const char *format, ...);

#define Salog(level, subModule, format, ...)      \
   do {						  \
	  OsaWriteLog(level, __FILE__, __LINE__, __func__, format, ## __VA_ARGS__);\
      } while (0) 

#define SaDatalog(format, ...)      \
   do {						  \
	  OsaWriteDataLog(__FILE__, __LINE__, __func__, format, ## __VA_ARGS__);\
      } while (0) 

#define SalogLimit(level, subModule, format, ...)      \
   do {						  \
	  OsaWriteLogLimit(level, __FILE__, __LINE__, __func__, format, ## __VA_ARGS__);\
      } while (0) 

#define SalogLimit2(level, subModule, format, ...)      \
   do {						  \
	  OsaWriteLogLimit2(level, __FILE__, __LINE__, __func__, format, ## __VA_ARGS__);\
      } while (0) 

#endif








