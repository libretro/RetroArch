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

#include "mmal.h"
#include "util/mmal_util.h"
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "interface/vcos/vcos.h"
#include "mmal_logging.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/mmal_parameters.h"
#include <stdio.h>

#ifdef _VIDEOCORE
#include "vcfw/rtos/common/rtos_common_mem.h" /* mem_alloc */
#endif

/** Only collect port stats if enabled in build. Performance could be
 * affected on an ARM since gettimeofday() involves a system call.
 */
#if defined(MMAL_COLLECT_PORT_STATS)
# define MMAL_COLLECT_PORT_STATS_ENABLED 1
#else
# define MMAL_COLLECT_PORT_STATS_ENABLED 0
#endif

static MMAL_STATUS_T mmal_port_private_parameter_get(MMAL_PORT_T *port,
                                                     MMAL_PARAMETER_HEADER_T *param);

static MMAL_STATUS_T mmal_port_private_parameter_set(MMAL_PORT_T *port,
                                                     const MMAL_PARAMETER_HEADER_T *param);

/* Define this if you want to log all buffer transfers */
//#define ENABLE_MMAL_EXTRA_LOGGING

/** Definition of the core's private structure for a port. */
typedef struct MMAL_PORT_PRIVATE_CORE_T
{
   VCOS_MUTEX_T lock; /**< Used to lock access to the port */
   VCOS_MUTEX_T send_lock; /**< Used to lock access while sending buffer to the port */
   VCOS_MUTEX_T stats_lock; /**< Used to lock access to the stats */
   VCOS_MUTEX_T connection_lock; /**< Used to lock access to a connection */

   /** Callback set by client to call when buffer headers need to be returned */
   MMAL_PORT_BH_CB_T buffer_header_callback;

   /** Keeps track of the number of buffer headers currently in transit in this port */
   int32_t transit_buffer_headers;
   VCOS_MUTEX_T transit_lock;
   VCOS_SEMAPHORE_T transit_sema;

   /** Copy of the public port format pointer, to detect accidental overwrites */
   MMAL_ES_FORMAT_T* format_ptr_copy;

   /** Port to which this port is connected, or NULL if disconnected */
   MMAL_PORT_T* connected_port;

   MMAL_BOOL_T core_owns_connection; /**< Connection is handled by the core */

   /** Pool of buffers used between connected ports - output port only */
   MMAL_POOL_T* pool_for_connection;

   /** Indicates whether the port is paused or not. Buffers received on
    * a paused port will be queued instead of being sent to the component. */
   MMAL_BOOL_T is_paused;
   /** Queue for buffers received from the client when in paused state */
   MMAL_BUFFER_HEADER_T* queue_first;
   /** Queue for buffers received from the client when in paused state */
   MMAL_BUFFER_HEADER_T** queue_last;

   /** Per-port statistics collected directly by the MMAL core */
   MMAL_CORE_PORT_STATISTICS_T stats;

   char *name; /**< Port name */
   unsigned int name_size; /** Size of the memory area reserved for the name string */
} MMAL_PORT_PRIVATE_CORE_T;

/*****************************************************************************
 * Static declarations
 *****************************************************************************/
static MMAL_STATUS_T mmal_port_enable_internal(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb);
static MMAL_STATUS_T mmal_port_disable_internal(MMAL_PORT_T *port);

static MMAL_STATUS_T mmal_port_connection_enable(MMAL_PORT_T *port, MMAL_PORT_T *connected_port);
static MMAL_STATUS_T mmal_port_connection_disable(MMAL_PORT_T *port, MMAL_PORT_T *connected_port);
static MMAL_STATUS_T mmal_port_connection_start(MMAL_PORT_T *port, MMAL_PORT_T *connected_port);
static MMAL_STATUS_T mmal_port_populate_from_pool(MMAL_PORT_T* port, MMAL_POOL_T* pool);
static MMAL_STATUS_T mmal_port_populate_clock_ports(MMAL_PORT_T* output, MMAL_PORT_T* input, MMAL_POOL_T* pool);
static MMAL_STATUS_T mmal_port_connect_default(MMAL_PORT_T *port, MMAL_PORT_T *other_port);
static void mmal_port_connected_input_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void mmal_port_connected_output_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static MMAL_BOOL_T mmal_port_connected_pool_cb(MMAL_POOL_T *pool, MMAL_BUFFER_HEADER_T *buffer, void *userdata);

static void mmal_port_name_update(MMAL_PORT_T *port);
static void mmal_port_update_port_stats(MMAL_PORT_T *port, MMAL_CORE_STATS_DIR direction);

/*****************************************************************************/

/* Macros used to make the port API thread safe */
#define LOCK_PORT(a) vcos_mutex_lock(&(a)->priv->core->lock);
#define UNLOCK_PORT(a) vcos_mutex_unlock(&(a)->priv->core->lock);

/* Macros used to make the buffer sending / flushing thread safe */
#define LOCK_SENDING(a) vcos_mutex_lock(&(a)->priv->core->send_lock);
#define UNLOCK_SENDING(a) vcos_mutex_unlock(&(a)->priv->core->send_lock);

/* Macros used to make the port connection API thread safe */
#define LOCK_CONNECTION(a) vcos_mutex_lock(&(a)->priv->core->connection_lock);
#define UNLOCK_CONNECTION(a) vcos_mutex_unlock(&(a)->priv->core->connection_lock);

/* Macros used to make mmal_port_disable() blocking until all
 * the buffers have been sent back to the client */
#define IN_TRANSIT_INCREMENT(a) \
   vcos_mutex_lock(&(a)->priv->core->transit_lock); \
   if (!(a)->priv->core->transit_buffer_headers++) \
      vcos_semaphore_wait(&(a)->priv->core->transit_sema); \
   vcos_mutex_unlock(&(a)->priv->core->transit_lock)
#define IN_TRANSIT_DECREMENT(a) \
   vcos_mutex_lock(&(a)->priv->core->transit_lock); \
   if (!--(a)->priv->core->transit_buffer_headers) \
      vcos_semaphore_post(&(a)->priv->core->transit_sema); \
   vcos_mutex_unlock(&(a)->priv->core->transit_lock)
#define IN_TRANSIT_WAIT(a) \
   vcos_semaphore_wait(&(a)->priv->core->transit_sema); \
   vcos_semaphore_post(&(a)->priv->core->transit_sema)
