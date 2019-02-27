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
#include "mmal_logging.h"
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"

#include <SDL/SDL.h>

#define NUM_PORTS_INPUT 1
#define SDL_WIDTH 800
#define SDL_HEIGHT 600

/* Buffering requirements */
#define INPUT_MIN_BUFFER_NUM 1
#define INPUT_RECOMMENDED_BUFFER_NUM 4

/****************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   SDL_Overlay *sdl_overlay;
   SDL_Surface *sdl_surface;
   unsigned int width;
   unsigned int height;
   MMAL_STATUS_T status;
   MMAL_RECT_T display_region;

   MMAL_QUEUE_T *queue;

   SDL_Thread *thread;
   MMAL_BOOL_T quit;
} MMAL_COMPONENT_MODULE_T;

static MMAL_STATUS_T sdl_port_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_STATUS_T status = MMAL_ENOSYS;
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   switch (param->id)
   {
   case MMAL_PARAMETER_DISPLAYREGION:
      {
         /* We only support setting the destination rectangle */
         const MMAL_DISPLAYREGION_T *display = (const MMAL_DISPLAYREGION_T *)param;
         if (display->set & MMAL_DISPLAY_SET_DEST_RECT)
            module->display_region = display->dest_rect;
         status = MMAL_SUCCESS;
      }
      break;
   default:
      break;
   }
   return status;
}

/** Destroy a previously created component */
static MMAL_STATUS_T sdl_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   SDL_Event event = {SDL_QUIT};

   module->quit = MMAL_TRUE;
   SDL_PushEvent(&event);
   if(module->thread)
      SDL_WaitThread(module->thread, NULL);
   if(module->sdl_overlay)
      SDL_FreeYUVOverlay(module->sdl_overlay);
   if(module->sdl_surface)
      SDL_FreeSurface(module->sdl_surface);
   SDL_QuitSubSystem(SDL_INIT_VIDEO);

   if(component->input_num) mmal_ports_free(component->input, 1);
   if(module->queue) mmal_queue_destroy(module->queue);
   vcos_free(module);
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_sdl_create_surface(MMAL_COMPONENT_MODULE_T *module)
{
   uint32_t flags;
   int bpp;
   int w = module->display_region.width;
   int h = module->display_region.height;

   flags = SDL_ANYFORMAT | SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_RESIZABLE;
   bpp = SDL_VideoModeOK(w, h, 16, flags);
   if(!bpp)
   {
      LOG_ERROR("no SDL video mode available");
      return MMAL_ENOSYS;
   }
   module->sdl_surface = SDL_SetVideoMode(w, h, bpp, flags);
   if(!module->sdl_surface)
   {
      LOG_ERROR("cannot create SDL surface");
      return MMAL_ENOMEM;
   }
   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T sdl_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_STATUS_T status;

   if ((status=mmal_sdl_create_surface(module)) != MMAL_SUCCESS)
      return status;

   /* We only support I420 */
   if (port->format->encoding != MMAL_ENCODING_I420)
      return MMAL_ENOSYS;

   /* Check if we need to re-create an overlay */
   if (module->sdl_overlay &&
       module->width == port->format->es->video.width &&
       module->height == port->format->es->video.height)
      return MMAL_SUCCESS; /* Nothing to do */

   if (module->sdl_overlay)
      SDL_FreeYUVOverlay(module->sdl_overlay);

   /* Create overlay */
   module->sdl_overlay =
      SDL_CreateYUVOverlay(port->format->es->video.width,
                           port->format->es->video.height,
                           SDL_YV12_OVERLAY, module->sdl_surface);
   if (!module->sdl_overlay)
   {
      LOG_ERROR("cannot create SDL overlay");
      return MMAL_ENOSPC;
   }
   module->width = port->format->es->video.width;
   module->height = port->format->es->video.height;

   port->buffer_size_min = module->width * module->height * 3 / 2;
   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T sdl_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T sdl_port_disable(MMAL_PORT_T *port)
{
   MMAL_PARAM_UNUSED(port);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T sdl_port_flush(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;

   /* Flush buffers that our component is holding on to.
    * If the reading thread is holding onto a buffer it will send it back ASAP as well
    * so no need to care about that.  */
   while((buffer = mmal_queue_get(module->queue)))
      mmal_port_buffer_header_callback(port, buffer);

   return MMAL_SUCCESS;
}

static MMAL_BOOL_T sdl_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_PORT_T *port = component->input[0];
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   unsigned int width = port->format->es->video.width;
   unsigned int height = port->format->es->video.height;
   MMAL_BUFFER_HEADER_T *buffer;
   uint8_t *src_plane[3];
   uint32_t *src_pitch;
   unsigned int i, line;
   MMAL_BOOL_T eos;
   SDL_Rect rect;

   buffer = mmal_queue_get(module->queue);
   if (!buffer)
	   return 0;

   eos = buffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS;

   /* Handle event buffers */
   if (buffer->cmd)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event = mmal_event_format_changed_get(buffer);
      if (event)
      {
         mmal_format_copy(port->format, event->format);
         module->status = port->priv->pf_set_format(port);
         if (module->status != MMAL_SUCCESS)
         {
            LOG_ERROR("format not set on port %p", port);
            if (mmal_event_error_send(port->component, module->status) != MMAL_SUCCESS)
               LOG_ERROR("unable to send an error event buffer");
         }
      }
      else
      {
         LOG_ERROR("discarding event %i on port %p", (int)buffer->cmd, port);
      }

      buffer->length = 0;
      mmal_port_buffer_header_callback(port, buffer);
      return 1;
   }

   if (module->status != MMAL_SUCCESS)
      return 1;

   /* Ignore empty buffers */
   if (!buffer->length)
      goto end;

   // FIXME: sanity check the size of the buffer

   /* Blit the buffer onto the overlay. */
   src_pitch = buffer->type->video.pitch;
   src_plane[0] = buffer->data + buffer->type->video.offset[0];
   src_plane[1] = buffer->data + buffer->type->video.offset[2];
   src_plane[2] = buffer->data + buffer->type->video.offset[1];

   SDL_LockYUVOverlay(module->sdl_overlay);
   for (i=0; i<3; i++)
   {
      uint8_t *src = src_plane[i];
      uint8_t *dst = module->sdl_overlay->pixels[i];

      if(i == 1) {width /= 2; height /= 2;}
      for(line = 0; line < height; line++)
      {
         memcpy(dst, src, width);
         src += src_pitch[i];
         dst += module->sdl_overlay->pitches[i];
      }
   }
   SDL_UnlockYUVOverlay(module->sdl_overlay);

   width = port->format->es->video.width;
   height = port->format->es->video.height;
   rect.x = module->display_region.x;
   rect.w = module->display_region.width;
   height = rect.w * height / width;
   rect.y = module->display_region.y + (module->display_region.height - height) / 2;
   rect.h = height;

   SDL_DisplayYUVOverlay(module->sdl_overlay, &rect);

 end:
   buffer->offset = buffer->length = 0;
   mmal_port_buffer_header_callback(port, buffer);

   /* Generate EOS events */
   if (eos)
      mmal_event_eos_send(port);

   return 1;
}

