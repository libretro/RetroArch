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

#ifndef MMAL_VC_API_H
#define MMAL_VC_API_H

/** @file
  *
  * Public API for MMAL VC client. Most functionality is exposed
  * via MMAL itself.
  */

#include "interface/mmal/mmal_types.h"
#include "interface/mmal/mmal_parameters.h"
#include "interface/mmal/mmal_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/** State of components created by the VC adaptation layer, used for
 * statistics reporting.
 */
typedef enum {
   MMAL_STATS_COMP_IDLE,
   MMAL_STATS_COMP_CREATED,
   MMAL_STATS_COMP_DESTROYING,
   MMAL_STATS_COMP_DESTROYED,
   MMAL_STATS_COMP_UNUSED = 0xffffffff /* force 32bit */
} MMAL_STATS_COMP_STATE_T;

/** Per-component statistics collected by the VC adaptation layer.
 */
struct MMAL_VC_COMP_STATS_T {
   struct MMAL_DRIVER_COMPONENT_T *comp;
   MMAL_STATS_COMP_STATE_T state;
   uint32_t pid;
   uint32_t pool_mem_alloc_size;
   char name[20];
};

/** VC adaptation layer statistics.
 */
struct MMAL_VC_STATS_T
{
   struct
   {
      uint32_t rx;               /**< Count of data buffers received */
      uint32_t rx_zero_copy;     /**< Count of zero-copy data buffers received */
      uint32_t rx_empty;         /**< Empty data buffers (to be filled) */
      uint32_t rx_fails;         /**< Gave up partway through */
      uint32_t tx;               /**< Count of data buffers sent */
      uint32_t tx_zero_copy;     /**< Count of zero-copy data buffers sent */
      uint32_t tx_empty;         /**< Count of empty data buffers sent */
      uint32_t tx_fails;         /**< Gave up partway through */
      uint32_t tx_short_msg;     /**< Messages sent directly in the control message */
      uint32_t rx_short_msg;     /**< Messages received directly in the control message */
   } buffers;
   struct service
   {
      uint32_t created;          /**< How many services created */
      uint32_t pending_destroy;  /**< How many destroyed */
      uint32_t destroyed;        /**< How many destroyed */
      uint32_t failures;         /**< Failures to create a service */
   } service;
   struct commands
   {
      uint32_t bad_messages;
      uint32_t executed;
      uint32_t failed;
      uint32_t replies;
      uint32_t reply_fails;
   } commands;
   struct
   {
      uint32_t tx;               /**< Count of events sent */
      uint32_t tx_fails;         /**< Count of events not fully sent */
   } events;
   struct
   {
      uint32_t created;
      uint32_t destroyed;
      uint32_t destroying;
      uint32_t failed;
      uint32_t list_size;
      struct MMAL_VC_COMP_STATS_T component_list[8];
   } components;
   struct
   {
      uint32_t enqueued_messages;
      uint32_t dequeued_messages;
      uint32_t max_parameter_set_delay;
      uint32_t max_messages_waiting;
   } worker;

};
typedef struct MMAL_VC_STATS_T MMAL_VC_STATS_T;

/* Simple circular text buffer used to store 'interesting' data
 * from MMAL clients. e.g. settings for each picture taken */
struct MMAL_VC_HOST_LOG_T
{
   /** Simple circular buffer of plain text log messages separated by NUL */
   char buffer[16 << 10];
   /** For VCDBG validation and to help detect buffer overflow */
   uint32_t magic;
   /** Write offset into buffer */
   int32_t offset;
   /** Counter of host messages logged since boot */
   unsigned count;
};
typedef struct MMAL_VC_HOST_LOG_T MMAL_VC_HOST_LOG_T;

/** Status from querying MMAL core statistics.
 */
typedef enum
{
   MMAL_STATS_FOUND,
   MMAL_STATS_COMPONENT_NOT_FOUND,
   MMAL_STATS_PORT_NOT_FOUND,
   MMAL_STATS_INVALID = 0x7fffffff
} MMAL_STATS_RESULT_T;

