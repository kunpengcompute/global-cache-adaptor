#ifndef _GCBUFFERLIST_H_
#define _GCBUFFERLIST_H_

#include <stdint.h>
#include <stddef.h>

typedef struct ListEntryType
{
    char *buf;
    uint32_t len;
} ListEntry;

#define ENTRY_PER_BUFFLIST 64
typedef struct GcBufferListType
{
    struct GcBufferListType *nextBufferList;
    uint16_t entrySumList;
    ListEntry entrys[ENTRY_PER_BUFFLIST];
} GcBufferList;

#endif