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
#include "mmal_logging.h"

#define attribute_deprecated
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT( 52, 23, 0 )
# include "libavformat/avformat.h"
 static AVPacket null_packet = {AV_NOPTS_VALUE, AV_NOPTS_VALUE};
# define av_init_packet(a) *(a) = null_packet
#endif

#if LIBAVCODEC_VERSION_MAJOR < 53
# define avcodec_decode_video2(a,b,c,d) avcodec_decode_video(a,b,c,(d)->data,(d)->size)
#endif

/* Buffering requirements */
#define INPUT_MIN_BUFFER_SIZE (800*1024)
#define INPUT_MIN_BUFFER_NUM 1
#define INPUT_RECOMMENDED_BUFFER_SIZE INPUT_MIN_BUFFER_SIZE
#define INPUT_RECOMMENDED_BUFFER_NUM 10
#define OUTPUT_MIN_BUFFER_NUM 1
#define OUTPUT_RECOMMENDED_BUFFER_NUM 4

#define DEFAULT_WIDTH 320
#define DEFAULT_HEIGHT 240

static uint32_t encoding_to_codecid(uint32_t encoding);
static uint32_t pixfmt_to_encoding(enum PixelFormat);

/****************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status;

   MMAL_QUEUE_T *queue_in;
   MMAL_QUEUE_T *queue_out;

   int picture_available;
   int64_t pts;
   int64_t dts;

   AVFrame *picture;
   AVCodecContext *codec_context;
   AVCodec *codec;

   int width;
   int height;
   enum PixelFormat pix_fmt;
   AVPicture layout;
   unsigned int planes;

   int frame_size;
   MMAL_BOOL_T output_needs_configuring;

} MMAL_COMPONENT_MODULE_T;

/** Destroy a previously created component */
static MMAL_STATUS_T avcodec_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   if (module->picture)
      av_free(module->picture);
   if (module->codec_context)
   {
      if(module->codec_context->extradata) vcos_free(module->codec_context->extradata);
      if(module->codec_context->codec) avcodec_close(module->codec_context);
      av_free(module->codec_context);
   }

   if(module->queue_in) mmal_queue_destroy(module->queue_in);
   if(module->queue_out) mmal_queue_destroy(module->queue_out);
   vcos_free(module);
   if(component->input_num) mmal_ports_free(component->input, 1);
   if(component->output_num) mmal_ports_free(component->output, 1);
   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T avcodec_input_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   enum CodecID codec_id;
   AVCodec *codec;

   codec_id = encoding_to_codecid(port->format->encoding);
   if(codec_id == CODEC_ID_NONE ||
      !(codec = avcodec_find_decoder(codec_id)))
   {
      LOG_ERROR("ffmpeg codec id %d (for %4.4s) not recognized",
                codec_id, (char *)&port->format->encoding);
      return MMAL_ENXIO;
   }

   module->picture = avcodec_alloc_frame();

   module->codec_context->width  = port->format->es->video.width;
   module->codec_context->height  = port->format->es->video.height;
   module->codec_context->extradata_size  = port->format->extradata_size;
   module->codec_context->extradata =
      vcos_calloc(1, port->format->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE,
                  "avcodec extradata");
   if(module->codec_context->extradata)
      memcpy(module->codec_context->extradata, port->format->extradata,
             port->format->extradata_size);

   if (codec->capabilities & CODEC_CAP_TRUNCATED)
      module->codec_context->flags |= CODEC_FLAG_TRUNCATED;

   if (avcodec_open(module->codec_context, codec) < 0)
   {
      LOG_ERROR("could not open codec");
      return MMAL_EIO;
   }

   /* Set a default format */
   if (module->codec_context->pix_fmt == PIX_FMT_NONE)
      module->codec_context->pix_fmt = PIX_FMT_YUV420P;

   /* Copy format to output */
   LOG_DEBUG("avcodec output format %i (%ix%i)", module->codec_context->pix_fmt,
             module->codec_context->width, module->codec_context->height);
   port->format->es->video.width = module->codec_context->width;
   port->format->es->video.height = module->codec_context->height;
   mmal_format_copy(component->output[0]->format, port->format);
   component->output[0]->format->encoding = pixfmt_to_encoding(module->codec_context->pix_fmt);
   if (!component->output[0]->format->es->video.width)
      component->output[0]->format->es->video.width = DEFAULT_WIDTH;
   if (!component->output[0]->format->es->video.height)
      component->output[0]->format->es->video.height = DEFAULT_HEIGHT;

   component->output[0]->priv->pf_set_format(component->output[0]);

   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T avcodec_output_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   /* Format can only be set to what is output by the codec */
   if (pixfmt_to_encoding(module->codec_context->pix_fmt) != port->format->encoding)
      return MMAL_EINVAL;

   module->pix_fmt = module->codec_context->pix_fmt;
   module->width = port->format->es->video.width;
   module->height = port->format->es->video.height;

   module->frame_size =
      avpicture_fill(&module->layout, 0, module->pix_fmt, module->width, module->height);
   if (module->frame_size < 0)
      return MMAL_EINVAL;

   /* Calculate the number of planes for this format */
   for (module->planes = 0; module->planes < 4; )
      if (!module->layout.data[module->planes++])
         break;

   port->buffer_size_min = module->frame_size;
   port->component->priv->module->output_needs_configuring = 0;
   mmal_component_action_trigger(port->component);

   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T avcodec_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T avcodec_port_flush(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_QUEUE_T *queue;

   if(port->type == MMAL_PORT_TYPE_OUTPUT)
      queue = module->queue_out;
   else if(port->type == MMAL_PORT_TYPE_INPUT)
      queue = module->queue_in;
   else
      return MMAL_EINVAL;

   /* Flush buffers that our component is holding on to.
    * If the reading thread is holding onto a buffer it will send it back ASAP as well
    * so no need to care about that.  */
   while((buffer = mmal_queue_get(queue)))
      mmal_port_buffer_header_callback(port, buffer);

   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T avcodec_port_disable(MMAL_PORT_T *port)
{
   MMAL_STATUS_T status;

   /* Actions are prevented from running at that point so a flush
    * will return all buffers. */
   status = avcodec_port_flush(port);
   if(status != MMAL_SUCCESS)
      return status;

   return MMAL_SUCCESS;
}

/*****************************************************************************/
static void avcodec_send_event_format_changed(MMAL_COMPONENT_T *component, MMAL_PORT_T *port)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer = NULL;
   MMAL_EVENT_FORMAT_CHANGED_T *event;

   /* Get an event buffer */
   module->status = mmal_port_event_get(port, &buffer, MMAL_EVENT_FORMAT_CHANGED);
   if (module->status != MMAL_SUCCESS)
   {
      LOG_ERROR("unable to get an event buffer");
      return;
   }
   event = mmal_event_format_changed_get(buffer);

   /* Fill in the new format */
   mmal_format_copy(event->format, port->format);
   event->format->es->video.width = module->codec_context->width;
   event->format->es->video.height = module->codec_context->height;
   event->format->encoding = pixfmt_to_encoding(module->codec_context->pix_fmt);

   /* Pass on the buffer requirements */
   event->buffer_num_min = port->buffer_num_min;
   event->buffer_size_min = module->codec_context->width * module->codec_context->height * 2;
   event->buffer_size_recommended = event->buffer_size_min;
   event->buffer_num_recommended = port->buffer_num_recommended;

   module->output_needs_configuring = 1;
   mmal_port_event_send(port, buffer);
}