#define IN_TRANSIT_COUNT(a) \
   (a)->priv->core->transit_buffer_headers

#define PORT_NAME_FORMAT "%s:%.2222s:%i%c%4.4s)"

/*****************************************************************************/

/** Allocate a port structure */
MMAL_PORT_T *mmal_port_alloc(MMAL_COMPONENT_T *component, MMAL_PORT_TYPE_T type, unsigned int extra_size)
{
   MMAL_PORT_T *port;
   MMAL_PORT_PRIVATE_CORE_T *core;
   unsigned int name_size = strlen(component->name) + sizeof(PORT_NAME_FORMAT);
   unsigned int size = sizeof(*port) + sizeof(MMAL_PORT_PRIVATE_T) +
      sizeof(MMAL_PORT_PRIVATE_CORE_T) + name_size + extra_size;
   MMAL_BOOL_T lock = 0, lock_send = 0, lock_transit = 0, sema_transit = 0;
   MMAL_BOOL_T lock_stats = 0, lock_connection = 0;

   LOG_TRACE("component:%s type:%u extra:%u", component->name, type, extra_size);

   port = vcos_calloc(1, size, "mmal port");
   if (!port)
   {
      LOG_ERROR("failed to allocate port, size %u", size);
      return 0;
   }
   port->type = type;

   port->priv = (MMAL_PORT_PRIVATE_T *)(port+1);
   port->priv->core = core = (MMAL_PORT_PRIVATE_CORE_T *)(port->priv+1);
   if (extra_size)
      port->priv->module = (struct MMAL_PORT_MODULE_T *)(port->priv->core+1);
   port->component = component;
   port->name = core->name = ((char *)(port->priv->core+1)) + extra_size;
   core->name_size = name_size;
   mmal_port_name_update(port);
   core->queue_last = &core->queue_first;

   port->priv->pf_connect = mmal_port_connect_default;

   lock = vcos_mutex_create(&port->priv->core->lock, "mmal port lock") == VCOS_SUCCESS;
   lock_send = vcos_mutex_create(&port->priv->core->send_lock, "mmal port send lock") == VCOS_SUCCESS;
   lock_transit = vcos_mutex_create(&port->priv->core->transit_lock, "mmal port transit lock") == VCOS_SUCCESS;
   sema_transit = vcos_semaphore_create(&port->priv->core->transit_sema, "mmal port transit sema", 1) == VCOS_SUCCESS;
   lock_stats = vcos_mutex_create(&port->priv->core->stats_lock, "mmal stats lock") == VCOS_SUCCESS;
   lock_connection = vcos_mutex_create(&port->priv->core->connection_lock, "mmal connection lock") == VCOS_SUCCESS;

   if (!lock || !lock_send || !lock_transit || !sema_transit || !lock_stats || !lock_connection)
   {
      LOG_ERROR("%s: failed to create sync objects (%u,%u,%u,%u,%u,%u)",
            port->name, lock, lock_send, lock_transit, sema_transit, lock_stats, lock_connection);
      goto error;
   }

   port->format = mmal_format_alloc();
   if (!port->format)
   {
      LOG_ERROR("%s: failed to allocate format object", port->name);
      goto error;
   }
   port->priv->core->format_ptr_copy = port->format;

   LOG_TRACE("%s: created at %p", port->name, port);
   return port;

 error:
   if (lock) vcos_mutex_delete(&port->priv->core->lock);
   if (lock_send) vcos_mutex_delete(&port->priv->core->send_lock);
   if (lock_transit) vcos_mutex_delete(&port->priv->core->transit_lock);
   if (sema_transit) vcos_semaphore_delete(&port->priv->core->transit_sema);
   if (lock_stats) vcos_mutex_delete(&port->priv->core->stats_lock);
   if (lock_connection) vcos_mutex_delete(&port->priv->core->connection_lock);
   if (port->format) mmal_format_free(port->format);
   vcos_free(port);
   return 0;
}

/** Free a port structure */
void mmal_port_free(MMAL_PORT_T *port)
{
   LOG_TRACE("%s at %p", port ? port->name : "<invalid>", port);

   if (!port)
      return;

   vcos_assert(port->format == port->priv->core->format_ptr_copy);
   mmal_format_free(port->priv->core->format_ptr_copy);
   vcos_mutex_delete(&port->priv->core->connection_lock);
   vcos_mutex_delete(&port->priv->core->stats_lock);
   vcos_semaphore_delete(&port->priv->core->transit_sema);
   vcos_mutex_delete(&port->priv->core->transit_lock);
   vcos_mutex_delete(&port->priv->core->send_lock);
   vcos_mutex_delete(&port->priv->core->lock);
   vcos_free(port);
}

/** Allocate an array of ports */
MMAL_PORT_T **mmal_ports_alloc(MMAL_COMPONENT_T *component, unsigned int ports_num,
   MMAL_PORT_TYPE_T type, unsigned int extra_size)
{
   MMAL_PORT_T **ports;
   unsigned int i;

   ports = vcos_calloc(1, sizeof(MMAL_PORT_T *) * ports_num, "mmal ports");
   if (!ports)
      return 0;

   for (i = 0; i < ports_num; i++)
   {
      ports[i] = mmal_port_alloc(component, type, extra_size);
      if (!ports[i])
         break;
      ports[i]->index = i;
      mmal_port_name_update(ports[i]);
   }

   if (i != ports_num)
   {
      for (ports_num = i, i = 0; i < ports_num; i++)
         mmal_port_free(ports[i]);
      vcos_free(ports);
      return 0;
   }

   return ports;
}

/** Free an array of ports */
void mmal_ports_free(MMAL_PORT_T **ports, unsigned int ports_num)
{
   unsigned int i;

   for (i = 0; i < ports_num; i++)
      mmal_port_free(ports[i]);
   vcos_free(ports);
}

