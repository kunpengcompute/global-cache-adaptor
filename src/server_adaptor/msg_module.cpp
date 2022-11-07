/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 * 
 */

#include "msg_module.h"
#include "objclass/objclass.h"
#include "osd/ClassHandler.h"

#include <cstdlib>
#include <time.h>
#include "salog.h"

using namespace std;

namespace {
const string LOG_TYPE = "MSG";
}

static void decode_str_str_map_to_bl(bufferlist::const_iterator &p, bufferlist *out)
{
    auto start = p;
    __u32 n;
    decode(n, p);
    unsigned len = 4;
    while (n--) {
        __u32 l;
        decode(l, p);
        p.advance(l);
        len += 4 + l;
        decode(l, p);
        p.advance(l);
        len += 4 + l;
    }
    start.copy(len, *out);
}

int MsgModule::ConvertClientopToOpreq(OSDOp &clientop, OpRequestOps &oneOp, OptionsType &optionType,
    OptionsLength &optionLength, long tid)
{
    int ret = 0;
    oneOp.opSubType = clientop.op.op;
    SaDatalog("Converting Clientop tid=%ld obj=%s type=0x%lX", tid, oneOp.objName.c_str(), oneOp.opSubType);
    switch (oneOp.opSubType) {
        case CEPH_OSD_OP_SPARSE_READ:
        case CEPH_OSD_OP_SYNC_READ:
        case CEPH_OSD_OP_READ: {
            optionType.read++;
            optionLength.read += clientop.op.extent.length / 1024;
            oneOp.objOffset = clientop.op.extent.offset;
            oneOp.objLength = clientop.op.extent.length;
            SaDatalog("Converting READ tid=%ld obj=%s offset=%u length=%u",
                tid, oneOp.objName.c_str(), oneOp.objOffset, oneOp.objLength);
            ConvertObjRw(clientop, oneOp);
    	} break;
        case CEPH_OSD_OP_WRITEFULL:
        case CEPH_OSD_OP_WRITE: {
	        optionType.write++;
            optionLength.write += clientop.op.extent.length / 1024;		
            oneOp.objOffset = clientop.op.extent.offset;
            oneOp.objLength = clientop.op.extent.length;
            SaDatalog("Converting WRITE/CEPH_OSD_OP_WRITEFULL tid=%ld obj=%s offset=%u length=%u",
                tid, oneOp.objName.c_str(), oneOp.objOffset, oneOp.objLength);
            ConvertObjRw(clientop, oneOp);
        } break;
        case CEPH_OSD_OP_GETXATTR:
        case CEPH_OSD_OP_RMXATTR:
        case CEPH_OSD_OP_CMPXATTR:
        case CEPH_OSD_OP_SETXATTR: {
            ConvertAttrOp(clientop, oneOp);
            Salog(LV_DEBUG, LOG_TYPE, "Print Attrs key and value:%lu", oneOp.opSubType);
            if (oneOp.keys.size() == oneOp.values.size()) {
                for (unsigned int i = 0; i < oneOp.keys.size(); i++) {
                    Salog(LV_DEBUG, LOG_TYPE, "<%s,%s>", oneOp.keys[i].c_str(), oneOp.values[i].c_str());
                }
            } else {
                string logTmp;
                for (auto &i : oneOp.keys) {
                    logTmp += i;
                    logTmp += "-";
                }
                Salog(LV_DEBUG, LOG_TYPE, "Print Attrs keys:%s", logTmp.c_str());
                logTmp.clear();
                for (auto &i : oneOp.values) {
                    logTmp += i;
                    logTmp += "-";
                }
                Salog(LV_DEBUG, LOG_TYPE, "Print Attrs values:%s", logTmp.c_str());
            }
        } break;
        case CEPH_OSD_OP_OMAPGETVALS:
        case CEPH_OSD_OP_OMAPSETVALS:
        case CEPH_OSD_OP_OMAPGETKEYS:
        case CEPH_OSD_OP_OMAPRMKEYS:
        case CEPH_OSD_OP_OMAPCLEAR:
        case CEPH_OSD_OP_OMAPGETVALSBYKEYS:
        case CEPH_OSD_OP_OMAP_CMP:
        case CEPH_OSD_OP_OMAPSETHEADER:
        case CEPH_OSD_OP_OMAPGETHEADER: {
            ConvertOmapOp(clientop, oneOp);
            Salog(LV_DEBUG, LOG_TYPE, "Print OMAP key and value:%lu", oneOp.opSubType);
            if (oneOp.keys.size() == oneOp.values.size()) {
                for (unsigned int i = 0; i < oneOp.keys.size(); i++) {
                    Salog(LV_DEBUG, LOG_TYPE, "<%s,%s>", oneOp.keys[i].c_str(), oneOp.values[i].c_str());
                }
            } else {
                string logTmp; 
                for (auto &i : oneOp.keys) {
                    logTmp += i;
                    logTmp += "-";
                }
                Salog(LV_DEBUG, LOG_TYPE, "Print OMAP keys:%s", logTmp.c_str());
                logTmp.clear();
                for (auto &i : oneOp.values) {
                    logTmp += i;
                    logTmp += "-";
                }
                Salog(LV_DEBUG, LOG_TYPE, "Print OMAP values:%s", logTmp.c_str());
            }
        } break;
        case CEPH_OSD_OP_GETXATTRS:
        case CEPH_OSD_OP_STAT:
            break;
        case CEPH_OSD_OP_CALL: {
            string cname, mname;
            auto bp = clientop.indata.cbegin();
            try {
                bp.copy(clientop.op.cls.class_len, cname);
                bp.copy(clientop.op.cls.method_len, mname);
            } catch (buffer::error &e) {
                Salog(LV_ERROR, LOG_TYPE, "unable to decode class [%s] + method[%s]", cname.c_str(), mname.c_str());
            }
            if (cname.compare("rbd") == 0 && mname.compare("copyup") == 0) {
                ret = 1;
                SaDatalog("Converting COPYUP tid=%ld obj=%s", tid, oneOp.objName.c_str());
            } 
        } break;
        case CEPH_OSD_OP_LIST_SNAPS: {
            SaDatalog("Converting COPYUP tid=%ld obj=%s", tid, oneOp.objName.c_str());
        }
            break;
        case CEPH_OSD_OP_CREATE:
            oneOp.opFlags = clientop.op.flags;
            break;
        case CEPH_OSD_OP_ROLLBACK:
            ConvertRollBackOp(clientop, oneOp);
            Salog(LV_DEBUG, LOG_TYPE, "rollback snapid: %s", oneOp.values[0]);
            break;
        default: {
            Salog(LV_DEBUG, LOG_TYPE, "Translate ClientOp, unknown op:0x%lX", oneOp.opSubType);
        } break;
    }
    return ret;
}

