// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

/*
 * 2021.11.15 - Modify function implementation as GlobalCache needed.
 * 		Huawei Technologies Co., Ltd.<foss@huawei.com>
 */

#include "common/config.h"
#include "common/debug.h"

#include "objclass/objclass.h"
#include "osd/osd_types.h"

#include "osd/ClassHandler.h"
#include "messages/MOSDOp.h"

#include "auth/Crypto.h"
#include "common/armor.h"
#include "sa_def.h"
#include "salog.h"

using namespace std;

namespace{
const string LOG_TYPE = "MSG";
}

static constexpr int dout_subsys = ceph_subsys_objclass;

static ClassHandler *ch;

void cls_initialize(ClassHandler *h)
{
	ch = h;
}

void cls_finalize()
{
	ch = NULL;
}


void *cls_alloc(size_t size)
{
	return malloc(size);
}

void cls_free(void *p)
{
	free(p);
}

int cls_register(const char *name, cls_handle_t *handle)
{
	ClassHandler::ClassData *cls = ch->register_class(name);
	*handle = (cls_handle_t)cls;
	return (cls != NULL);
}

int cls_unregister(cls_handle_t handle)
{
	ClassHandler::ClassData *cls = (ClassHandler::ClassData *)handle;
	ch->unregister_class(cls);
	return 1;
}

int cls_register_method(cls_handle_t hclass, const char *method, int flags, cls_method_call_t class_call,
	cls_method_handle_t *handle)
{
	if(!(flags & (CLS_METHOD_RD | CLS_METHOD_WR)))
		return -EINVAL;
	ClassHandler::ClassData *cls = (ClassHandler::ClassData *)hclass;
	cls_method_handle_t hmethod = (cls_method_handle_t)cls->register_method(method, flags, class_call);
	if(handle)
		*handle = hmethod;
	return (hmethod != NULL);
}

int cls_register_cxx_method(cls_handle_t hclass, const char *method, int flags, cls_method_cxx_call_t class_call,
	cls_method_handle_t *handle)
{
	ClassHandler::ClassData *cls = (ClassHandler::ClassData *)hclass;
	cls_method_handle_t hmethod = (cls_method_handle_t)cls->register_cxx_method(method, flags, class_call);
	if(handle)
		*handle = hmethod;
	return (hmethod != NULL);
}

int cls_unregister_method(cls_method_handle_t handle)
{
	ClassHandler::ClassMethod *method = (ClassHandler::ClassMethod *)handle;
	method->unregister();
	return 1;
}

int cls_getxattr(cls_method_context_t hctx, const char *name, char **outdata, int *outdatalen)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;
	
	op.opSubType = CEPH_OSD_OP_GETXATTR;
	op.objName = ptr->get_oid().name;
	op.keys.push_back(string(name));

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if (r < 0)
		return r;

	*outdata = (char *)malloc(ops[0].outdata.length());
	if (!*outdata)
		return -ENOMEM;
	memcpy(*outdata, ops[0].outdata.c_str(),ops[0].outdata.length());
	*outdatalen = ops[0].outdata.length();

	return r;
}	

int cls_setxattr(cls_method_context_t hctx, const char *name, const char *value, int val_len)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;

	op.opSubType = CEPH_OSD_OP_SETXATTR;
	op.objName = ptr->get_oid().name;
	op.keys.push_back(string(name));
	op.values.push_back(string(value, val_len));

	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return pctx->cbFunc(opreq);
}

int cls_get_request_origin(cls_method_context_t hctx, entity_inst_t *origin)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);

	*origin = ptr->get_orig_source_inst();
	return 0;
}

uint64_t cls_get_features(cls_method_context_t hctx)
{
	return CEPH_FEATURE_CRUSH_TUNABLES | CEPH_FEATURE_CRUSH_TUNABLES2 | CEPH_FEATURE_CRUSH_TUNABLES3 |
		CEPH_FEATURE_CRUSH_V2 | CEPH_FEATURE_OSDHASHPSPOOL | CEPH_FEATURE_OSD_PRIMARY_AFFINITY;
}

uint64_t cls_get_client_features(cls_method_context_t hctx)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);

	return ptr->get_connection()->get_features();
}

int cls_cxx_create(cls_method_context_t hctx, bool exclusive)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	SaOpReq opreq = *pOpReq;
	OpRequestOps op;
	op.opSubType = CEPH_OSD_OP_CREATE;
	op.objName = ptr->get_oid().name;

	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return pctx->cbFunc(opreq);
}