/** Set format of a port */
MMAL_STATUS_T mmal_port_format_commit(MMAL_PORT_T *port)
{
   MMAL_STATUS_T status;
   char encoding_string[16];

   if (!port || !port->priv)
   {
      LOG_ERROR("invalid port (%p/%p)", port, port ? port->priv : NULL);
      return MMAL_EINVAL;
   }

   if (port->format != port->priv->core->format_ptr_copy)
   {
      LOG_ERROR("%s: port format has been overwritten, resetting %p to %p",
            port->name, port->format, port->priv->core->format_ptr_copy);
      port->format = port->priv->core->format_ptr_copy;
      return MMAL_EFAULT;
   }

   if (port->format->encoding == 0)
      snprintf(encoding_string, sizeof(encoding_string), "<NO-FORMAT>");
   else
      snprintf(encoding_string, sizeof(encoding_string), "%4.4s", (char*)&port->format->encoding);

   LOG_TRACE("%s(%i:%i) port %p format %i:%s",
             port->component->name, (int)port->type, (int)port->index, port,
             (int)port->format->type, encoding_string);

   if (!port->priv->pf_set_format)
   {
      LOG_ERROR("%s: no component implementation", port->name);
      return MMAL_ENOSYS;
   }

   LOCK_PORT(port);
   status = port->priv->pf_set_format(port);
   mmal_port_name_update(port);

   /* Make sure the buffer size / num are sensible */
   if (port->buffer_size < port->buffer_size_min)
      port->buffer_size = port->buffer_size_min;
   if (port->buffer_num < port->buffer_num_min)
      port->buffer_num = port->buffer_num_min;
   /* The output port settings might have changed */
   if (port->type == MMAL_PORT_TYPE_INPUT)
   {
      MMAL_PORT_T **ports = port->component->output;
      unsigned int i;

      for (i = 0; i < port->component->output_num; i++)
      {
         if (ports[i]->buffer_size < ports[i]->buffer_size_min)
            ports[i]->buffer_size = ports[i]->buffer_size_min;
         if (ports[i]->buffer_num < ports[i]->buffer_num_min)
            ports[i]->buffer_num = ports[i]->buffer_num_min;
      }
   }

   UNLOCK_PORT(port);
   return status;
}

/** Enable processing on a port */
MMAL_STATUS_T mmal_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_STATUS_T status;
   MMAL_PORT_T *connected_port;
   MMAL_PORT_PRIVATE_CORE_T *core;

   if (!port || !port->priv)
      return MMAL_EINVAL;

   LOG_TRACE("%s port %p, cb %p, buffers (%i/%i/%i,%i/%i/%i)",
             port->name, port, cb,
             (int)port->buffer_num, (int)port->buffer_num_recommended, (int)port->buffer_num_min,
             (int)port->buffer_size, (int)port->buffer_size_recommended, (int)port->buffer_size_min);

   if (!port->priv->pf_enable)
      return MMAL_ENOSYS;

   core = port->priv->core;
   LOCK_CONNECTION(port);
   connected_port = core->connected_port;

   /* Sanity checking */
   if (port->is_enabled)
   {
      UNLOCK_CONNECTION(port);
      LOG_ERROR("%s(%p) already enabled", port->name, port);
      return MMAL_EINVAL;
   }
   if (connected_port && cb) /* Callback must be NULL for connected ports */
   {
      UNLOCK_CONNECTION(port);
      LOG_ERROR("callback (%p) not allowed for connected port (%s)%p",
         cb, port->name, connected_port);
      return MMAL_EINVAL;
   }

   /* Start by preparing the port connection so that everything is ready for when
    * both ports are enabled */
   if (connected_port)
   {
      LOCK_CONNECTION(connected_port);
      status = mmal_port_connection_enable(port, connected_port);
      if (status != MMAL_SUCCESS)
      {
         UNLOCK_CONNECTION(connected_port);
         UNLOCK_CONNECTION(port);
         return status;
      }

      cb = connected_port->type == MMAL_PORT_TYPE_INPUT ?
         mmal_port_connected_output_cb : mmal_port_connected_input_cb;
   }

   /* Enable the input port of a connection first */
   if (connected_port && connected_port->type == MMAL_PORT_TYPE_INPUT)
   {
      status = mmal_port_enable_internal(connected_port, mmal_port_connected_input_cb);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to enable connected port (%s)%p (%s)", connected_port->name,
            connected_port, mmal_status_to_string(status));
         goto error;
      }
   }

   status = mmal_port_enable_internal(port, cb);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to enable port %s(%p) (%s)", port->name, port,
         mmal_status_to_string(status));
      goto error;
   }

   /* Enable the output port of a connection last */
   if (connected_port && connected_port->type != MMAL_PORT_TYPE_INPUT)
   {
      status = mmal_port_enable_internal(connected_port, mmal_port_connected_output_cb);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to enable connected port (%s)%p (%s)", connected_port->name,
            connected_port, mmal_status_to_string(status));
         goto error;
      }
   }

   /* Kick off the connection */
   if (connected_port && core->core_owns_connection)
   {
      status = mmal_port_connection_start(port, connected_port);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to start connection (%s)%p (%s)", port->name,
            port, mmal_status_to_string(status));
         goto error;
      }
   }

   if (connected_port)
      UNLOCK_CONNECTION(connected_port);
   UNLOCK_CONNECTION(port);
   return MMAL_SUCCESS;

error:
   if (connected_port && connected_port->is_enabled)
      mmal_port_disable_internal(connected_port);
   if (port->is_enabled)
      mmal_port_disable_internal(port);
   if (connected_port)
      mmal_port_connection_disable(port, connected_port);

   if (connected_port)
      UNLOCK_CONNECTION(connected_port);
   UNLOCK_CONNECTION(port);
   return status;
}

static MMAL_STATUS_T mmal_port_enable_internal(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PORT_PRIVATE_CORE_T* core = port->priv->core;
   MMAL_STATUS_T status = MMAL_SUCCESS;

   LOCK_PORT(port);

   if (port->is_enabled)
      goto end;

   /* Sanity check the buffer requirements */
   if (port->buffer_num < port->buffer_num_min)
   {
      LOG_ERROR("buffer_num too small (%i/%i)", (int)port->buffer_num, (int)port->buffer_num_min);
      status = MMAL_EINVAL;
      goto end;
   }
   if (port->buffer_size < port->buffer_size_min)
   {
      LOG_ERROR("buffer_size too small (%i/%i)", (int)port->buffer_size, (int)port->buffer_size_min);
      status = MMAL_EINVAL;
      goto end;
   }

   core->buffer_header_callback = cb;
   status = port->priv->pf_enable(port, cb);
   if (status != MMAL_SUCCESS)
      goto end;

   LOCK_SENDING(port);
   port->is_enabled = 1;
   UNLOCK_SENDING(port);

end:
   UNLOCK_PORT(port);
   return status;
}