/* If opening dev_vchiq outside mmal/vchiq this is the file path and mode */
#define MMAL_DEV_VCHIQ_PATH "/dev/vchiq"
#define MMAL_DEV_VCHIQ_MODE O_RDWR

MMAL_STATUS_T mmal_vc_init(void);
MMAL_STATUS_T mmal_vc_init_fd(int dev_vchiq_fd);
void mmal_vc_deinit(void);

MMAL_STATUS_T mmal_vc_use(void);
MMAL_STATUS_T mmal_vc_release(void);

MMAL_STATUS_T mmal_vc_get_version(uint32_t *major, uint32_t *minor, uint32_t *minimum);
MMAL_STATUS_T mmal_vc_get_stats(MMAL_VC_STATS_T *stats, int reset);

/** Return the MMAL core statistics for a given component/port.
 *
 * @param stats         Updated with given port statistics
 * @param result        Whether the port/component was found
 * @param name          Filled in with the name of the port
 * @param namelen       Length of name
 * @param component     Which component (indexed from zero)
 * @param port_type     Which type of port
 * @param port          Which port (index from zero)
 * @param reset         Reset the stats.
 */
MMAL_STATUS_T mmal_vc_get_core_stats(MMAL_CORE_STATISTICS_T *stats,
                                     MMAL_STATS_RESULT_T *result,
                                     char *name,
                                     size_t namelen,
                                     MMAL_PORT_TYPE_T type,
                                     unsigned component,
                                     unsigned port,
                                     MMAL_CORE_STATS_DIR dir,
                                     MMAL_BOOL_T reset);
/**
 * Stores an arbitrary text message in a circular buffer inside the MMAL VC server.
 * The purpose of this message is to log high level events from the host in order
 * to diagnose problems that require multiple actions to reproduce. e.g. taking
 * multiple pictures with different settings.
 *
 * @param   msg  The message text.
 * @return  MMAL_SUCCESS if the message was logged or MMAL_ENOSYS if the API
 *          if not supported.
 */
MMAL_STATUS_T mmal_vc_host_log(const char *msg);

/* For backwards compatibility in builds */
#define MMAL_VC_API_HAVE_HOST_LOG

/* VC DEBUG ONLY ************************************************************/
/** Consumes memory in the relocatable heap.
 *
 * The existing reserved memory is freed first then the new chunk is allocated.
 * If zero is specified for the size then the previously reserved memory
 * is freed and no allocation occurs.
 *
 * At startup no memory is reserved.
 *
 * @param size    Size of memory to consume in bytes.
 * @param handle  Set to the mem handle for the reserved memory or zero
 *                if no memory was allocated.
 * @return        MMAL_SUCCESS if memory was reserved (or size zero requested),
 *                MMAL_ENOSPC if the allocation failed or MMAL_ENOSYS if the
 *                API is not supported e.g in release mode VC images.
 * @internal
 */
MMAL_STATUS_T mmal_vc_consume_mem(size_t size, uint32_t *handle);

typedef enum
{
   MMAL_VC_COMPACT_NONE       = 0,
   MMAL_VC_COMPACT_NORMAL     = 1,
   MMAL_VC_COMPACT_DISCARD    = 2,
   MMAL_VC_COMPACT_AGGRESSIVE = 4,
   MMAL_VC_COMPACT_SHUFFLE    = 0x80,
   MMAL_VC_COMPACT_ALL        = MMAL_VC_COMPACT_NORMAL | MMAL_VC_COMPACT_DISCARD | MMAL_VC_COMPACT_AGGRESSIVE,
} MMAL_VC_COMPACT_MODE_T;

/** Trigger relocatable heap compaction.
 * @internal
 */
MMAL_STATUS_T mmal_vc_compact(MMAL_VC_COMPACT_MODE_T mode, uint32_t *duration);

/** Trigger LMK action from VC, for diagnostics.
 * @internal
 */
MMAL_STATUS_T mmal_vc_lmk(uint32_t alloc_size);

#ifdef __cplusplus
}
#endif
#endif