/*****************************************************************************/
static MMAL_STATUS_T avcodec_send_eos(MMAL_COMPONENT_T *component, MMAL_PORT_T *port)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *out = mmal_queue_get(module->queue_out);

   if (!out)
      return MMAL_EAGAIN;

   out->length = 0;
   out->flags = MMAL_BUFFER_HEADER_FLAG_EOS;
   mmal_port_buffer_header_callback(port, out);
   return MMAL_SUCCESS;
}

/*****************************************************************************/
static MMAL_STATUS_T avcodec_send_picture(MMAL_COMPONENT_T *component, MMAL_PORT_T *port)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *out;
   int i, size;

   /* Detect format changes */
   if (module->codec_context->width != module->width ||
       module->codec_context->height != module->height ||
       module->codec_context->pix_fmt != module->pix_fmt)
   {
      avcodec_send_event_format_changed(component, port);
      return MMAL_EAGAIN;
   }

   out = mmal_queue_get(module->queue_out);
   if (!out)
      return MMAL_EAGAIN;

   size = avpicture_layout((AVPicture *)module->picture, module->pix_fmt,
                           module->width, module->height, out->data, out->alloc_size);
   if (size < 0)
   {
      mmal_queue_put_back(module->queue_out, out);
      LOG_ERROR("avpicture_layout failed: %i, %i, %i, %i",module->pix_fmt,
                module->width, module->height, out->alloc_size );
      mmal_event_error_send(component, MMAL_EINVAL);
      return MMAL_EINVAL;
   }

   out->length = size;
   out->pts    = module->pts;
   out->flags  = 0;

   out->type->video.planes = module->planes;
   for (i = 0; i < 3; i++)
   {
      out->type->video.offset[i] = (uint64_t)module->layout.data[i];
      out->type->video.pitch[i] = module->layout.linesize[i];
   }

   mmal_port_buffer_header_callback(port, out);
   return MMAL_SUCCESS;
}

