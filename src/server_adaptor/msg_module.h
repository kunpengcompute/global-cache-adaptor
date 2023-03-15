/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef MSG_MODULE_H
#define MSG_MODULE_H

#include <queue>
#include <mutex>

#include <messages/MOSDOp.h>
#include "osd/ClassHandler.h"

#include "sa_def.h"

using MSG_UNIQUE_LOCK = std::unique_lock<std::mutex>;

class MsgModule {
    bool readStatFlag;
    void ConvertObjRw(OSDOp &clientop, OpRequestOps &oneOp);
    void ConvertOmapOp(OSDOp &clientop, OpRequestOps &oneOp);
    void ConvertAttrOp(OSDOp &clientop, OpRequestOps &oneOp);
    void ConvertRollBackOp(OSDOp &clientop, OpRequestOps &oneOp);

public:
    MsgModule(): readStatFlag(false) {}
    ~MsgModule() {}

    int ConvertClientopToOpreq(OSDOp &clientop, OpRequestOps &oneOp,
        OptionsType &optionType, OptionsLength &optionLength, long tid);
    void SetReadStatFlag(bool flag)
    {
        readStatFlag = flag;
    }
};

#endif
