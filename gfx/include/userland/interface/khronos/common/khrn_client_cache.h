/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef KHRN_CLIENT_CACHE_H
#define KHRN_CLIENT_CACHE_H

//#define WORKAROUND_HW2551     // define to pad header structure to 32 bytes

#include "interface/khronos/common/khrn_client_pointermap.h"
#include "interface/khronos/common/khrn_client.h"

typedef struct CACHE_LINK_S {
   struct CACHE_LINK_S *prev;
   struct CACHE_LINK_S *next;
} CACHE_LINK_T;

typedef struct {
   CACHE_LINK_T link;

   int len;
   int key;

#ifdef WORKAROUND_HW2551
   uint8_t pad[16];
#endif

   //on the server side
   //we store a KHRN_INTERLOCK_T in the
   //same space as this struct
   uint8_t pad_for_interlock[24];

   uint8_t data[1];
} CACHE_ENTRY_T;

typedef struct {
   uint8_t *tree;
   uint8_t *data;

   int client_depth;
   int server_depth;

   CACHE_LINK_T start;
   CACHE_LINK_T end;

   KHRN_POINTER_MAP_T map;
} KHRN_CACHE_T;

#define CACHE_LOG2_BLOCK_SIZE    6
#define CACHE_MAX_DEPTH          16

#define CACHE_SIG_ATTRIB_0    0
#define CACHE_SIG_ATTRIB_1    1
#define CACHE_SIG_ATTRIB_2    2
#define CACHE_SIG_ATTRIB_3    3
#define CACHE_SIG_ATTRIB_4    4
#define CACHE_SIG_ATTRIB_5    5
#define CACHE_SIG_ATTRIB_6    6
#define CACHE_SIG_ATTRIB_7    7

#define CACHE_SIG_INDEX       8

extern int khrn_cache_init(KHRN_CACHE_T *cache);
extern void khrn_cache_term(KHRN_CACHE_T *cache);

extern int khrn_cache_lookup(CLIENT_THREAD_STATE_T *thread, KHRN_CACHE_T *cache, const void *data, int len, int sig);
extern int khrn_cache_get_entries(KHRN_CACHE_T *cache);

#endif
