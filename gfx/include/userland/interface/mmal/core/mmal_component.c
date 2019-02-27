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
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "core/mmal_core_private.h"
#include "mmal_logging.h"

/* Minimum number of buffers that will be available on the control port */
#define MMAL_CONTROL_PORT_BUFFERS_MIN 4

/** Definition of the core private context. */
typedef struct
{
   MMAL_COMPONENT_PRIVATE_T private;

   /** Action registered by component and run when buffers are received by any of the ports */
   void (*pf_action)(MMAL_COMPONENT_T *component);

   /** Action thread */
   VCOS_THREAD_T action_thread;
   VCOS_EVENT_T action_event;
   VCOS_MUTEX_T action_mutex;
   MMAL_BOOL_T action_quit;

   VCOS_MUTEX_T lock; /**< Used to lock access to the component */
   MMAL_BOOL_T destruction_pending;

} MMAL_COMPONENT_CORE_PRIVATE_T;

/*****************************************************************************/
static void mmal_core_init(void);
static void mmal_core_deinit(void);

static MMAL_STATUS_T mmal_component_supplier_create(const char *name, MMAL_COMPONENT_T *component);
static void mmal_component_init_control_port(MMAL_PORT_T *port);

static MMAL_STATUS_T mmal_component_destroy_internal(MMAL_COMPONENT_T *component);
static MMAL_STATUS_T mmal_component_release_internal(MMAL_COMPONENT_T *component);

/*****************************************************************************/
static VCOS_MUTEX_T mmal_core_lock;
/** Used to generate a unique id for each MMAL component in this context.    */
static unsigned int mmal_core_instance_count;
static unsigned int mmal_core_refcount;
/*****************************************************************************/

