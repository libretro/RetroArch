/*
Copyright (c) 2013, Broadcom Europe Ltd
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

/** \file
 * Multi-Media Abstraction Layer API
 */
#ifndef MMAL_VC_DBGLOG_H
#define MMAL_VC_DBGLOG_H

#include "interface/mmal/mmal.h"
#include "mmal_vc_msgs.h"

/* Debug log for MMAL messages going past */

typedef enum {
   MMAL_DBG_MSG,
   MMAL_DBG_BULK,
   MMAL_DBG_OPENED,
   MMAL_DBG_CLOSED,
   MMAL_DBG_BULK_ACK,
   MMAL_DBG_BULK_TX,
   MMAL_DBG_BULK_RX,
} MMAL_DBG_EVENT_TYPE_T;


/** Debug log data. */
typedef union
{
   struct {
      mmal_worker_msg_header header;
      uint32_t msg[4];     /**< Snarf this much message data */
   } msg;

   struct {
      uint32_t len;        /**< Length of transfer */
      uint32_t data[4];    /**< Snarf this much payload data */
   } bulk;

   uint32_t uint;

   uint32_t arr[15];       /** Pad complete DBG_ENTRY_T to 64 bytes per line */

} MMAL_DBG_DATA_T;

/** One entry in the debug log */
typedef struct
{
   uint32_t time;
   uint32_t event_type;
   MMAL_DBG_DATA_T u;
} MMAL_DBG_ENTRY_T;

#define MMAL_DBG_ENTRIES_MAX  64
#define MMAL_DBG_ENTRIES_MASK (MMAL_DBG_ENTRIES_MAX-1)
#define MMAL_DBG_VERSION  1

/** The debug log itself. This is currently allocated in uncached
  * memory so that the ARM can easily access it.
  */
typedef struct
{
   uint32_t version;
   uint32_t magic;
   uint32_t num_entries;
   uint32_t index;
   uint32_t size;
   uint32_t elemsize;
   uint32_t pad[2];
   MMAL_DBG_ENTRY_T entries[MMAL_DBG_ENTRIES_MAX];

} MMAL_DBG_LOG_T;

extern VCOS_MUTEX_T mmal_dbg_lock;
extern MMAL_DBG_LOG_T *mmal_dbg_log;

/** Get the next event and hold the lock. Should only be
  * accessed by the macros below.
  */
static inline MMAL_DBG_ENTRY_T *mmal_log_lock_event(MMAL_DBG_EVENT_TYPE_T event_type ) {
   uint32_t index;
   MMAL_DBG_ENTRY_T *entry;
   vcos_mutex_lock(&mmal_dbg_lock);
   index = mmal_dbg_log->index++;
   entry = mmal_dbg_log->entries + (index & MMAL_DBG_ENTRIES_MASK);
   entry->time = vcos_getmicrosecs();
   entry->event_type = event_type;
   return entry;
}

/** Release the lock. Should only be accessed by the macros below. */
static inline void mmal_log_unlock_event(void) {
   vcos_mutex_unlock(&mmal_dbg_lock);
}

/** Initialise the logging module. */
MMAL_STATUS_T mmal_vc_dbglog_init(void);

/** Deinitialise the logging module. */
void mmal_vc_dbglog_deinit(void);

/** Put an entry into the log.
  *
  * @param short_type  type of event, e.g. OPENED
  * @param name        union entry name,  e.g. uint.
  * @param event       event data
  */
#define MMAL_LOG_EVENT(short_type, name, value)  {\
   MMAL_DBG_ENTRY_T *entry = mmal_log_lock_event(MMAL_DBG_##short_type); \
   entry->u.name = value;\
   mmal_log_unlock_event(); \
}

/** Log an uint event (i.e. all just the fact that it occurred */
#define LOG_EVENT_UINT(short_type,u) MMAL_LOG_EVENT(short_type, uint, u)

#define LOG_EVENT_OPENED()         LOG_EVENT_UINT(OPENED,0)
#define LOG_EVENT_CLOSED()         LOG_EVENT_UINT(CLOSED,0)
#define LOG_EVENT_BULK_ACK(reason) LOG_EVENT_UINT(BULK_ACK,reason)

/** Log a message. Grabs part of the message data.
  */
#define LOG_EVENT_MSG(header) {\
   MMAL_DBG_ENTRY_T *entry = mmal_log_lock_event(MMAL_DBG_MSG); \
   memcpy(&entry->u.msg, header, sizeof(entry->u.msg)); \
   mmal_log_unlock_event(); \
}

/** Log bulk data. For now, do not grab the actual data itself */
#define LOG_EVENT_BULK(type, len, data) {\
   MMAL_DBG_ENTRY_T *entry = mmal_log_lock_event(MMAL_DBG_BULK_##type); \
   entry->u.bulk.len = len; \
   mmal_log_unlock_event(); \
}


#endif
