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

#define FRAME_LENGTH 2048

/* Buffering requirements */
#define INPUT_MIN_BUFFER_NUM 4
#define INPUT_RECOMMENDED_BUFFER_NUM 8

/****************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status;
   MMAL_QUEUE_T *queue;
   MMAL_BOOL_T audio_opened;

} MMAL_COMPONENT_MODULE_T;

/** Destroy a previously created component */
static MMAL_STATUS_T sdl_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   if (module->audio_opened)
      SDL_CloseAudio();
   SDL_QuitSubSystem(SDL_INIT_AUDIO);

   if(component->input_num) mmal_ports_free(component->input, 1);
   if(module->queue) mmal_queue_destroy(module->queue);
   vcos_free(module);
   return MMAL_SUCCESS;
}

static void sdl_callback( void *ctx, uint8_t *stream, int size )
{
   MMAL_PORT_T *port = (MMAL_PORT_T *)ctx;
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;
   unsigned int i, bytes;

   while (size > 0)
   {
      buffer = mmal_queue_get(module->queue);
      if (!buffer)
      {
         LOG_ERROR("audio underrun");
         return;
      }

      if (port->format->encoding == MMAL_ENCODING_PCM_SIGNED &&
          port->format->es->audio.bits_per_sample == 16)
      {
         bytes = buffer->length;
         if (bytes > (unsigned int)size) bytes = size;
         memcpy(stream, buffer->data + buffer->offset, bytes);
         buffer->offset += bytes;
         buffer->length -= bytes;
         stream += bytes;
         size -= bytes;
      }
      else if (port->format->es->audio.bits_per_sample == 32)
      {
         bytes = buffer->length;
         if (bytes > 2 * (unsigned int)size) bytes = 2 * size;
         vcos_assert(!(bytes&0x3));

         if (port->format->encoding == MMAL_ENCODING_PCM_FLOAT)
         {
            float *in = (float *)(buffer->data + buffer->offset);
            int16_t *out = (int16_t *)stream;
            for (i = 0; i < bytes / 4; i++)
            {
               if (*in >= 1.0) *out = 32767;
               else if (*in < -1.0) *out = -32768;
               else *out = *in * 32768.0;
               in++; out++;
            }
         }
         else if (port->format->encoding == MMAL_ENCODING_PCM_SIGNED)
         {
            int32_t *in = (int32_t *)(buffer->data + buffer->offset);
            int16_t *out = (int16_t *)stream;
            for (i = 0; i < bytes / 4; i++)
               *out++ = (*in++) >> 16;
         }
         buffer->offset += bytes;
         buffer->length -= bytes;
         stream += bytes / 2;
         size -= bytes / 2;
      }

      if (buffer->length)
      {
         /* We still have some data left for next time */
         mmal_queue_put_back(module->queue, buffer);
         continue;
      }

      /* Handle the EOS */
      if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS)
         mmal_event_eos_send(port);

      buffer->offset = 0;
      mmal_port_buffer_header_callback(port, buffer);
   }
}

/** Set format on a port */
static MMAL_STATUS_T sdl_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   SDL_AudioSpec desired, obtained;

   if (port->format->encoding != MMAL_ENCODING_PCM_SIGNED &&
       port->format->encoding != MMAL_ENCODING_PCM_FLOAT &&
       port->format->es->audio.bits_per_sample != 16 &&
      port->format->es->audio.bits_per_sample != 32)
   {
      LOG_ERROR("port does not support '%4.4s' at %ibps",
                (char *)&port->format->encoding, port->format->es->audio.bits_per_sample);
      return MMAL_EINVAL;
   }

   if (module->audio_opened)
      SDL_CloseAudio();
   module->audio_opened = MMAL_FALSE;

   desired.freq       = port->format->es->audio.sample_rate;
   desired.format     = AUDIO_S16SYS;
   desired.channels   = port->format->es->audio.channels;
   desired.callback   = sdl_callback;
   desired.userdata   = port;
   desired.samples    = FRAME_LENGTH;

   /* Open the sound device. */
   if (SDL_OpenAudio( &desired, &obtained ) < 0)
       return MMAL_ENOSYS;
   module->audio_opened = MMAL_TRUE;

   /* Now have a look at what we got. */
   if (obtained.format != AUDIO_S16SYS)
      return MMAL_ENOSYS;

   port->format->es->audio.sample_rate = obtained.freq;
   port->format->es->audio.channels = obtained.channels;
   port->buffer_size_min = obtained.samples * port->format->es->audio.channels * 2;

   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T sdl_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   SDL_PauseAudio( 0 );
   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T sdl_port_disable(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;

   SDL_PauseAudio( 1 );

   while((buffer = mmal_queue_get(module->queue)))
      mmal_port_buffer_header_callback(port, buffer);
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T sdl_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   /* Handle event buffers */
   if (buffer->cmd)
   {
      LOG_ERROR("discarding event %i on port %p", (int)buffer->cmd, port);
      buffer->length = 0;
      mmal_port_buffer_header_callback(port, buffer);
      return MMAL_SUCCESS;
   }

   if (module->status != MMAL_SUCCESS)
      return module->status;

   mmal_queue_put(module->queue, buffer);

   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_sdl(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;

   /* Check we're the requested component */
   if(strcmp(name, "sdl." MMAL_AUDIO_RENDER))
      return MMAL_ENOENT;

   if( SDL_WasInit(SDL_INIT_AUDIO) )
      return MMAL_ENXIO;

   /* Allocate our module context */
   component->priv->module = module = vcos_calloc(1, sizeof(*module), "mmal module");
   if(!module)
      return MMAL_ENOMEM;

   if(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_NOPARACHUTE) < 0)
      return MMAL_ENXIO;

   /* Allocate the ports for this component */
   component->input = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_INPUT, 0);
   if(!component->input)
      goto error;
   component->input_num = 1;

   module->queue = mmal_queue_create();
   if(!module->queue)
      goto error;

   component->input[0]->priv->pf_set_format = sdl_port_set_format;
   component->input[0]->priv->pf_enable = sdl_port_enable;
   component->input[0]->priv->pf_disable = sdl_port_disable;
   component->input[0]->priv->pf_send = sdl_port_send;
   component->input[0]->buffer_num_min = INPUT_MIN_BUFFER_NUM;
   component->input[0]->buffer_num_recommended = INPUT_RECOMMENDED_BUFFER_NUM;

   component->priv->pf_destroy = sdl_component_destroy;
   return MMAL_SUCCESS;

 error:
   sdl_component_destroy(component);
   return MMAL_ENOMEM;
}

MMAL_CONSTRUCTOR(mmal_register_component_sdl_audio);
void mmal_register_component_sdl_audio(void)
{
   mmal_component_supplier_register("sdl", mmal_component_create_sdl);
}