/** Create an instance of a component */
static MMAL_STATUS_T mmal_component_create_core(const char *name,
   MMAL_STATUS_T (*constructor)(const char *name, MMAL_COMPONENT_T *),
   struct MMAL_COMPONENT_MODULE_T *constructor_private,
   MMAL_COMPONENT_T **component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   unsigned int size = sizeof(MMAL_COMPONENT_T) + sizeof(MMAL_COMPONENT_CORE_PRIVATE_T);
   unsigned int i, name_length = strlen(name) + 1;
   unsigned int port_index;
   char *component_name;

   if(!component)
      return MMAL_EINVAL;

   mmal_core_init();

   *component = vcos_calloc(1, size + name_length, "mmal component");
   if(!*component)
      return MMAL_ENOMEM;

   private = (MMAL_COMPONENT_CORE_PRIVATE_T *)&(*component)[1];
   (*component)->priv = (MMAL_COMPONENT_PRIVATE_T *)private;
   (*component)->name = component_name= (char *)&((MMAL_COMPONENT_CORE_PRIVATE_T *)(*component)->priv)[1];
   memcpy(component_name, name, name_length);
   /* coverity[missing_lock] Component and mutex have just been created. No need to lock yet */
   (*component)->priv->refcount = 1;
   (*component)->priv->priority = VCOS_THREAD_PRI_NORMAL;

   if(vcos_mutex_create(&private->lock, "mmal component lock") != VCOS_SUCCESS)
   {
      vcos_free(*component);
      return MMAL_ENOMEM;
   }

   vcos_mutex_lock(&mmal_core_lock);
   (*component)->id=mmal_core_instance_count++;
   vcos_mutex_unlock(&mmal_core_lock);

   /* Create the control port */
   (*component)->control = mmal_port_alloc(*component, MMAL_PORT_TYPE_CONTROL, 0);
   if(!(*component)->control)
      goto error;
   mmal_component_init_control_port((*component)->control);

   /* Create the actual component */
   (*component)->priv->module = constructor_private;
   if (!constructor)
      constructor = mmal_component_supplier_create;
   status = constructor(name, *component);
   if (status != MMAL_SUCCESS)
   {
      if (status == MMAL_ENOSYS)
         LOG_ERROR("could not find component '%s'", name);
      else
         LOG_ERROR("could not create component '%s' (%i)", name, status);
      goto error;
   }

   /* Make sure we have enough space for at least a MMAL_EVENT_FORMAT_CHANGED */
   if ((*component)->control->buffer_size_min <
       sizeof(MMAL_ES_FORMAT_T) + sizeof(MMAL_ES_SPECIFIC_FORMAT_T) + sizeof(MMAL_EVENT_FORMAT_CHANGED_T))
      (*component)->control->buffer_size_min = sizeof(MMAL_ES_FORMAT_T) +
         sizeof(MMAL_ES_SPECIFIC_FORMAT_T) + sizeof(MMAL_EVENT_FORMAT_CHANGED_T);
   /* Make sure we have enough events */
   if ((*component)->control->buffer_num_min < MMAL_CONTROL_PORT_BUFFERS_MIN)
      (*component)->control->buffer_num_min = MMAL_CONTROL_PORT_BUFFERS_MIN;

   /* Create the event pool */
   (*component)->priv->event_pool = mmal_pool_create((*component)->control->buffer_num_min,
         (*component)->control->buffer_size_min);
   if (!(*component)->priv->event_pool)
   {
      status = MMAL_ENOMEM;
      LOG_ERROR("could not create event pool (%d, %d)", (*component)->control->buffer_num_min,
            (*component)->control->buffer_size_min);
      goto error;
   }

   /* Build the list of all the ports */
   (*component)->port_num = (*component)->input_num + (*component)->output_num + (*component)->clock_num + 1;
   (*component)->port = vcos_malloc((*component)->port_num * sizeof(MMAL_PORT_T *), "mmal ports");
   if (!(*component)->port)
   {
      status = MMAL_ENOMEM;
      LOG_ERROR("could not create list of ports");
      goto error;
   }
   port_index = 0;
   (*component)->port[port_index++] = (*component)->control;
   for (i = 0; i < (*component)->input_num; i++)
      (*component)->port[port_index++] = (*component)->input[i];
   for (i = 0; i < (*component)->output_num; i++)
      (*component)->port[port_index++] = (*component)->output[i];
   for (i = 0; i < (*component)->clock_num; i++)
      (*component)->port[port_index++] = (*component)->clock[i];
   for (i = 0; i < (*component)->port_num; i++)
      (*component)->port[i]->index_all = i;

   LOG_INFO("created '%s' %d %p", name, (*component)->id, *component);

   /* Make sure the port types, indexes and buffer sizes are set correctly */
   (*component)->control->type = MMAL_PORT_TYPE_CONTROL;
   (*component)->control->index = 0;
   for (i = 0; i < (*component)->input_num; i++)
   {
      MMAL_PORT_T *port = (*component)->input[i];
      port->type = MMAL_PORT_TYPE_INPUT;
      port->index = i;
   }
   for (i = 0; i < (*component)->output_num; i++)
   {
      MMAL_PORT_T *port = (*component)->output[i];
      port->type = MMAL_PORT_TYPE_OUTPUT;
      port->index = i;
   }
   for (i = 0; i < (*component)->clock_num; i++)
   {
      MMAL_PORT_T *port = (*component)->clock[i];
      port->type = MMAL_PORT_TYPE_CLOCK;
      port->index = i;
   }
   for (i = 0; i < (*component)->port_num; i++)
   {
      MMAL_PORT_T *port = (*component)->port[i];
      if (port->buffer_size < port->buffer_size_min)
         port->buffer_size = port->buffer_size_min;
      if (port->buffer_num < port->buffer_num_min)
         port->buffer_num = port->buffer_num_min;
   }

   return MMAL_SUCCESS;

 error:
   mmal_component_destroy_internal(*component);
   *component = 0;
   return status;
}

/** Create an instance of a component */
MMAL_STATUS_T mmal_component_create(const char *name,
   MMAL_COMPONENT_T **component)
{
   LOG_TRACE("%s", name);
   return mmal_component_create_core(name, 0, 0, component);
}

