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

#include "msg_perf_record.h"
#include <string>
#include <unistd.h>
#include <ctime>

#include <sys/syscall.h>
#define gettid() syscall(__NR_gettid)

using namespace std;

namespace {
const unsigned char SHORT_BUFF_LEN = 128;
}

void MsgPerfRecord::set_recv(long t)
{
     msgRecvCount++;
     msgRecvTinc += t;
}

void MsgPerfRecord::set_send(long t)
{
     msgSendCount++;
     msgSendTinc += t;
}

void MsgPerfRecord::set_Total(long t)
{
     msgTotalCount++;
     msgTotalTinc += t;
}

std::function<void()> MsgPerfRecord::write_perf_log()
{
    return [this]() {
        string threadName = "sa-msg-perf";
	pthread_setname_np(pthread_self(), threadName.c_str());
	std::cout << "Server Adaptor: msg performance tick start" << std::endl;
 
	std::unique_ptr<struct tm> ltm = std::make_unique<struct tm>();
	struct timeval tv {};
	struct timezone tz {};
	gettimeofday(&tv, &tz);
	localtime_r(&tv.tv_sec, ltm.get());

	char fileName[SHORT_BUFF_LEN] = { 0 };
	sprintf(fileName, "/var/log/gcache/sa_msg_perf.%04d%02d%02d-%02d%02d%02d.log", (1900 + ltm.get()->tm_year),
	    (1 + ltm.get()->tm_mon), ltm.get()->tm_mday, ltm.get()->tm_hour, ltm.get()->tm_min, ltm.get()->tm_sec);

	outfile.open(fileName, ios::out | ios::app);
	outfile << "******************start******************" << std::endl;
	while (!finish) {
	    sleep(3);
	    if (msgRecvCountPre == msgRecvCount && msgSendCountPre == msgSendCount &&
		msgTotalCountPre == msgTotalCount) {
	        continue;
	    }
	    msgRecvCountPre = msgRecvCount;
	    msgSendCountPre = msgSendCount;
	    msgTotalCountPre = msgTotalCount;
	    gettimeofday(&tv, &tz);
		localtime_r(&tv.tv_sec, ltm.get());
	    char curTime[SHORT_BUFF_LEN] = { 0 };
	    sprintf(curTime, "%02d:%02d:%02d", ltm.get()->tm_hour, ltm.get()->tm_min, ltm.get()->tm_sec);
	    outfile << curTime << " *********************************************************" << std::endl;
	    outfile << "msgRecvCount = " << msgRecvCount << setw(16) << "   avg_recv_lat = " << msgRecvTinc / msgRecvCount << std::endl;
	    outfile << "msgSendCount = " << msgSendCount << setw(16) << "   avg_send_lat = " << msgSendTinc / msgSendCount << std::endl;
	    outfile << "msgTotalCount = " << msgTotalCount << setw(16) << "  avg_total_lat = " << msgTotalTinc / msgTotalCount << std::endl;
	}
	outfile << "******************finish******************" << std::endl;
	outfile.close();
    };
}

void MsgPerfRecord::start()
{
    std::function<void()> perf_thread = write_perf_log();
    threads.push_back(std::thread(perf_thread));
}

void MsgPerfRecord::stop()
{
    finish = true;
    for (auto &i : threads) {
        i.join();
    }
}