int cls_cxx_remove(cls_method_context_t hctx)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	SaOpReq opreq = *pOpReq;
	OpRequestOps op;
	op.opSubType = CEPH_OSD_OP_DELETE;
	op.objName = ptr->get_oid().name;

	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return pctx->cbFunc(opreq);
}

int cls_cxx_stat(cls_method_context_t hctx, uint64_t *size, time_t *mtime)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;

	op.opSubType = CEPH_OSD_OP_STAT;
	op.objName = ptr->get_oid().name;

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if(r < 0)
		return r;

	auto iter = ops[0].outdata.cbegin();
	utime_t ut;
	uint64_t s;
	try{
		decode(s, iter);
		decode(ut, iter);
	} catch (buffer::error &err){
		return -EIO;
	}
	if(size)
		*size = s;
	if(mtime)
		*mtime = ut.sec();
	return 0;
}

int cls_cxx_stat2(cls_method_context_t hctx, uint64_t *size, ceph::real_time *mtime)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;

	op.opSubType = CEPH_OSD_OP_STAT;
	op.isRbd = pOpReq->vecOps[pctx->opId].isRbd;
	op.rbdObjId.head = pOpReq->vecOps[pctx->opId].rbdObjId.head;
	op.rbdObjId.seq = pOpReq->vecOps[pctx->opId].rbdObjId.seq;
	op.rbdObjId.version = pOpReq->vecOps[pctx->opId].rbdObjId.version;
	op.rbdObjId.format = pOpReq->vecOps[pctx->opId].rbdObjId.format;
	op.rbdObjId.poolId = pOpReq->vecOps[pctx->opId].rbdObjId.poolId;
	op.objName = pOpReq->vecOps[pctx->opId].objName; // ptr->get_oid().name;

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if(r < 0)
		return r;

	auto iter = ops[0].outdata.cbegin();
	real_time ut;
	uint64_t s;
	try{
		decode(s, iter);
		decode(ut, iter);
	} catch (buffer::error &err){
		return -EIO;
	}
	if(size)
		*size = s;
	if(mtime)
		*mtime = ut;
	return 0;
}

int cls_cxx_read(cls_method_context_t hctx, int ofs, int len, bufferlist *outbl)
{
	return cls_cxx_read2(hctx, ofs, len, outbl, 0);
}

int cls_cxx_read2(cls_method_context_t hctx, int ofs, int len, 
		bufferlist *outbl, uint32_t op_flags)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;

	if (len <= 0 || len >= (INT_MAX -2)) {
		return -EINVAL;
	}
	op.opSubType = CEPH_OSD_OP_SYNC_READ;
	op.objName = ptr->get_oid().name;
	op.objOffset = ofs;
	op.objLength = len;
	op.outData = new char[len];
	op.outDataLen = len;

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if(r < 0)
		return r;

	outbl->claim(ops[0].outdata);
	return outbl->length();
}

int cls_cxx_write(cls_method_context_t hctx, int ofs, int len, bufferlist *inbl)
{
	return cls_cxx_write2(hctx, ofs, len, inbl, 0);
}

int cls_cxx_write2(cls_method_context_t hctx, int ofs, int len, bufferlist *inbl, uint32_t op_flags)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_WRITE;
	op.isRbd = pOpReq->vecOps[pctx->opId].isRbd;
	op.rbdObjId.head = pOpReq->vecOps[pctx->opId].rbdObjId.head;
	op.rbdObjId.seq = pOpReq->vecOps[pctx->opId].rbdObjId.seq;
	op.rbdObjId.version = pOpReq->vecOps[pctx->opId].rbdObjId.version;
	op.rbdObjId.format = pOpReq->vecOps[pctx->opId].rbdObjId.format;
	op.rbdObjId.poolId = pOpReq->vecOps[pctx->opId].rbdObjId.poolId;
	op.objName = pOpReq->vecOps[pctx->opId].objName; // ptr->get_oid().name;
	op.objOffset = ofs;
	op.objLength = len;
	op.inData = inbl->c_str();
	op.inDataLen = inbl->length();

	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return pctx->cbFunc(opreq);
}

