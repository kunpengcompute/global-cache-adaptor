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

#ifndef _GCBUFFERLIST_H_
#define _GCBUFFERLIST_H_

#include <stdint.h>
#include <stddef.h>

typedef struct ListEntryType
{
    char  *buf;         /* 页面数据起始地址 */
    uint32_t len;          /* 有效数据长度，单位为byte */
} ListEntry;

#define ENTRY_PER_BUFFLIST 64
typedef struct GcBufferListType
{
    struct GcBufferListType *nextBufferList;           /* 下一个list指针，用于组成list链 */
    uint16_t     entrySumList;         /* 本list中list_entry数据 */
    ListEntry  entrys[ENTRY_PER_BUFFLIST]; /* list_entry数组*/
} GcBufferList;

#endif