/*****************************************************************************/
static MMAL_BOOL_T avcodec_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port_in = component->input[0];
   MMAL_PORT_T *port_out = component->output[0];
   MMAL_BUFFER_HEADER_T *in;
   AVPacket avpkt;
   int used = 0;

   if (module->output_needs_configuring)
      return 0;

   if (module->picture_available &&
       avcodec_send_picture(component, port_out) != MMAL_SUCCESS)
      return 0;

   module->picture_available = 0;

   /* Get input buffer to decode */
   in = mmal_queue_get(module->queue_in);
   if (!in)
      return 0;

   /* Discard empty buffers. EOS buffers are not discarded since they will be used
    * to flush the codec. */
   if (!in->length && !(in->flags & MMAL_BUFFER_HEADER_FLAG_EOS))
      goto end;

   /* Avcodec expects padded input data */
   if (in->length && !in->offset)
   {
      if(in->length + FF_INPUT_BUFFER_PADDING_SIZE <= in->alloc_size)
         memset(in->data + in->length, 0, FF_INPUT_BUFFER_PADDING_SIZE);
      else
         LOG_WARN("could not pad buffer"); // Try to decode the data anyway
   }

   /* The actual decoding */
   module->codec_context->reordered_opaque = in->pts;
   av_init_packet(&avpkt);
   avpkt.data = in->length ? in->data + in->offset : 0;
   avpkt.size = in->length;
   used = avcodec_decode_video2(module->codec_context, module->picture,
                                &module->picture_available, &avpkt);

   /* Check for errors */
   if (used < 0 || used > (int)in->length)
   {
      LOG_ERROR("decoding failed (%i), discarding buffer", used);
      used = in->length;
   }

   if (module->picture_available)
   {
      module->pts = module->picture->reordered_opaque;
      if (module->pts == MMAL_TIME_UNKNOWN)
         module->pts = in->dts;
   }

 end:
   in->offset += used;
   in->length -= used;

   if (in->length)
   {
      mmal_queue_put_back(module->queue_in, in);
      return 1;
   }

   /* We want to keep the EOS buffer until all the frames have been flushed */
   if ((in->flags & MMAL_BUFFER_HEADER_FLAG_EOS) &&
       module->picture_available)
   {
      mmal_queue_put_back(module->queue_in, in);
      return 1;
   }

   /* Send EOS */
   if ((in->flags & MMAL_BUFFER_HEADER_FLAG_EOS) &&
       avcodec_send_eos(component, port_out) != MMAL_SUCCESS)
   {
      mmal_queue_put_back(module->queue_in, in);
      return 0;
   }

   in->offset = 0;
   mmal_port_buffer_header_callback(port_in, in);
   return 1;
}

/*****************************************************************************/
static void avcodec_do_processing_loop(MMAL_COMPONENT_T *component)
{
   while (avcodec_do_processing(component));
}

/** Buffer sending */
static MMAL_STATUS_T avcodec_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   if(port->type == MMAL_PORT_TYPE_OUTPUT) mmal_queue_put(module->queue_out, buffer);
   if(port->type == MMAL_PORT_TYPE_INPUT) mmal_queue_put(module->queue_in, buffer);
   mmal_component_action_trigger(port->component);

   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_avcodec(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status = MMAL_ENOMEM;
   MMAL_COMPONENT_MODULE_T *module;

   /* Check we're the requested component */
   if(strcmp(name, "avcodec." MMAL_VIDEO_DECODE))
      return MMAL_ENOENT;

   /* Allocate our module context */
   component->priv->module = module = vcos_malloc(sizeof(*module), "mmal module");
   memset(module, 0, sizeof(*module));

   /* Allocate the ports for this component */
   component->input = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_INPUT, 0);
   if(!component->input) goto error;
   component->input_num = 1;
   component->output = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_OUTPUT, 0);
   if(!component->output) goto error;
   component->output_num = 1;

   module->queue_in = mmal_queue_create();
   if(!module->queue_in) goto error;
   module->queue_out = mmal_queue_create();
   if(!module->queue_out) goto error;

   module->codec_context = avcodec_alloc_context();
   if(!module->codec_context) goto error;

   component->input[0]->priv->pf_set_format = avcodec_input_port_set_format;
   component->input[0]->priv->pf_enable = avcodec_port_enable;
   component->input[0]->priv->pf_disable = avcodec_port_disable;
   component->input[0]->priv->pf_flush = avcodec_port_flush;
   component->input[0]->priv->pf_send = avcodec_port_send;
   component->input[0]->buffer_num_min = INPUT_MIN_BUFFER_NUM;
   component->input[0]->buffer_num_recommended = INPUT_RECOMMENDED_BUFFER_NUM;
   component->input[0]->buffer_size_min = INPUT_MIN_BUFFER_SIZE;
   component->input[0]->buffer_size_recommended = INPUT_RECOMMENDED_BUFFER_SIZE;

   component->output[0]->priv->pf_set_format = avcodec_output_port_set_format;
   component->output[0]->priv->pf_enable = avcodec_port_enable;
   component->output[0]->priv->pf_disable = avcodec_port_disable;
   component->output[0]->priv->pf_flush = avcodec_port_flush;
   component->output[0]->priv->pf_send = avcodec_port_send;
   component->output[0]->buffer_num_min = OUTPUT_MIN_BUFFER_NUM;
   component->output[0]->buffer_num_recommended = OUTPUT_RECOMMENDED_BUFFER_NUM;

   component->output[0]->format->type = MMAL_ES_TYPE_VIDEO;
   component->output[0]->format->encoding = MMAL_ENCODING_I420;
   component->output[0]->format->es->video.width = DEFAULT_WIDTH;
   component->output[0]->format->es->video.height = DEFAULT_HEIGHT;

   component->priv->pf_destroy = avcodec_component_destroy;

   status = mmal_component_action_register(component, avcodec_do_processing_loop);
   if (status != MMAL_SUCCESS)
      goto error;

   return MMAL_SUCCESS;

 error:
   avcodec_component_destroy(component);
   return status;
}