int cls_cxx_write_full(cls_method_context_t hctx, bufferlist *inbl)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_WRITEFULL;
	op.objName = ptr->get_oid().name;
	op.objOffset = 0;
	op.objLength = inbl->length();
	op.inData = inbl->c_str();
	op.inDataLen = inbl->length();

	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return pctx->cbFunc(opreq);
}

int cls_cxx_getxattr(cls_method_context_t hctx, const char *name, bufferlist *outbl)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;

	op.opSubType = CEPH_OSD_OP_GETXATTR;
	op.objName = ptr->get_oid().name;
	op.keys.push_back(string(name));

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if(r < 0)
		return r;

	outbl->claim(ops[0].outdata);
	return outbl->length();
}

int cls_cxx_getxattrs(cls_method_context_t hctx, map<string, bufferlist> *attrset)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;

	op.opSubType = CEPH_OSD_OP_GETXATTRS;
	op.objName = ptr->get_oid().name;

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if( r < 0)
		return r;

	auto iter = ops[0].outdata.cbegin();
	try{
		decode(*attrset, iter);
	} catch (buffer::error &err){
		return -EIO;
	}
	return 0;	
}

int cls_cxx_setxattr(cls_method_context_t hctx, const char *name, bufferlist *inbl)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_SETXATTR;
	op.objName = ptr->get_oid().name;
	op.keys.push_back(string(name));
	string val;
	auto bp = inbl->cbegin();
	bp.copy(inbl->length(), val);
	op.values.push_back(val);

	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return pctx->cbFunc(opreq);
}

int cls_cxx_map_get_all_vals(cls_method_context_t hctx, map<string, bufferlist> *vals, bool *more)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;
	uint64_t max_to_get = -1;
	op.opSubType = CEPH_OSD_OP_OMAPGETVALS;
	op.objName = ptr->get_oid().name;
	op.keys.push_back("start_after");
	op.values.push_back(string(""));

	op.keys.push_back("max_return");
	op.values.push_back(to_string(max_to_get));

	op.keys.push_back("filter_prefix");
	op.values.push_back(string(""));

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if( r < 0)
		return r;

	auto iter = ops[0].outdata.cbegin();
	try{
		decode(*vals, iter);
		decode(*more, iter);
	} catch (buffer::error &err){
		return -EIO;
	}
	return vals->size();		
}

int cls_cxx_map_get_keys(cls_method_context_t hctx, const string &start_obj, uint64_t max_to_get, set<string> *keys,
	bool *more)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;
	op.opSubType = CEPH_OSD_OP_OMAPGETKEYS;
	op.objName = ptr->get_oid().name;
	op.keys.push_back("start_after");
	op.values.push_back(start_obj);

	op.keys.push_back("max_return");
	op.values.push_back(to_string(max_to_get));

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if( r < 0)
		return r;

	auto iter = ops[0].outdata.cbegin();
	try{
		decode(*keys, iter);
		decode(*more, iter);
	} catch (buffer::error &err){
		return -EIO;
	}
	return keys->size();			
}

int cls_cxx_map_get_vals(cls_method_context_t hctx, const string &start_obj, const string &filter_prefix, 
	uint64_t max_to_get, map<string, bufferlist> *vals, bool *more)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;
	
	op.opSubType = CEPH_OSD_OP_OMAPGETVALS;
	op.objName = ptr->get_oid().name;
	op.keys.push_back("start_after");
	op.values.push_back(start_obj);

	op.keys.push_back("max_return");
	op.values.push_back(to_string(max_to_get));

	op.keys.push_back("filter_prefix");
	op.values.push_back(filter_prefix);
	
	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if( r < 0)
		return r;

	auto iter = ops[0].outdata.cbegin();
	try{
		decode(*vals, iter);
		decode(*more, iter);
	} catch (buffer::error &err){
		return -EIO;
	}
	return vals->size();		
}

int cls_cxx_map_read_header(cls_method_context_t hctx, bufferlist *outbl)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;

	op.opSubType = CEPH_OSD_OP_OMAPGETHEADER;
	op.objName = ptr->get_oid().name;

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if(r < 0)
		return r;

	outbl->claim(ops[0].outdata);

	return 0;	
}