/** Create an instance of a component */
MMAL_STATUS_T mmal_component_create_with_constructor(const char *name,
   MMAL_STATUS_T (*constructor)(const char *name, MMAL_COMPONENT_T *),
   struct MMAL_COMPONENT_MODULE_T *constructor_private,
   MMAL_COMPONENT_T **component)
{
   LOG_TRACE("%s", name);
   return mmal_component_create_core(name, constructor, constructor_private, component);
}

/** Destroy a previously created component */
static MMAL_STATUS_T mmal_component_destroy_internal(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;
   MMAL_STATUS_T status;

   LOG_TRACE("%s %d", component->name, component->id);

   mmal_component_action_deregister(component);

   /* Should pf_destroy be allowed to fail ?
    * If so, what do we do if it fails ?
    */
   if (component->priv->pf_destroy)
   {
      status = component->priv->pf_destroy(component);
      if(!vcos_verify(status == MMAL_SUCCESS))
         return status;
   }

   if (component->priv->event_pool)
      mmal_pool_destroy(component->priv->event_pool);

   if (component->control)
      mmal_port_free(component->control);

   if (component->port)
      vcos_free(component->port);

   vcos_mutex_delete(&private->lock);
   vcos_free(component);
   mmal_core_deinit();
   return MMAL_SUCCESS;
}

/** Release a reference to a component */
static MMAL_STATUS_T mmal_component_release_internal(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;
   unsigned int i;

   if (!vcos_verify(component->priv->refcount > 0))
      return MMAL_EINVAL;

   vcos_mutex_lock(&private->lock);
   if (--component->priv->refcount)
   {
      vcos_mutex_unlock(&private->lock);
      return MMAL_SUCCESS;
   }
   private->destruction_pending = 1;
   vcos_mutex_unlock(&private->lock);

   LOG_TRACE("%s %d preparing for destruction", component->name, component->id);

   /* Make sure the ports are all disabled */
   for(i = 0; i < component->input_num; i++)
      if(component->input[i]->is_enabled)
         mmal_port_disable(component->input[i]);
   for(i = 0; i < component->output_num; i++)
      if(component->output[i]->is_enabled)
         mmal_port_disable(component->output[i]);
   for(i = 0; i < component->clock_num; i++)
      if(component->clock[i]->is_enabled)
         mmal_port_disable(component->clock[i]);
   if(component->control->is_enabled)
      mmal_port_disable(component->control);

   /* Make sure all the ports are disconnected. This is necessary to prevent
    * connected ports from referencing destroyed components */
   for(i = 0; i < component->input_num; i++)
      mmal_port_disconnect(component->input[i]);
   for(i = 0; i < component->output_num; i++)
      mmal_port_disconnect(component->output[i]);
   for(i = 0; i < component->clock_num; i++)
      mmal_port_disconnect(component->clock[i]);

   /* If there is any reference pending on the ports we will delay the actual destruction */
   vcos_mutex_lock(&private->lock);
   if (component->priv->refcount_ports)
   {
      private->destruction_pending = 0;
      vcos_mutex_unlock(&private->lock);
      LOG_TRACE("%s %d delaying destruction", component->name, component->id);
      return MMAL_SUCCESS;
   }
   vcos_mutex_unlock(&private->lock);

   return mmal_component_destroy_internal(component);
}

/** Destroy a component */
MMAL_STATUS_T mmal_component_destroy(MMAL_COMPONENT_T *component)
{
   if(!component)
      return MMAL_EINVAL;

   LOG_TRACE("%s %d", component->name, component->id);

   return mmal_component_release_internal(component);
}

/** Acquire a reference to a component */
void mmal_component_acquire(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   LOG_TRACE("component %s(%d), refcount %i", component->name, component->id,
             component->priv->refcount);

   vcos_mutex_lock(&private->lock);
   component->priv->refcount++;
   vcos_mutex_unlock(&private->lock);
}

/** Release a reference to a component */
MMAL_STATUS_T mmal_component_release(MMAL_COMPONENT_T *component)
{
   if(!component)
      return MMAL_EINVAL;

   LOG_TRACE("component %s(%d), refcount %i", component->name, component->id,
             component->priv->refcount);

   return mmal_component_release_internal(component);
}