static MMAL_STATUS_T mmal_port_connection_enable(MMAL_PORT_T *port, MMAL_PORT_T *connected_port)
{
   MMAL_PORT_T *output = port->type == MMAL_PORT_TYPE_OUTPUT ? port : connected_port;
   MMAL_PORT_T *input = connected_port->type == MMAL_PORT_TYPE_INPUT ? connected_port : port;
   MMAL_PORT_T *pool_port = (output->capabilities & MMAL_PORT_CAPABILITY_ALLOCATION) ? output : input;
   MMAL_PORT_PRIVATE_CORE_T *pool_core = pool_port->priv->core;
   uint32_t buffer_size, buffer_num;
   MMAL_POOL_T *pool;

   /* At this point both ports hold the connection lock */

   /* Ensure that the buffer numbers and sizes used are the maxima between connected ports. */
   buffer_num  = MMAL_MAX(port->buffer_num,  connected_port->buffer_num);
   buffer_size = MMAL_MAX(port->buffer_size, connected_port->buffer_size);
   port->buffer_num  = connected_port->buffer_num  = buffer_num;
   port->buffer_size = connected_port->buffer_size = buffer_size;

   if (output->capabilities & MMAL_PORT_CAPABILITY_PASSTHROUGH)
      buffer_size = 0;

   if (!port->priv->core->core_owns_connection)
      return MMAL_SUCCESS;

   pool = mmal_port_pool_create(pool_port, buffer_num, buffer_size);
   if (!pool)
   {
      LOG_ERROR("failed to create pool for connection");
      return MMAL_ENOMEM;
   }

   pool_core->pool_for_connection = pool;
   mmal_pool_callback_set(pool, mmal_port_connected_pool_cb, output);
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_connection_disable(MMAL_PORT_T *port, MMAL_PORT_T *connected_port)
{
   MMAL_POOL_T *pool = port->priv->core->pool_for_connection ?
      port->priv->core->pool_for_connection : connected_port->priv->core->pool_for_connection;

   mmal_pool_destroy(pool);
   port->priv->core->pool_for_connection =
      connected_port->priv->core->pool_for_connection = NULL;
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_port_connection_start(MMAL_PORT_T *port, MMAL_PORT_T *connected_port)
{
   MMAL_PORT_T *output = port->type == MMAL_PORT_TYPE_OUTPUT ? port : connected_port;
   MMAL_PORT_T *input = connected_port->type == MMAL_PORT_TYPE_INPUT ? connected_port : port;
   MMAL_POOL_T *pool = port->priv->core->pool_for_connection ?
      port->priv->core->pool_for_connection : connected_port->priv->core->pool_for_connection;
   MMAL_STATUS_T status;

   if (output->type == MMAL_PORT_TYPE_CLOCK && input->type == MMAL_PORT_TYPE_CLOCK)
   {
      /* Clock ports need buffers to send clock updates, so
       * populate both clock ports */
      status = mmal_port_populate_clock_ports(output, input, pool);
   }
   else
   {
      /* Put the buffers into the output port */
      status = mmal_port_populate_from_pool(output, pool);
   }

   return status;
}

/** Disable processing on a port */
MMAL_STATUS_T mmal_port_disable(MMAL_PORT_T *port)
{
   MMAL_STATUS_T status;
   MMAL_PORT_T *connected_port;
   MMAL_PORT_PRIVATE_CORE_T *core;

   if (!port || !port->priv)
      return MMAL_EINVAL;

   LOG_TRACE("%s(%i:%i) port %p", port->component->name,
             (int)port->type, (int)port->index, port);

   if (!port->priv->pf_disable)
      return MMAL_ENOSYS;

   core = port->priv->core;
   LOCK_CONNECTION(port);
   connected_port = core->connected_port;

   /* Sanity checking */
   if (!port->is_enabled)
   {
      UNLOCK_CONNECTION(port);
      LOG_ERROR("port %s(%p) is not enabled", port->name, port);
      return MMAL_EINVAL;
   }

   if (connected_port)
      LOCK_CONNECTION(connected_port);

   /* Disable the output port of a connection first */
   if (connected_port && connected_port->type != MMAL_PORT_TYPE_INPUT)
   {
      status = mmal_port_disable_internal(connected_port);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to disable connected port (%s)%p (%s)", connected_port->name,
            connected_port, mmal_status_to_string(status));
         goto end;
      }
   }

   status = mmal_port_disable_internal(port);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to disable port (%s)%p", port->name, port);
      goto end;
   }

   /* Disable the input port of a connection last */
   if (connected_port && connected_port->type == MMAL_PORT_TYPE_INPUT)
   {
      status = mmal_port_disable_internal(connected_port);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to disable connected port (%s)%p (%s)", connected_port->name,
            connected_port, mmal_status_to_string(status));
         goto end;
      }
   }

   if (connected_port)
   {
      status = mmal_port_connection_disable(port, connected_port);
      if (status != MMAL_SUCCESS)
         LOG_ERROR("failed to disable connection (%s)%p (%s)", port->name,
            port, mmal_status_to_string(status));
   }

end:
   if (connected_port)
      UNLOCK_CONNECTION(connected_port);
   UNLOCK_CONNECTION(port);

   return status;
}

static MMAL_STATUS_T mmal_port_disable_internal(MMAL_PORT_T *port)
{
   MMAL_PORT_PRIVATE_CORE_T* core = port->priv->core;
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_BUFFER_HEADER_T *buffer;

   LOCK_PORT(port);

   if (!port->is_enabled)
      goto end;

   LOCK_SENDING(port);
   port->is_enabled = 0;
   UNLOCK_SENDING(port);

   mmal_component_action_lock(port->component);

   if (core->pool_for_connection)
      mmal_pool_callback_set(core->pool_for_connection, NULL, NULL);

   status = port->priv->pf_disable(port);

   mmal_component_action_unlock(port->component);

   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("port %p could not be disabled (%s)", port->name, mmal_status_to_string(status));
      LOCK_SENDING(port);
      port->is_enabled = 1;
      UNLOCK_SENDING(port);
      goto end;
   }

   /* Flush our internal queue */
   buffer = port->priv->core->queue_first;
   while (buffer)
   {
      MMAL_BUFFER_HEADER_T *next = buffer->next;
      mmal_port_buffer_header_callback(port, buffer);
      buffer = next;
   }
   port->priv->core->queue_first = 0;
   port->priv->core->queue_last = &port->priv->core->queue_first;

   /* Wait for all the buffers to have come back from the component */
   LOG_DEBUG("%s waiting for %i buffers left in transit", port->name, (int)IN_TRANSIT_COUNT(port));
   IN_TRANSIT_WAIT(port);
   LOG_DEBUG("%s has no buffers left in transit", port->name);

   port->priv->core->buffer_header_callback = NULL;

 end:
   UNLOCK_PORT(port);
   return status;
}