void MsgModule::ConvertObjRw(OSDOp &clientop, OpRequestOps &oneOp)
{
    if (clientop.op.op == CEPH_OSD_OP_READ || clientop.op.op == CEPH_OSD_OP_SPARSE_READ ||
        clientop.op.op == CEPH_OSD_OP_SYNC_READ) {
        oneOp.outDataLen = clientop.op.extent.length;
    } else if (clientop.op.op == CEPH_OSD_OP_WRITE || clientop.op.op == CEPH_OSD_OP_WRITEFULL) {
        oneOp.inData = clientop.indata.c_str();
        oneOp.inDataLen = clientop.indata.length();
    }
}

void MsgModule::ConvertOmapOp(OSDOp &clientop, OpRequestOps &oneOp)
{
    auto bp = clientop.indata.cbegin();
    if (clientop.op.op == CEPH_OSD_OP_OMAPGETVALS) {
        string start_after;
        uint64_t max_return;
        string filter_prefix;

        decode(start_after, bp);
        oneOp.keys.push_back("start_after");
        oneOp.values.push_back(start_after);

        decode(max_return, bp);
        oneOp.keys.push_back("max_return");
        oneOp.values.push_back(to_string(max_return));

        decode(filter_prefix, bp);
        oneOp.keys.push_back("filter_prefix");
        oneOp.values.push_back(filter_prefix);
    } else if (clientop.op.op == CEPH_OSD_OP_OMAPSETVALS) {
        bufferlist to_set_bl;
        map<string, bufferlist> to_set;
        decode_str_str_map_to_bl(bp, &to_set_bl);
        bufferlist::const_iterator pt = to_set_bl.begin();
        decode(to_set, pt);
        for (map<string, bufferlist>::iterator i = to_set.begin(); i != to_set.end(); ++i) {
            std::string key = i->first;
            string val;
            auto bp = i->second.cbegin();
            bp.copy(i->second.length(), val);
            oneOp.keys.push_back(key);
            oneOp.values.push_back(val);
        }
    } else if (clientop.op.op == CEPH_OSD_OP_OMAPGETKEYS) {
        string start_after;
        uint64_t max_return;

        decode(start_after, bp);
        oneOp.keys.push_back("start_after");
        oneOp.values.push_back(start_after);

        decode(max_return, bp);
        oneOp.keys.push_back("max_return");
        oneOp.values.push_back(to_string(max_return));
    } else if (clientop.op.op == CEPH_OSD_OP_OMAPRMKEYS) {
        set<string> keys_to_rm;
        decode(keys_to_rm, bp);
        for (auto key : keys_to_rm) {
            oneOp.keys.push_back(key);
        }
    } else if (clientop.op.op == CEPH_OSD_OP_OMAPGETVALSBYKEYS) {
        set<string> keys_to_get;
        decode(keys_to_get, bp);
        for (auto key : keys_to_get) {
            oneOp.keys.push_back(key);
        }
    } else if ((clientop.op.op == CEPH_OSD_OP_OMAPGETHEADER) || (clientop.op.op == CEPH_OSD_OP_OMAPCLEAR)) {
        // soid
    } else if (clientop.op.op == CEPH_OSD_OP_OMAPSETHEADER) {
        oneOp.inDataLen = clientop.op.extent.length;
        oneOp.inData = (char *)clientop.indata.c_str();
    } else if (clientop.op.op == CEPH_OSD_OP_OMAP_CMP) {
        map<string, pair<bufferlist, int> > assertions;
        decode(assertions, bp);
        set<string> to_get;
        for (map<string, pair<bufferlist, int> >::iterator i = assertions.begin(); i != assertions.end(); ++i) {
            oneOp.keys.push_back(i->first);
            auto &bl = i->second.first;
            std::string val;
            bl.copy(0, bl.length(), val);
            oneOp.values.push_back(val);
            oneOp.subops.push_back(i->second.second);
        }
    }
}

