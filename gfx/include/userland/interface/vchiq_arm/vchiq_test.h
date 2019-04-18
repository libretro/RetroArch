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

#ifndef VCHIQ_TEST_H
#define VCHIQ_TEST_H

#include "vchiq_test_if.h"

#define VCOS_LOG_CATEGORY (&vchiq_test_log_category)

#define VERBOSE_TRACE 1

#define FUNC_FOURCC VCHIQ_MAKE_FOURCC('f','u','n','c')
#define FUN2_FOURCC VCHIQ_MAKE_FOURCC('f','u','n','2')

#define SERVICE1_DATA_SIZE 1024
#define SERVICE2_DATA_SIZE 2048
#define FUN2_MAX_DATA_SIZE 16384
#define FUN2_MAX_ALIGN     4096
#define BULK_ALIGN_SIZE    4096

#define VCHIQ_TEST_VER     3

enum {
   MSG_ERROR,
   MSG_ONEWAY,
   MSG_ASYNC,
   MSG_SYNC,
   MSG_CONFIG,
   MSG_ECHO
};

struct test_params
{
   int magic; /* = MSG_CONFIG */
   int blocksize;
   int iters;
   int verify;
   int echo;
   int align_size;
   int client_align;
   int server_align;
   int client_message_quota;
   int server_message_quota;
};

#if VERBOSE_TRACE

#define EXPECT(_e, _v) if (_e != _v) { vcos_log_error("%d: " #_e " != " #_v, __LINE__); VCOS_BKPT; goto error_exit; } else { vcos_log_trace("%d: " #_e " == " #_v, __LINE__); }

#define START_CALLBACK(_r, _u) \
   if (++callback_index == callback_count) { \
      if (reason != _r) { \
         vcos_log_error("%d: expected callback reason " #_r ", got %d", __LINE__, reason); VCOS_BKPT; goto error_exit; \
      } \
      else if ((int)VCHIQ_GET_SERVICE_USERDATA(service) != _u) { \
         vcos_log_error("%d: expected userdata %d, got %d", __LINE__, _u, (int)VCHIQ_GET_SERVICE_USERDATA(service)); VCOS_BKPT; goto error_exit; \
      } \
      else \
      { \
         vcos_log_trace("%d: " #_r ", " #_u, __LINE__); \
      }

#define START_BULK_CALLBACK(_r, _u, _bu)   \
   if (++bulk_index == bulk_count) {  \
      if (reason != _r) { \
         vcos_log_error("%d: expected callback reason " #_r ", got %d", __LINE__, reason); VCOS_BKPT; goto error_exit; \
      } \
      else if ((int)VCHIQ_GET_SERVICE_USERDATA(service) != _u) { \
         vcos_log_error("%d: expected userdata %d, got %d", __LINE__, _u, (int)VCHIQ_GET_SERVICE_USERDATA(service)); VCOS_BKPT; goto error_exit; \
      } \
      else if ((int)bulk_userdata != _bu) { \
         vcos_log_error("%d: expected bulk_userdata %d, got %d", __LINE__, _bu, (int)bulk_userdata); VCOS_BKPT; goto error_exit; \
      } \
      else \
      { \
         vcos_log_trace("%d: " #_r ", " #_u ", " #_bu, __LINE__); \
      }

#else

#define EXPECT(_e, _v) if (_e != _v) { vcos_log_trace("%d: " #_e " != " #_v, __LINE__); VCOS_BKPT; goto error_exit; }

#define START_CALLBACK(_r, _u) \
   if (++callback_index == callback_count) { \
      if (reason != _r) { \
         vcos_log_error("%d: expected callback reason " #_r ", got %d", __LINE__, reason); VCOS_BKPT; goto error_exit; \
      } \
      else if ((int)VCHIQ_GET_SERVICE_USERDATA(service) != _u) { \
         vcos_log_error("%d: expected userdata %d, got %d", __LINE__, _u, (int)VCHIQ_GET_SERVICE_USERDATA(service)); VCOS_BKPT; goto error_exit; \
      }

#define START_BULK_CALLBACK(_r, _u, _bu)   \
   if (++bulk_index == bulk_count) {  \
      if (reason != _r) { \
         vcos_log_error("%d: expected callback reason " #_r ", got %d", __LINE__, reason); VCOS_BKPT; goto error_exit; \
      } \
      else if ((int)VCHIQ_GET_SERVICE_USERDATA(service) != _u) { \
         vcos_log_error("%d: expected userdata %d, got %d", __LINE__, _u, (int)VCHIQ_GET_SERVICE_USERDATA(service)); VCOS_BKPT; goto error_exit; \
      } \
      else if ((int)bulk_userdata != _bu) { \
         vcos_log_error("%d: expected bulkuserdata %d, got %d", __LINE__, _bu, (int)bulk_userdata); VCOS_BKPT; goto error_exit; \
      }

#endif

#define END_CALLBACK(_s) \
      return _s; \
   }

#endif