/** Send a buffer header to a port */
MMAL_STATUS_T mmal_port_send_buffer(MMAL_PORT_T *port,
   MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;

   if (!port || !port->priv)
   {
      LOG_ERROR("invalid port");
      return MMAL_EINVAL;
   }

#ifdef ENABLE_MMAL_EXTRA_LOGGING
   LOG_TRACE("%s(%i:%i) port %p, buffer %p (%p,%i,%i)",
             port->component->name, (int)port->type, (int)port->index, port, buffer,
             buffer ? buffer->data: 0, buffer ? (int)buffer->offset : 0,
             buffer ? (int)buffer->length : 0);
#endif

   if (buffer->alloc_size && !buffer->data &&
       !(port->capabilities & MMAL_PORT_CAPABILITY_PASSTHROUGH))
   {
      LOG_ERROR("%s(%p) received invalid buffer header", port->name, port);
      return MMAL_EINVAL;
   }

   if (!port->priv->pf_send)
      return MMAL_ENOSYS;

   LOCK_SENDING(port);

   if (!port->is_enabled)
   {
      UNLOCK_SENDING(port);
      return MMAL_EINVAL;
   }

   if (port->type == MMAL_PORT_TYPE_OUTPUT && buffer->length)
   {
      LOG_DEBUG("given an output buffer with length != 0");
      buffer->length = 0;
   }

   /* coverity[lock] transit_sema is used for signalling, and is not a lock */
   /* coverity[lock_order] since transit_sema is not a lock, there is no ordering conflict */
   IN_TRANSIT_INCREMENT(port);

   if (port->priv->core->is_paused)
   {
      /* Add buffer to our internal queue */
      buffer->next = NULL;
      *port->priv->core->queue_last = buffer;
      port->priv->core->queue_last = &buffer->next;
   }
   else
   {
      /* Send buffer to component */
      status = port->priv->pf_send(port, buffer);
   }

   if (status != MMAL_SUCCESS)
   {
      IN_TRANSIT_DECREMENT(port);
      LOG_ERROR("%s: send failed: %s", port->name, mmal_status_to_string(status));
   }
   else
   {
      mmal_port_update_port_stats(port, MMAL_CORE_STATS_RX);
   }

   UNLOCK_SENDING(port);
   return status;
}

/** Flush a port */
MMAL_STATUS_T mmal_port_flush(MMAL_PORT_T *port)
{
   MMAL_BUFFER_HEADER_T *buffer = 0;
   MMAL_STATUS_T status;

   if (!port || !port->priv)
      return MMAL_EINVAL;

   LOG_TRACE("%s(%i:%i) port %p", port->component->name,
             (int)port->type, (int)port->index, port);

   if (!port->priv->pf_flush)
      return MMAL_ENOSYS;

   /* N.B. must take action lock *before* sending lock */
   mmal_component_action_lock(port->component);
   LOCK_SENDING(port);

   if (!port->is_enabled)
   {
      UNLOCK_SENDING(port);
      mmal_component_action_unlock(port->component);
      return MMAL_SUCCESS;
   }

   status = port->priv->pf_flush(port);
   if (status == MMAL_SUCCESS)
   {
      /* Flush our internal queue */
      buffer = port->priv->core->queue_first;
      port->priv->core->queue_first = 0;
      port->priv->core->queue_last = &port->priv->core->queue_first;
   }

   UNLOCK_SENDING(port);
   mmal_component_action_unlock(port->component);

   while (buffer)
   {
      MMAL_BUFFER_HEADER_T *next = buffer->next;
      mmal_port_buffer_header_callback(port, buffer);
      buffer = next;
   }
   return status;
}

/* Set a parameter on a port. */
MMAL_STATUS_T mmal_port_parameter_set(MMAL_PORT_T *port,
   const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_STATUS_T status = MMAL_ENOSYS;

   if (!port)
   {
      LOG_ERROR("no port");
      return MMAL_EINVAL;
   }
   if (!param)
   {
      LOG_ERROR("param not supplied");
      return MMAL_EINVAL;
   }
   if (!port->priv)
   {
      LOG_ERROR("port not configured");
      return MMAL_EINVAL;
   }

   LOG_TRACE("%s(%i:%i) port %p, param %p (%x,%i)", port->component->name,
             (int)port->type, (int)port->index, port,
             param, param ? param->id : 0, param ? (int)param->size : 0);

   LOCK_PORT(port);
   if (port->priv->pf_parameter_set)
      status = port->priv->pf_parameter_set(port, param);
   if (status == MMAL_ENOSYS)
   {
      /* is this a core parameter? */
      status = mmal_port_private_parameter_set(port, param);
   }
   UNLOCK_PORT(port);
   return status;
}

/* Get a port parameter */
MMAL_STATUS_T mmal_port_parameter_get(MMAL_PORT_T *port,
   MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_STATUS_T status = MMAL_ENOSYS;

   if (!port || !port->priv)
      return MMAL_EINVAL;

   LOG_TRACE("%s(%i:%i) port %p, param %p (%x,%i)", port->component->name,
             (int)port->type, (int)port->index, port,
             param, param ? param->id : 0, param ? (int)param->size : 0);

   if (!param)
      return MMAL_EINVAL;

   LOCK_PORT(port);
   if (port->priv->pf_parameter_get)
      status = port->priv->pf_parameter_get(port, param);
   if (status == MMAL_ENOSYS)
   {
      /* is this a core parameter? */
      status = mmal_port_private_parameter_get(port, param);
   }

   UNLOCK_PORT(port);
   return status;
}

/** Buffer header callback. */
void mmal_port_buffer_header_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
#ifdef ENABLE_MMAL_EXTRA_LOGGING
   LOG_TRACE("%s(%i:%i) port %p, buffer %p (%i,%p,%i,%i)",
             port->component->name, (int)port->type, (int)port->index, port, buffer,
             buffer ? (int)buffer->cmd : 0, buffer ? buffer->data : 0,
             buffer ? (int)buffer->offset : 0, buffer ? (int)buffer->length : 0);