void MsgModule::ConvertAttrOp(OSDOp &clientop, OpRequestOps &oneOp)
{
    ceph_osd_op &op = clientop.op;

    auto bp = clientop.indata.cbegin();
    std::string xattr_name;
    bp.copy(op.xattr.name_len, xattr_name);
    oneOp.keys.push_back(xattr_name);

    if (op.op == CEPH_OSD_OP_SETXATTR) {
        string val;
        bp.copy(op.xattr.value_len, val);
        oneOp.values.push_back(val);
    } else if (op.op == CEPH_OSD_OP_CMPXATTR) {
        oneOp.subops.push_back((int)op.xattr.cmp_op);
        oneOp.cmpModes.push_back(op.xattr.cmp_mode);
        switch (op.xattr.cmp_mode) {
            case CEPH_OSD_CMPXATTR_MODE_STRING: {
                string val;
                bp.copy(op.xattr.value_len, val);
                val[op.xattr.value_len] = 0;
                oneOp.values.push_back(val);
            } break;
            case CEPH_OSD_CMPXATTR_MODE_U64: {
                uint64_t u64val;
                decode(u64val, bp);
                oneOp.u64vals.push_back(u64val);
            } break;
        }
    }
}

void MsgModule::ConvertRollBackOp(OSDOp &clientop, OpRequestOps &oneOp)
{
    ceph_osd_op &op = clientop.op;
    oneOp.keys.push_back("snapid");
    oneOp.values.push_back(to_string(op.snap.snapid));
}