/** Enable processing on a component */
MMAL_STATUS_T mmal_component_enable(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private;
   MMAL_STATUS_T status = MMAL_ENOSYS;
   unsigned int i;

   if(!component)
      return MMAL_EINVAL;

   private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   LOG_TRACE("%s %d", component->name, component->id);

   vcos_mutex_lock(&private->lock);

   /* Check we have anything to do */
   if (component->is_enabled)
   {
      vcos_mutex_unlock(&private->lock);
      return MMAL_SUCCESS;
   }

   if (component->priv->pf_enable)
      status = component->priv->pf_enable(component);

   /* If the component does not support enable/disable, we handle that
    * in the core itself */
   if (status == MMAL_ENOSYS)
   {
      status = MMAL_SUCCESS;

      /* Resume all input / output ports */
      for (i = 0; status == MMAL_SUCCESS && i < component->input_num; i++)
         status = mmal_port_pause(component->input[i], MMAL_FALSE);
      for (i = 0; status == MMAL_SUCCESS && i < component->output_num; i++)
         status = mmal_port_pause(component->output[i], MMAL_FALSE);
   }

   if (status == MMAL_SUCCESS)
      component->is_enabled = 1;

   vcos_mutex_unlock(&private->lock);

   return status;
}

/** Disable processing on a component */
MMAL_STATUS_T mmal_component_disable(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private;
   MMAL_STATUS_T status = MMAL_ENOSYS;
   unsigned int i;

   if (!component)
      return MMAL_EINVAL;

   private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   LOG_TRACE("%s %d", component->name, component->id);

   vcos_mutex_lock(&private->lock);

   /* Check we have anything to do */
   if (!component->is_enabled)
   {
      vcos_mutex_unlock(&private->lock);
      return MMAL_SUCCESS;
   }

   if (component->priv->pf_disable)
      status = component->priv->pf_disable(component);

   /* If the component does not support enable/disable, we handle that
    * in the core itself */
   if (status == MMAL_ENOSYS)
   {
      status = MMAL_SUCCESS;

      /* Pause all input / output ports */
      for (i = 0; status == MMAL_SUCCESS && i < component->input_num; i++)
         status = mmal_port_pause(component->input[i], MMAL_TRUE);
      for (i = 0; status == MMAL_SUCCESS && i < component->output_num; i++)
         status = mmal_port_pause(component->output[i], MMAL_TRUE);
   }

   if (status == MMAL_SUCCESS)
      component->is_enabled = 0;

   vcos_mutex_unlock(&private->lock);

   return status;
}