#endif

   if (!vcos_verify(IN_TRANSIT_COUNT(port) >= 0))
      LOG_ERROR("%s: buffer headers in transit < 0 (%d)", port->name, (int)IN_TRANSIT_COUNT(port));

   if (MMAL_COLLECT_PORT_STATS_ENABLED)
   {
      mmal_port_update_port_stats(port, MMAL_CORE_STATS_TX);
   }

   port->priv->core->buffer_header_callback(port, buffer);

   IN_TRANSIT_DECREMENT(port);
}

/** Event callback */
void mmal_port_event_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   if (port->priv->core->buffer_header_callback)
   {
      port->priv->core->buffer_header_callback(port, buffer);
   }
   else
   {
      LOG_ERROR("event lost on port %i,%i (buffer header callback not defined)",
                (int)port->type, (int)port->index);
      mmal_buffer_header_release(buffer);
   }
}

/** Connect an output port to an input port. */
MMAL_STATUS_T mmal_port_connect(MMAL_PORT_T *port, MMAL_PORT_T *other_port)
{
   MMAL_PORT_PRIVATE_CORE_T* core;
   MMAL_PORT_PRIVATE_CORE_T* other_core;
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PORT_T* output_port = NULL;

   if (!port || !port->priv || !other_port || !other_port->priv)
   {
      LOG_ERROR("invalid port");
      return MMAL_EINVAL;
   }

   if ((port->type == MMAL_PORT_TYPE_CLOCK) && (port->type != other_port->type))
   {
      LOG_ERROR("invalid port connection");
      return MMAL_EINVAL;
   }

   LOG_TRACE("connecting %s(%p) to %s(%p)", port->name, port, other_port->name, other_port);

   if (!port->priv->pf_connect || !other_port->priv->pf_connect)
   {
      LOG_ERROR("at least one pf_connect is NULL");
      return MMAL_ENOSYS;
   }

   core = port->priv->core;
   other_core = other_port->priv->core;

   LOCK_CONNECTION(port);
   if (core->connected_port)
   {
      LOG_ERROR("port %p is already connected", port);
      UNLOCK_CONNECTION(port);
      return MMAL_EISCONN;
   }
   if (port->is_enabled)
   {
      LOG_ERROR("port %p should not be enabled", port);
      UNLOCK_CONNECTION(port);
      return MMAL_EINVAL;
   }

   LOCK_CONNECTION(other_port);
   if (other_core->connected_port)
   {
      LOG_ERROR("port %p is already connected", other_port);
      status = MMAL_EISCONN;
      goto finish;
   }
   if (other_port->is_enabled)
   {
      LOG_ERROR("port %p should not be enabled", other_port);
      status = MMAL_EINVAL;
      goto finish;
   }

   core->connected_port = other_port;
   other_core->connected_port = port;

   core->core_owns_connection = 0;
   other_core->core_owns_connection = 0;

   /* Check to see if the port will manage the connection on its own. If not then the core
    * will manage it. */
   output_port = port->type == MMAL_PORT_TYPE_OUTPUT ? port : other_port;
   if (output_port->priv->pf_connect(port, other_port) == MMAL_SUCCESS)
      goto finish;

   core->core_owns_connection = 1;
   other_core->core_owns_connection = 1;

finish:
   UNLOCK_CONNECTION(other_port);
   UNLOCK_CONNECTION(port);
   return status;
}

/** Disconnect a connected port. */
MMAL_STATUS_T mmal_port_disconnect(MMAL_PORT_T *port)
{
   MMAL_PORT_PRIVATE_CORE_T* core;
   MMAL_PORT_T* other_port;
   MMAL_STATUS_T status = MMAL_SUCCESS;

   if (!port || !port->priv)
   {
      LOG_ERROR("invalid port");
      return MMAL_EINVAL;
   }

   LOG_TRACE("%s(%p)", port->name, port);

   LOCK_CONNECTION(port);

   core = port->priv->core;
   other_port = core->connected_port;

   if (!other_port)
   {
      UNLOCK_CONNECTION(port);
      LOG_DEBUG("%s(%p) is not connected", port->name, port);
      return MMAL_ENOTCONN;
   }

   LOCK_CONNECTION(other_port);

   /* Make sure the connection is disabled first */
   if (port->is_enabled)
   {
      MMAL_PORT_T *output = port->type == MMAL_PORT_TYPE_OUTPUT ? port : other_port;
      MMAL_PORT_T *input = other_port->type == MMAL_PORT_TYPE_INPUT ? other_port : port;

      status = mmal_port_disable_internal(output);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to disable port (%s)%p", port->name, port);
         goto end;
      }
      status = mmal_port_disable_internal(input);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to disable port (%s)%p", port->name, port);
         goto end;
      }
      status = mmal_port_connection_disable(port, other_port);
   }

   if (!core->core_owns_connection)
   {
      status = port->priv->pf_connect(port, NULL);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("disconnection of %s(%p) failed (%i)", port->name, port, status);
         goto end;
      }
   }

   core->connected_port = NULL;
   other_port->priv->core->connected_port = NULL;

end:
   UNLOCK_CONNECTION(other_port);
   UNLOCK_CONNECTION(port);
   return status;
}

/** Allocate a payload buffer */
uint8_t *mmal_port_payload_alloc(MMAL_PORT_T *port, uint32_t payload_size)
{
   uint8_t *mem;

   if (!port || !port->priv)
      return NULL;

   LOG_TRACE("%s(%i:%i) port %p, size %i", port->component->name,
             (int)port->type, (int)port->index, port, (int)payload_size);

   if (!payload_size)
      return NULL;

   /* TODO: keep track of the allocs so we can free them when the component is destroyed */

   if (!port->priv->pf_payload_alloc)
   {
      /* Revert to using the heap */
#ifdef _VIDEOCORE
      mem = (void *)mem_alloc(payload_size, 32, MEM_FLAG_DIRECT, port->name);
#else
      mem = vcos_malloc(payload_size, "mmal payload");
#endif
      goto end;
   }

   LOCK_PORT(port);
   mem = port->priv->pf_payload_alloc(port, payload_size);
   UNLOCK_PORT(port);

 end:
   /* Acquire the port if the allocation was successful.
    * This will ensure that the component is not destroyed until the payload has been freed. */
   if (mem)
      mmal_port_acquire(port);
   return mem;
}

