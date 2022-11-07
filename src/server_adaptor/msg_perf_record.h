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

#ifndef MSG_PERF_RECORD_H
#define MSG_PERF_RECORD_H

#include <stdint.h>
#include <sys/time.h>
#include <thread>
#include <iomanip>
#include <sched.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <atomic>
#include <functional>

using namespace std;

class MsgPerfRecord {
    std::atomic<unsigned long long> msgRecvTinc;
    std::atomic<unsigned long long> msgRecvCount;
    unsigned long long msgRecvCountPre;

    std::atomic<unsigned long long> msgSendTinc;
    std::atomic<unsigned long long> msgSendCount;
    unsigned long long msgSendCountPre;

    std::atomic<unsigned long long> msgTotalTinc;
    std::atomic<unsigned long long> msgTotalCount;
    unsigned long long msgTotalCountPre;

    std::atomic<bool> finish;
    std::ofstream outfile;
    vector<std::thread> threads;

public:
    MsgPerfRecord()
    {
        msgRecvTinc = 0;
	msgRecvCount = 0;
	msgRecvCountPre = 0;

	msgSendTinc = 0;
	msgSendCount = 0;
	msgSendCountPre = 0;
	
	msgTotalTinc = 0;
	msgTotalCount = 0;
	msgTotalCountPre = 0;

	finish = false;
    }
    ~MsgPerfRecord() {}

    void set_recv(long t);
    void set_send(long t);
    void set_Total(long t);

    std::function<void()> write_perf_log();

    void start();
    void stop();
};


#endif