static MMAL_STATUS_T mmal_component_enable_control_port(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   (void)port;
   (void)cb;
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_component_disable_control_port(MMAL_PORT_T *port)
{
   (void)port;
   return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_component_parameter_set(MMAL_PORT_T *control_port,
                                           const MMAL_PARAMETER_HEADER_T *param)
{
   (void)control_port;
   (void)param;
   /* No generic component control parameters */
   LOG_ERROR("parameter id 0x%08x not supported", param->id);
   return MMAL_ENOSYS;
}

MMAL_STATUS_T mmal_component_parameter_get(MMAL_PORT_T *control_port,
                                           MMAL_PARAMETER_HEADER_T *param)
{
   (void)control_port;
   (void)param;
   /* No generic component control parameters */
   LOG_ERROR("parameter id 0x%08x not supported", param->id);
   return MMAL_ENOSYS;
}

static void mmal_component_init_control_port(MMAL_PORT_T *port)
{
   port->format->type = MMAL_ES_TYPE_CONTROL;
   port->buffer_num_min = MMAL_CONTROL_PORT_BUFFERS_MIN;
   port->buffer_num = port->buffer_num_min;
   port->buffer_size_min = sizeof(MMAL_ES_FORMAT_T) + sizeof(MMAL_ES_SPECIFIC_FORMAT_T);
   port->buffer_size = port->buffer_size_min;

   /* Default to generic handling */
   port->priv->pf_enable = mmal_component_enable_control_port;
   port->priv->pf_disable = mmal_component_disable_control_port;
   port->priv->pf_parameter_set = mmal_component_parameter_set;
   port->priv->pf_parameter_get = mmal_component_parameter_get;
   /* No pf_set_format - format of control port cannot be changed */
   /* No pf_send - buffers cannot be sent to control port */
}

/** Acquire a reference on a port */
void mmal_port_acquire(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   LOG_TRACE("port %s(%p), refcount %i", port->name, port,
             component->priv->refcount_ports);

   vcos_mutex_lock(&private->lock);
   component->priv->refcount_ports++;
   vcos_mutex_unlock(&private->lock);
}

/** Release a reference on a port */
MMAL_STATUS_T mmal_port_release(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   LOG_TRACE("port %s(%p), refcount %i", port->name, port,
             component->priv->refcount_ports);

   /* Sanity check the refcount */
   if (!vcos_verify(component->priv->refcount_ports > 0))
      return MMAL_EINVAL;

   vcos_mutex_lock(&private->lock);
   if (--component->priv->refcount_ports ||
       component->priv->refcount || private->destruction_pending)
   {
      vcos_mutex_unlock(&private->lock);
      return MMAL_SUCCESS;
   }
   vcos_mutex_unlock(&private->lock);

   return mmal_component_destroy_internal(component);
}

/*****************************************************************************
 * Actions support
 *****************************************************************************/

/** Registers an action with the core */
static void *mmal_component_action_thread_func(void *arg)
{
   MMAL_COMPONENT_T *component = (MMAL_COMPONENT_T *)arg;
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;
   VCOS_STATUS_T status;

   while (1)
   {
      status = vcos_event_wait(&private->action_event);

      if (status == VCOS_EAGAIN)
         continue;
      if (private->action_quit)
         break;
      if (!vcos_verify(status == VCOS_SUCCESS))
         break;

      vcos_mutex_lock(&private->action_mutex);
      private->pf_action(component);
      vcos_mutex_unlock(&private->action_mutex);
   }
   return 0;
}

/** Registers an action with the core */
MMAL_STATUS_T mmal_component_action_register(MMAL_COMPONENT_T *component,
                                             void (*pf_action)(MMAL_COMPONENT_T *) )
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;
   VCOS_THREAD_ATTR_T attrs;
   VCOS_STATUS_T status;

   if (private->pf_action)
      return MMAL_EINVAL;

   status = vcos_event_create(&private->action_event, component->name);
   if (status != VCOS_SUCCESS)
      return MMAL_ENOMEM;

   status = vcos_mutex_create(&private->action_mutex, component->name);
   if (status != VCOS_SUCCESS)
   {
      vcos_event_delete(&private->action_event);
      return MMAL_ENOMEM;
   }

   vcos_thread_attr_init(&attrs);
   vcos_thread_attr_setpriority(&attrs,
                                private->private.priority);
   status = vcos_thread_create(&private->action_thread, component->name, &attrs,
                               mmal_component_action_thread_func, component);
   if (status != VCOS_SUCCESS)
   {
      vcos_mutex_delete(&private->action_mutex);
      vcos_event_delete(&private->action_event);
      return MMAL_ENOMEM;
   }

   private->pf_action = pf_action;
   return MMAL_SUCCESS;
}

/** De-registers the current action with the core */
MMAL_STATUS_T mmal_component_action_deregister(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   if (!private->pf_action)
      return MMAL_EINVAL;

   private->action_quit = 1;
   vcos_event_signal(&private->action_event);
   vcos_thread_join(&private->action_thread, NULL);
   vcos_event_delete(&private->action_event);
   vcos_mutex_delete(&private->action_mutex);
   private->pf_action = NULL;
   private->action_quit = 0;
   return MMAL_SUCCESS;
}

/** Triggers a registered action */
MMAL_STATUS_T mmal_component_action_trigger(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   if (!private->pf_action)
      return MMAL_EINVAL;

   vcos_event_signal(&private->action_event);
   return MMAL_SUCCESS;
}

/** Lock an action to prevent it from running */
MMAL_STATUS_T mmal_component_action_lock(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   if (!private->pf_action)
      return MMAL_EINVAL;

   vcos_mutex_lock(&private->action_mutex);
   return MMAL_SUCCESS;
}

/** Unlock an action to allow it to run again */
MMAL_STATUS_T mmal_component_action_unlock(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_CORE_PRIVATE_T *private = (MMAL_COMPONENT_CORE_PRIVATE_T *)component->priv;

   if (!private->pf_action)
      return MMAL_EINVAL;

   vcos_mutex_unlock(&private->action_mutex);
   return MMAL_SUCCESS;
}

/*****************************************************************************
 * Initialisation / Deinitialisation of the MMAL core
 *****************************************************************************/
static void mmal_core_init_once(void)
{
   vcos_mutex_create(&mmal_core_lock, VCOS_FUNCTION);
}

static void mmal_core_init(void)
{
   static VCOS_ONCE_T once = VCOS_ONCE_INIT;
   vcos_init();
   vcos_once(&once, mmal_core_init_once);

   vcos_mutex_lock(&mmal_core_lock);
   if (mmal_core_refcount++)
   {
      vcos_mutex_unlock(&mmal_core_lock);
      return;
   }

   mmal_logging_init();
   vcos_mutex_unlock(&mmal_core_lock);
}

static void mmal_core_deinit(void)
{
   vcos_mutex_lock(&mmal_core_lock);
   if (!mmal_core_refcount || --mmal_core_refcount)
   {
      vcos_mutex_unlock(&mmal_core_lock);
      return;
   }

   mmal_logging_deinit();
   vcos_mutex_unlock(&mmal_core_lock);
   vcos_deinit();
}

/*****************************************************************************
 * Supplier support
 *****************************************************************************/

/** a component supplier gets passed a string and returns a
  * component (if it can) based on that string.
  */

#define SUPPLIER_PREFIX_LEN 32
typedef struct MMAL_COMPONENT_SUPPLIER_T
{
   struct MMAL_COMPONENT_SUPPLIER_T *next;
   MMAL_COMPONENT_SUPPLIER_FUNCTION_T create;
   char prefix[SUPPLIER_PREFIX_LEN];
} MMAL_COMPONENT_SUPPLIER_T;

/** List of component suppliers.
  *
  * Does not need to be thread-safe if we assume that suppliers
  * can never be removed.
  */
static MMAL_COMPONENT_SUPPLIER_T *suppliers;

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_supplier_create(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_SUPPLIER_T *supplier = suppliers;
   MMAL_STATUS_T status = MMAL_ENOSYS;
   const char *dot = strchr(name, '.');
   size_t dot_size = dot ? dot - name : (int)strlen(name);

   /* walk list of suppliers to see if any can create this component */
   while (supplier)
   {
      if (strlen(supplier->prefix) == dot_size && !memcmp(supplier->prefix, name, dot_size))
      {
         status = supplier->create(name, component);
         if (status == MMAL_SUCCESS)
            break;
      }
      supplier = supplier->next;
   }
   return status;
}

void mmal_component_supplier_register(const char *prefix,
   MMAL_COMPONENT_SUPPLIER_FUNCTION_T create_fn)
{
   MMAL_COMPONENT_SUPPLIER_T *supplier = vcos_calloc(1,sizeof(*supplier),NULL);

   LOG_TRACE("prefix %s fn %p", (prefix ? prefix : "NULL"), create_fn);

   if (vcos_verify(supplier))
   {
      supplier->create = create_fn;
      strncpy(supplier->prefix, prefix, SUPPLIER_PREFIX_LEN);
      supplier->prefix[SUPPLIER_PREFIX_LEN-1] = '\0';

      supplier->next = suppliers;
      suppliers = supplier;
   }
   else
   {
      LOG_ERROR("no memory for supplier registry entry");
   }
}

MMAL_DESTRUCTOR(mmal_component_supplier_destructor);
void mmal_component_supplier_destructor(void)
{
   MMAL_COMPONENT_SUPPLIER_T *supplier = suppliers;

   /* walk list of suppliers and free associated memory */
   while (supplier)
   {
      MMAL_COMPONENT_SUPPLIER_T *current = supplier;
      supplier = supplier->next;
      vcos_free(current);
   }
}