/** Free a payload buffer */
void mmal_port_payload_free(MMAL_PORT_T *port, uint8_t *payload)
{
   if (!port || !port->priv)
      return;

   LOG_TRACE("%s(%i:%i) port %p, payload %p", port->component->name,
             (int)port->type, (int)port->index, port, payload);

   if (!port->priv->pf_payload_alloc)
   {
      /* Revert to using the heap */
#ifdef _VIDEOCORE
      mem_release((MEM_HANDLE_T)payload);
#else
      vcos_free(payload);
#endif
      mmal_port_release(port);
      return;
   }

   LOCK_PORT(port);
   port->priv->pf_payload_free(port, payload);
   UNLOCK_PORT(port);
   mmal_port_release(port);
}

MMAL_STATUS_T mmal_port_event_get(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T **buffer, uint32_t event)
{
   if (!port || !port->priv || !buffer)
      return MMAL_EINVAL;

   LOG_TRACE("%s(%i:%i) port %p, event %4.4s", port->component->name,
             (int)port->type, (int)port->index, port, (char *)&event);

   /* Get an event buffer from our event pool */
   *buffer = mmal_queue_get(port->component->priv->event_pool->queue);
   if (!*buffer)
   {
      LOG_ERROR("%s(%i:%i) port %p, no event buffer left for %4.4s", port->component->name,
                (int)port->type, (int)port->index, port, (char *)&event);
      return MMAL_ENOSPC;
   }

   (*buffer)->cmd = event;
   (*buffer)->length = 0;

   /* Special case for the FORMAT_CHANGED event. We need to properly initialise the event
    * buffer so that it contains an initialised MMAL_ES_FORMAT_T structure. */
   if (event == MMAL_EVENT_FORMAT_CHANGED)
   {
      uint32_t size = sizeof(MMAL_EVENT_FORMAT_CHANGED_T);
      size += sizeof(MMAL_ES_FORMAT_T) + sizeof(MMAL_ES_SPECIFIC_FORMAT_T);

      if ((*buffer)->alloc_size < size)
      {
         LOG_ERROR("%s(%i:%i) port %p, event buffer for %4.4s is too small (%i/%i)",
                   port->component->name, (int)port->type, (int)port->index, port,
                   (char *)&event, (int)(*buffer)->alloc_size, (int)size);
         goto error;
      }

      memset((*buffer)->data, 0, size);
      (*buffer)->length = size;
   }

   return MMAL_SUCCESS;

error:
   if (*buffer)
      mmal_buffer_header_release(*buffer);
   *buffer = NULL;
   return MMAL_ENOSPC;
}

/** Populate clock ports from the given pool */
static MMAL_STATUS_T mmal_port_populate_clock_ports(MMAL_PORT_T* output, MMAL_PORT_T* input, MMAL_POOL_T* pool)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_BUFFER_HEADER_T *buffer;

   if (!output->priv->pf_send || !input->priv->pf_send)
      return MMAL_ENOSYS;

   LOG_TRACE("output %s %p, input %s %p, pool: %p", output->name, output, input->name, input, pool);

   buffer = mmal_queue_get(pool->queue);
   while (buffer)
   {
      status = mmal_port_send_buffer(output, buffer);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to send buffer to clock port %s", output->name);
         mmal_buffer_header_release(buffer);
         break;
      }

      buffer = mmal_queue_get(pool->queue);
      if (buffer)
      {
         status = mmal_port_send_buffer(input, buffer);
         if (status != MMAL_SUCCESS)
         {
            LOG_ERROR("failed to send buffer to clock port %s", output->name);
            mmal_buffer_header_release(buffer);
            break;
         }
         buffer = mmal_queue_get(pool->queue);
      }
   }

   return status;
}

/** Populate an output port with a pool of buffers */
static MMAL_STATUS_T mmal_port_populate_from_pool(MMAL_PORT_T* port, MMAL_POOL_T* pool)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   uint32_t buffer_idx;
   MMAL_BUFFER_HEADER_T *buffer;

   if (!port->priv->pf_send)
      return MMAL_ENOSYS;

   LOG_TRACE("%s port %p, pool: %p", port->name, port, pool);

   /* Populate port from pool */
   for (buffer_idx = 0; buffer_idx < port->buffer_num; buffer_idx++)
   {
      buffer = mmal_queue_get(pool->queue);
      if (!buffer)
      {
         LOG_ERROR("too few buffers in the pool");
         status = MMAL_ENOMEM;
         break;
      }

      status = mmal_port_send_buffer(port, buffer);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("failed to send buffer to port");
         mmal_buffer_header_release(buffer);
         break;
      }
   }

   return status;
}

/** Default behaviour when setting up or tearing down a connection to another port */
static MMAL_STATUS_T mmal_port_connect_default(MMAL_PORT_T *port, MMAL_PORT_T *other_port)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(other_port);

   LOG_TRACE("port %p, other_port %p", port, other_port);
   return MMAL_ENOSYS;
}

/** Connected input port buffer callback */
static void mmal_port_connected_input_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   LOG_TRACE("buffer %p from connected input port %p: data %p, alloc_size %u, length %u",
             buffer, port, buffer->data, buffer->alloc_size, buffer->length);

   /* Clock ports are bi-directional and a buffer coming from an "input"
    * clock port can potentially have valid payload data, in which case
    * it should be sent directly to the connected port. */
   if (port->type == MMAL_PORT_TYPE_CLOCK && buffer->length)
   {
      MMAL_PORT_T* connected_port = port->priv->core->connected_port;
      mmal_port_send_buffer(connected_port, buffer);
      return;
   }

   /* Simply release buffer back into pool for re-use */
   mmal_buffer_header_release(buffer);
}