static struct {
   uint32_t encoding;
   int codecid;
} codec_to_encoding_table[] =
{
   {MMAL_ENCODING_H263,    CODEC_ID_H263},
   {MMAL_ENCODING_H264,    CODEC_ID_H264},
   {MMAL_ENCODING_MP4V,    CODEC_ID_MPEG4},
   {MMAL_ENCODING_MP2V,    CODEC_ID_MPEG2VIDEO},
   {MMAL_ENCODING_MP1V,    CODEC_ID_MPEG1VIDEO},
   {MMAL_ENCODING_WMV3,    CODEC_ID_WMV3},
   {MMAL_ENCODING_WMV2,    CODEC_ID_WMV2},
   {MMAL_ENCODING_WMV1,    CODEC_ID_WMV1},
   {MMAL_ENCODING_WVC1,    CODEC_ID_VC1},
   {MMAL_ENCODING_VP6,     CODEC_ID_VP6},
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 52, 68, 2 )
   {MMAL_ENCODING_VP8,     CODEC_ID_VP8},
#endif
   {MMAL_ENCODING_THEORA,  CODEC_ID_THEORA},

   {MMAL_ENCODING_GIF,  CODEC_ID_GIF},
   {MMAL_ENCODING_PNG,  CODEC_ID_PNG},
   {MMAL_ENCODING_PPM,  CODEC_ID_PPM},
   {MMAL_ENCODING_BMP,  CODEC_ID_BMP},
   {MMAL_ENCODING_JPEG, CODEC_ID_MJPEG},

   {MMAL_ENCODING_UNKNOWN, CODEC_ID_NONE}
};

static uint32_t encoding_to_codecid(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; codec_to_encoding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(codec_to_encoding_table[i].encoding == encoding) break;
   return codec_to_encoding_table[i].codecid;
}

static struct {
   uint32_t encoding;
   enum PixelFormat pixfmt;
} pixfmt_to_encoding_table[] =
{
   {MMAL_ENCODING_I420,    PIX_FMT_YUV420P},
   {MMAL_ENCODING_I422,    PIX_FMT_YUV422P},
   {MMAL_ENCODING_I420,    PIX_FMT_YUVJ420P}, // FIXME
   {MMAL_ENCODING_I422,    PIX_FMT_YUVJ422P}, // FIXME
   {MMAL_ENCODING_RGB16,   PIX_FMT_RGB565},
   {MMAL_ENCODING_BGR16,   PIX_FMT_BGR565},
   {MMAL_ENCODING_RGB24,   PIX_FMT_RGB24},
   {MMAL_ENCODING_BGR24,   PIX_FMT_BGR24},
   {MMAL_ENCODING_ARGB,    PIX_FMT_ARGB},
   {MMAL_ENCODING_RGBA,    PIX_FMT_RGBA},
   {MMAL_ENCODING_ABGR,    PIX_FMT_ABGR},
   {MMAL_ENCODING_BGRA,    PIX_FMT_BGRA},
   {MMAL_ENCODING_UNKNOWN, PIX_FMT_NONE}
};

static uint32_t pixfmt_to_encoding(enum PixelFormat pixfmt)
{
   unsigned int i;
   for(i = 0; pixfmt_to_encoding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(pixfmt_to_encoding_table[i].pixfmt == pixfmt) break;
   return pixfmt_to_encoding_table[i].encoding;
}

MMAL_CONSTRUCTOR(mmal_register_component_avcodec);
void mmal_register_component_avcodec(void)
{
   avcodec_init();
   avcodec_register_all();

   mmal_component_supplier_register("avcodec", mmal_component_create_avcodec);
}