/*****************************************************************************/
static void sdl_do_processing_loop(MMAL_COMPONENT_T *component)
{
   while (sdl_do_processing(component));
}

/** Buffer sending */
static MMAL_STATUS_T sdl_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   mmal_queue_put(module->queue, buffer);
   mmal_component_action_trigger(port->component);

   return MMAL_SUCCESS;
}

/** SDL event thread */
static int sdl_event_thread(void *arg)
{
   MMAL_COMPONENT_T *component = (MMAL_COMPONENT_T *)arg;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   SDL_Event event;

   while (SDL_WaitEvent(&event))
   {
      switch (event.type)
      {
      case SDL_QUIT:
         if (!module->quit)
            mmal_event_error_send(component, MMAL_SUCCESS);
         return 0;
      default:
         break;
      }
   }

   return 0;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_sdl(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;

   /* Check we're the requested component */
   if(strcmp(name, "sdl." MMAL_VIDEO_RENDER))
      return MMAL_ENOENT;

   if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTTHREAD|SDL_INIT_NOPARACHUTE) < 0)
      return MMAL_ENXIO;

   /* Allocate our module context */
   component->priv->module = module = vcos_calloc(1, sizeof(*module), "mmal module");
   if(!module) return MMAL_ENOMEM;

   module->queue = mmal_queue_create();
   if(!module->queue) goto error;

   /* Allocate the ports for this component */
   component->input = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_INPUT, 0);
   if(!component->input) goto error;
   component->input_num = 1;
   module->display_region.width = SDL_WIDTH;
   module->display_region.height = SDL_HEIGHT;

   /************/

   component->input[0]->priv->pf_set_format = sdl_port_set_format;
   component->input[0]->priv->pf_enable = sdl_port_enable;
   component->input[0]->priv->pf_disable = sdl_port_disable;
   component->input[0]->priv->pf_flush = sdl_port_flush;
   component->input[0]->priv->pf_send = sdl_port_send;
   component->input[0]->priv->pf_parameter_set = sdl_port_parameter_set;
   component->input[0]->buffer_num_min = INPUT_MIN_BUFFER_NUM;
   component->input[0]->buffer_num_recommended = INPUT_RECOMMENDED_BUFFER_NUM;

   component->priv->pf_destroy = sdl_component_destroy;

   /* Create a thread to monitor SDL events */
   module->thread = SDL_CreateThread(sdl_event_thread, component);

   status = mmal_component_action_register(component, sdl_do_processing_loop);
   if (status != MMAL_SUCCESS)
      goto error;

   return MMAL_SUCCESS;

 error:
   sdl_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_sdl);
void mmal_register_component_sdl(void)
{
   mmal_component_supplier_register("sdl", mmal_component_create_sdl);
}