/** Connected output port buffer callback */
static void mmal_port_connected_output_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_PORT_T* connected_port = port->priv->core->connected_port;
   MMAL_STATUS_T status;

   LOG_TRACE("buffer %p from connected output port %p: data %p, alloc_size %u, length %u",
             buffer, port, buffer->data, buffer->alloc_size, buffer->length);

   if (buffer->cmd)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event = mmal_event_format_changed_get(buffer);

      /* Handle format changed events */
      if (event)
      {
         /* Apply the change */
         status = mmal_format_full_copy(port->format, event->format);
         if (status == MMAL_SUCCESS)
            status = mmal_port_format_commit(port);
         if (status != MMAL_SUCCESS)
            LOG_ERROR("format commit failed on port %s (%i)", port->name, status);

         /* Forward to the connected port */
         if (status == MMAL_SUCCESS)
            status = mmal_port_send_buffer(connected_port, buffer);

         if (status != MMAL_SUCCESS)
         {
            mmal_event_error_send(port->component, status);
            mmal_buffer_header_release(buffer);
         }
         return; /* Event handled */
      }

      /* FIXME Release other event buffers for now, until we can deal with shared memory issues */
      mmal_buffer_header_release(buffer);
   }
   else
   {
      if (port->is_enabled)
      {
         /* Forward data buffers to the connected input port */
         status = mmal_port_send_buffer(connected_port, buffer);
         if (status != MMAL_SUCCESS)
         {
            LOG_ERROR("%s could not send buffer on port %s (%s)",
                      port->name, connected_port->name, mmal_status_to_string(status));
            mmal_buffer_header_release(buffer);
         }
      }
      else
      {
         /* This port is disabled. Buffer will be a flushed buffer, so
          * return to the pool rather than delivering it.
          */
         mmal_buffer_header_release(buffer);
      }
   }
}

/** Callback for when a buffer from a connected input port is finally released */
static MMAL_BOOL_T mmal_port_connected_pool_cb(MMAL_POOL_T *pool, MMAL_BUFFER_HEADER_T *buffer, void *userdata)
{
   MMAL_PORT_T* port = (MMAL_PORT_T*)userdata;
   MMAL_STATUS_T status;
   MMAL_PARAM_UNUSED(pool);

   LOG_TRACE("released buffer %p, data %p alloc_size %u length %u",
             buffer, buffer->data, buffer->alloc_size, buffer->length);

   /* Pipe the buffer back to the output port */
   status = mmal_port_send_buffer(port, buffer);

   /* Put the buffer back in the pool if we were successful */
   return status != MMAL_SUCCESS;
}

/*****************************************************************************/
static void mmal_port_name_update(MMAL_PORT_T *port)
{
   MMAL_PORT_PRIVATE_CORE_T* core = port->priv->core;

   vcos_snprintf(core->name, core->name_size - 1, PORT_NAME_FORMAT,
            port->component->name, mmal_port_type_to_string(port->type), (int)port->index,
            port->format && port->format->encoding ? '(' : '\0',
            port->format && port->format->encoding ? (char *)&port->format->encoding : "");
}

static MMAL_STATUS_T mmal_port_get_core_stats(MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PARAMETER_CORE_STATISTICS_T *stats_param = (MMAL_PARAMETER_CORE_STATISTICS_T*)param;
   MMAL_CORE_STATISTICS_T *stats = &stats_param->stats;
   MMAL_CORE_STATISTICS_T *src_stats;
   MMAL_PORT_PRIVATE_CORE_T *core = port->priv->core;
   vcos_mutex_lock(&core->stats_lock);
   switch (stats_param->dir)
   {
   case MMAL_CORE_STATS_RX:
      src_stats = &port->priv->core->stats.rx;
      break;
   default:
      src_stats = &port->priv->core->stats.tx;
      break;
   }
   *stats = *src_stats;
   if (stats_param->reset)
      memset(src_stats, 0, sizeof(*src_stats));
   vcos_mutex_unlock(&core->stats_lock);
   return MMAL_SUCCESS;
}

/** Update the port stats, called per buffer.
 *
 */
static void mmal_port_update_port_stats(MMAL_PORT_T *port, MMAL_CORE_STATS_DIR direction)
{
   MMAL_PORT_PRIVATE_CORE_T *core = port->priv->core;
   MMAL_CORE_STATISTICS_T *stats;
   unsigned stc = vcos_getmicrosecs();

   vcos_mutex_lock(&core->stats_lock);

   stats = direction == MMAL_CORE_STATS_RX ? &core->stats.rx : &core->stats.tx;

   stats->buffer_count++;

   if (!stats->first_buffer_time)
   {
      stats->last_buffer_time = stats->first_buffer_time = stc;
   }
   else
   {
      stats->max_delay = vcos_max(stats->max_delay, stc-stats->last_buffer_time);
      stats->last_buffer_time = stc;
   }
   vcos_mutex_unlock(&core->stats_lock);
}

static MMAL_STATUS_T mmal_port_private_parameter_get(MMAL_PORT_T *port,
                                                     MMAL_PARAMETER_HEADER_T *param)
{
   switch (param->id)
   {
   case MMAL_PARAMETER_CORE_STATISTICS:
      return mmal_port_get_core_stats(port, param);
   default:
      return MMAL_ENOSYS;
   }
}

static MMAL_STATUS_T mmal_port_private_parameter_set(MMAL_PORT_T *port,
                                                     const MMAL_PARAMETER_HEADER_T *param)
{
   (void)port;
   switch (param->id)
   {
   default:
      return MMAL_ENOSYS;
   }
}

MMAL_STATUS_T mmal_port_pause(MMAL_PORT_T *port, MMAL_BOOL_T pause)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;

   LOCK_SENDING(port);

   /* When resuming from pause, we send all our queued buffers to the port */
   if (!pause && port->is_enabled)
   {
      MMAL_BUFFER_HEADER_T *buffer = port->priv->core->queue_first;
      while (buffer)
      {
         MMAL_BUFFER_HEADER_T *next = buffer->next;
         status = port->priv->pf_send(port, buffer);
         if (status != MMAL_SUCCESS)
         {
            buffer->next = next;
            break;
         }
         buffer = next;
      }

      /* If for some reason we could not send one of the buffers, we just
       * leave all the buffers in our internal queue and return an error. */
      if (status != MMAL_SUCCESS)
      {
         port->priv->core->queue_first = buffer;
      }
      else
      {
         port->priv->core->queue_first = 0;
         port->priv->core->queue_last = &port->priv->core->queue_first;
      }
   }

   if (status == MMAL_SUCCESS)
      port->priv->core->is_paused = pause;

   UNLOCK_SENDING(port);
   return status;
}

MMAL_BOOL_T mmal_port_is_connected(MMAL_PORT_T *port)
{
   return !!port->priv->core->connected_port;
}