int cls_cxx_map_get_val(cls_method_context_t hctx, const string &key, bufferlist *outbl)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	int r;

	op.opSubType = CEPH_OSD_OP_OMAPGETVALSBYKEYS;
	op.objName = ptr->get_oid().name;
	op.keys.push_back(key);

	vector<OSDOp> ops(1);
	ops.swap(ptr->ops);
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	r = pctx->cbFunc(opreq);
	ops.swap(ptr->ops);
	if(r < 0)
		return r;

	auto iter = ops[0].outdata.cbegin();
	try{
		map<string, bufferlist> m;

		decode(m, iter);
		map<string, bufferlist>::iterator iter = m.begin();
		if(iter == m.end())
			return -ENOENT;

		*outbl = iter->second;
	} catch (buffer::error &e){
		return -EIO;
	}
	return 0;					
}

int cls_cxx_map_set_val(cls_method_context_t hctx, const string &key, bufferlist *inbl)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_OMAPSETVALS;
	op.objName = ptr->get_oid().name;
	op.keys.push_back(key);
	string val;
	auto bp = inbl->cbegin();
	bp.copy(inbl->length(), val);
	op.values.push_back(val);
	
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return  pctx->cbFunc(opreq);
}

int cls_cxx_map_set_vals(cls_method_context_t hctx, const std::map<string, bufferlist> *map)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_OMAPSETVALS;
	op.objName = ptr->get_oid().name;
 	for (auto i = map->begin(); i != map->end(); ++i) {
		std::string key = i->first;
		string val;
		auto bp = i->second.cbegin();
		bp.copy(i->second.length(), val);
		op.keys.push_back(key);
		op.values.push_back(val);
	}
	
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);	
	return pctx->cbFunc(opreq);
}

int cls_cxx_map_clear(cls_method_context_t hctx)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_OMAPCLEAR;
	op.objName = ptr->get_oid().name;
	
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);	
	return pctx->cbFunc(opreq);
}

int cls_cxx_map_write_header(cls_method_context_t hctx, bufferlist *inbl)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_OMAPSETHEADER;
	op.objName = ptr->get_oid().name;
	op.inDataLen = inbl->length();
	op.inData = inbl->c_str();

	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);	
	return pctx->cbFunc(opreq);
}

int cls_cxx_map_remove_key(cls_method_context_t hctx, const string &key)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	SaOpReq *pOpReq = pctx->opReq;
	SaOpReq opreq = *pOpReq;
	MOSDOp *ptr = reinterpret_cast<MOSDOp *>(pOpReq->ptrMosdop);
	OpRequestOps op;
	
	op.opSubType = CEPH_OSD_OP_OMAPRMKEYS;
	op.objName = ptr->get_oid().name;
	op.keys.push_back(key);
	
	opreq.vecOps.clear();
	opreq.vecOps.push_back(op);
	return pctx->cbFunc(opreq);
}

int cls_gen_random_bytes(char *buf, int size)
{
	ch->cct->random()->get_bytes(buf, size);
	return 0;
}

int cls_gen_rand_base64(char *dest, int size)
{
	char buf[size];
	char tmp_dest[size + 4];
	int ret;

	ret = cls_gen_random_bytes(buf, sizeof(buf));
	if(ret < 0){
		lgeneric_derr(ch->cct) << "cannot get random bytes: " << ret << dendl;
		return -1;
	}

	ret = ceph_armor(tmp_dest, &tmp_dest[sizeof(tmp_dest)],(const char *)buf,
		((const char *)buf) + ((size - 1) * 3 + 4 - 1) / 4);
	if(ret <0){
		lgeneric_derr(ch->cct) << "ceph_armor failed" << dendl;
		return -1;
	}
	tmp_dest[ret] = '\0';
	memcpy(dest, tmp_dest, size);
	dest[size - 1] = '\0';

	return 0;
}

uint64_t cls_current_version(cls_method_context_t hctx)
{
	return 0;
}


int cls_current_subop_num(cls_method_context_t hctx)
{
	SaOpContext *pctx = reinterpret_cast<SaOpContext *>(hctx);
	return pctx->opId;
}

int cls_log(int level, const char *format, ...)
{
	int size = 256;
	va_list ap;
	while(1){
		char buf[size];
		va_start(ap, format);
		int n = vsnprintf(buf, size, format, ap);
		va_end(ap);
#define MAX_SIZE 8196
		if((n > -1 && n < size) || size > MAX_SIZE){
			Salog(LV_DEBUG, LOG_TYPE, "%s", buf);
			return n;
		}
		size *=2;
	}
}
