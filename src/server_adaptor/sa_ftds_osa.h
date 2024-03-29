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

#ifndef SA_FTDS_H
#define SA_FTDS_H

/*
 * FTDS range for persistence layer is 0~149, 950~1023, stream use of 100~149
 *
 * NOTE: Stream and server, VDB will not exist in one process at the same time,
 * Therefore, we can reuse the range of the server and VDB.
 */

constexpr int SA_FTDS_OP_LIFE = 440;
constexpr int SA_FTDS_MOSDOP_ENQUEUE = 441;
constexpr int SA_FTDS_QUEUE_PERIOD = 442;
constexpr int SA_FTDS_TRANS_OPREQ = 443;
constexpr int SA_FTDS_LOCK_ONE = 444;
constexpr int SA_FTDS_WRITE_QOS = 445;
constexpr int SA_FTDS_WRITE_OP_TOTAL = 446;
constexpr int SA_FTDS_READ_OP_TOTAL = 447;
constexpr int SA_FTDS_COUNT = 448;

#endif
