/* License:LGPL-2.1
 *
 * Copyright (c) 2021 Huawei Technologies Co., Ltd All rights reserved.
 *
 */

#ifndef SA_O_H
#define SA_O_H

#include "sa_def.h"

#include <map>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OSA_API_PUBLIC
#define OSA_API_PUBLIC __attribute__((visibility ("default")))
#endif

class SaExport;
OSA_API_PUBLIC int OSA_Init(SaExport &sa);
OSA_API_PUBLIC int OSA_Finish();
OSA_API_PUBLIC int OSA_FinishCacheOps(void *p, unsigned long int t, unsigned long int l, int r);
OSA_API_PUBLIC void OSA_ProcessBuf(const char *buf, unsigned int len, int cnt, void *p);

OSA_API_PUBLIC void OSA_EncodeOmapGetkeys(const SaBatchKeys *batchKeys, int i, void *p);
OSA_API_PUBLIC void OSA_EncodeOmapGetvals(const SaBatchKv *KVs, int i, void *p);
OSA_API_PUBLIC void OSA_EncodeOmapGetvalsbykeys(const SaBatchKv *keyValue, int i, void *p);
OSA_API_PUBLIC void OSA_EncodeRead(uint64_t opType, unsigned int offset, unsigned int len, char *buf,
    unsigned int bufLen, int i, void *p);
OSA_API_PUBLIC void OSA_SetOpResult(int i, int32_t ret, void *p);
OSA_API_PUBLIC void OSA_EncodeXattrGetxattr(const SaBatchKv *keyValue, int i, void *p);
OSA_API_PUBLIC void OSA_EncodeXattrGetxattrs(const SaBatchKv *keyValue, int i, void *p);
OSA_API_PUBLIC void OSA_EncodeGetOpstat(uint64_t psize, time_t ptime, int i, void *p);
OSA_API_PUBLIC int OSA_ExecClass(SaOpContext *pctx, PREFETCH_FUNC prefetch);
OSA_API_PUBLIC void OSA_EncodeListSnaps(const ObjSnaps *objSnaps, int i, void *p);

#ifdef __cplusplus
}
#endif

#endif
