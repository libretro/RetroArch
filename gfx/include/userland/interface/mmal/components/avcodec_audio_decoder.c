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

#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT( 52, 23, 0 )
# include "libavformat/avformat.h"
 static AVPacket null_packet = {AV_NOPTS_VALUE, AV_NOPTS_VALUE};
# define av_init_packet(a) *(a) = null_packet
#endif

#if LIBAVCODEC_VERSION_MAJOR < 53
# define avcodec_decode_audio3(a,b,c,d) avcodec_decode_audio2(a,b,c,(d)->data,(d)->size)
#endif

#if LIBAVCODEC_VERSION_MAJOR < 54
#define AVSampleFormat      SampleFormat
#define AV_SAMPLE_FMT_NONE  SAMPLE_FMT_NONE
#define AV_SAMPLE_FMT_U8    SAMPLE_FMT_U8
#define AV_SAMPLE_FMT_S16   SAMPLE_FMT_S16
#define AV_SAMPLE_FMT_S32   SAMPLE_FMT_S32
#define AV_SAMPLE_FMT_FLT   SAMPLE_FMT_FLT
#define AV_SAMPLE_FMT_DBL   SAMPLE_FMT_DBL
#endif

/* Buffering requirements */
#define INPUT_MIN_BUFFER_SIZE (4*1024)
#define INPUT_MIN_BUFFER_NUM 1
#define INPUT_RECOMMENDED_BUFFER_SIZE INPUT_MIN_BUFFER_SIZE
#define INPUT_RECOMMENDED_BUFFER_NUM 10
#define OUTPUT_MIN_BUFFER_NUM 1
#define OUTPUT_RECOMMENDED_BUFFER_NUM 4
#define OUTPUT_MIN_BUFFER_SIZE 512
#define OUTPUT_RECOMMENDED_BUFFER_SIZE 4096

static uint32_t encoding_to_codecid(uint32_t encoding);
static uint32_t samplefmt_to_encoding(enum AVSampleFormat);
static unsigned int samplefmt_to_sample_size(enum AVSampleFormat samplefmt);

/****************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status;

   MMAL_QUEUE_T *queue_in;
   MMAL_QUEUE_T *queue_out;

   int64_t pts;

   int64_t last_pts;
   int64_t samples_since_last_pts;

   int output_buffer_size;
   uint8_t *output_buffer;

   uint8_t *output;
   int output_size;
   AVCodecContext *codec_context;
   AVCodec *codec;

   enum AVSampleFormat sample_fmt;
   int channels;
   int sample_rate;
   int bits_per_sample;

   MMAL_BOOL_T output_needs_configuring;

} MMAL_COMPONENT_MODULE_T;

/** Destroy a previously created component */
static MMAL_STATUS_T avcodec_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   if (module->codec_context)
   {
      if (module->codec_context->extradata)
         vcos_free(module->codec_context->extradata);
      if (module->codec_context->codec)
         avcodec_close(module->codec_context);
      av_free(module->codec_context);
   }
   if (module->output_buffer)
      av_free(module->output_buffer);

   if (module->queue_in)
      mmal_queue_destroy(module->queue_in);
   if (module->queue_out)
      mmal_queue_destroy(module->queue_out);
   vcos_free(module);
   if (component->input_num)
      mmal_ports_free(component->input, 1);
   if (component->output_num)
      mmal_ports_free(component->output, 1);
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
   if (codec_id == CODEC_ID_NONE ||
       !(codec = avcodec_find_decoder(codec_id)))
   {
      LOG_ERROR("ffmpeg codec id %d (for %4.4s) not recognized",
                codec_id, (char *)&port->format->encoding);
      return MMAL_ENXIO;
   }

   module->output_buffer_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
   if (module->output_buffer)
      av_free(module->output_buffer);
   module->output_buffer = av_malloc(module->output_buffer_size);

   module->codec_context->sample_rate  = port->format->es->audio.sample_rate;
   module->codec_context->channels  = port->format->es->audio.channels;
   module->codec_context->block_align = port->format->es->audio.block_align;
   module->codec_context->bit_rate = port->format->bitrate;
   module->codec_context->bits_per_coded_sample = port->format->es->audio.bits_per_sample;
   module->codec_context->extradata_size  = port->format->extradata_size;
   module->codec_context->extradata =
      vcos_calloc(1, port->format->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE,
                  "avcodec extradata");
   if (module->codec_context->extradata)
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
   if (module->codec_context->sample_fmt == AV_SAMPLE_FMT_NONE)
      module->codec_context->sample_fmt = AV_SAMPLE_FMT_S16;

   /* Copy format to output */
   mmal_format_copy(component->output[0]->format, port->format);
   LOG_DEBUG("avcodec output format %i", module->codec_context->sample_fmt);
   component->output[0]->format->encoding = samplefmt_to_encoding(module->codec_context->sample_fmt);
   component->output[0]->format->es->audio.bits_per_sample =
      samplefmt_to_sample_size(module->codec_context->sample_fmt);

   component->output[0]->priv->pf_set_format(component->output[0]);
   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T avcodec_output_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;

   /* Format can only be set to what is output by the codec */
   if (samplefmt_to_encoding(module->codec_context->sample_fmt) != port->format->encoding ||
       samplefmt_to_sample_size(module->codec_context->sample_fmt) != port->format->es->audio.bits_per_sample)
      return MMAL_EINVAL;

   if (!port->format->es->audio.sample_rate || !port->format->es->audio.channels)
      return MMAL_EINVAL;

   module->sample_fmt = module->codec_context->sample_fmt;
   module->sample_rate = port->format->es->audio.sample_rate;
   module->channels = port->format->es->audio.channels;
   module->bits_per_sample = port->format->es->audio.bits_per_sample;

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
   event->format->es->audio.sample_rate = module->codec_context->sample_rate;
   event->format->es->audio.channels = module->codec_context->channels;
   event->format->encoding = samplefmt_to_encoding(module->codec_context->sample_fmt);
   event->format->es->audio.bits_per_sample = samplefmt_to_sample_size(module->codec_context->sample_fmt);

   /* Pass on the buffer requirements */
   event->buffer_num_min = port->buffer_num_min;
   event->buffer_size_min = port->buffer_size_min;
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
static MMAL_STATUS_T avcodec_send_frame(MMAL_COMPONENT_T *component, MMAL_PORT_T *port)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *out;
   int size, samples;

   /* Detect format changes */
   if (module->codec_context->channels != module->channels ||
       module->codec_context->sample_rate != module->sample_rate ||
       module->codec_context->sample_fmt != module->sample_fmt)
   {
      avcodec_send_event_format_changed(component, port);
      return MMAL_EAGAIN;
   }

   out = mmal_queue_get(module->queue_out);
   if (!out)
      return MMAL_EAGAIN;

   size = module->output_size;
   if (size > (int)out->alloc_size)
      size = out->alloc_size;

   samples = size / module->channels * 8 / module->bits_per_sample;
   size = samples * module->channels * module->bits_per_sample / 8;
   out->length = size;
   out->pts    = module->pts;
   out->flags  = 0;
   memcpy(out->data, module->output, size);
   module->output_size -= size;
   module->output += size;

   if (module->pts != MMAL_TIME_UNKNOWN)
   {
      module->last_pts = module->pts;
      module->samples_since_last_pts = 0;
   }
   module->pts = MMAL_TIME_UNKNOWN;
   module->samples_since_last_pts += samples;

   if (out->pts == MMAL_TIME_UNKNOWN)
      out->pts = module->last_pts + module->samples_since_last_pts * 1000000 / module->sample_rate;

   out->dts = out->pts;
   mmal_port_buffer_header_callback(port, out);
   return MMAL_SUCCESS;
}

/*****************************************************************************/
static MMAL_BOOL_T avaudio_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port_in = component->input[0];
   MMAL_PORT_T *port_out = component->output[0];
   MMAL_BUFFER_HEADER_T *in;
   AVPacket avpkt;
   int used = 0;

   if (module->output_needs_configuring)
      return 0;

   if (module->output_size &&
       avcodec_send_frame(component, port_out) != MMAL_SUCCESS)
      return 0;
   if (module->output_size)
      return 1;

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
   av_init_packet(&avpkt);
   avpkt.data = in->length ? in->data + in->offset : 0;
   avpkt.size = in->length;
   module->output_size = module->output_buffer_size;
   module->output = module->output_buffer;
   used = avcodec_decode_audio3(module->codec_context, (int16_t*)module->output,
                                &module->output_size, &avpkt);

   /* Check for errors */
   if (used < 0 || used > (int)in->length)
   {
      LOG_ERROR("decoding failed (%i), discarding buffer", used);
      used = in->length;
   }

   module->pts = in->dts;
   if (module->pts == MMAL_TIME_UNKNOWN)
      module->pts = in->pts;

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
       module->output_size)
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
   while (avaudio_do_processing(component));
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
   if(strcmp(name, "avcodec." MMAL_AUDIO_DECODE))
      return MMAL_ENOENT;

   /* Allocate our module context */
   component->priv->module = module = vcos_calloc(1, sizeof(*module), "mmal module");
   if(!module)
      return MMAL_ENOENT;

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
   component->output[0]->priv->pf_disable = avcodec_port_flush;
   component->output[0]->priv->pf_send = avcodec_port_send;
   component->output[0]->buffer_num_min = OUTPUT_MIN_BUFFER_NUM;
   component->output[0]->buffer_num_recommended = OUTPUT_RECOMMENDED_BUFFER_NUM;
   component->output[0]->buffer_size_min = OUTPUT_MIN_BUFFER_SIZE;
   component->output[0]->buffer_size_recommended = OUTPUT_RECOMMENDED_BUFFER_SIZE;

   component->output[0]->format->type = MMAL_ES_TYPE_AUDIO;
   component->output[0]->format->encoding = MMAL_ENCODING_PCM_SIGNED_LE;

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
   {MMAL_ENCODING_MP4A,            CODEC_ID_AAC},
   {MMAL_ENCODING_MPGA,            CODEC_ID_MP3},
   {MMAL_ENCODING_ALAW,            CODEC_ID_PCM_ALAW},
   {MMAL_ENCODING_MULAW,           CODEC_ID_PCM_MULAW},
   {MMAL_ENCODING_ADPCM_MS,        CODEC_ID_ADPCM_MS},
   {MMAL_ENCODING_ADPCM_IMA_MS,    CODEC_ID_ADPCM_IMA_WAV},
   {MMAL_ENCODING_ADPCM_SWF,       CODEC_ID_ADPCM_SWF},
   {MMAL_ENCODING_WMA1,            CODEC_ID_WMAV1},
   {MMAL_ENCODING_WMA2,            CODEC_ID_WMAV2},
   {MMAL_ENCODING_WMAP,            CODEC_ID_WMAPRO},
   {MMAL_ENCODING_WMAL,            CODEC_ID_NONE},
   {MMAL_ENCODING_WMAV,            CODEC_ID_WMAVOICE},
   {MMAL_ENCODING_AMRNB,           CODEC_ID_AMR_NB},
   {MMAL_ENCODING_AMRWB,           CODEC_ID_AMR_WB},
   {MMAL_ENCODING_AMRWBP,          CODEC_ID_NONE},
   {MMAL_ENCODING_AC3,             CODEC_ID_AC3},
   {MMAL_ENCODING_EAC3,            CODEC_ID_EAC3},
   {MMAL_ENCODING_DTS,             CODEC_ID_DTS},
   {MMAL_ENCODING_MLP,             CODEC_ID_MLP},
   {MMAL_ENCODING_FLAC,            CODEC_ID_FLAC},
   {MMAL_ENCODING_VORBIS,          CODEC_ID_VORBIS},
   {MMAL_ENCODING_SPEEX,           CODEC_ID_SPEEX},
   {MMAL_ENCODING_NELLYMOSER,      CODEC_ID_NELLYMOSER},
   {MMAL_ENCODING_QCELP,           CODEC_ID_QCELP},

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
   enum AVSampleFormat samplefmt;
   unsigned int sample_size;
} samplefmt_to_encoding_table[] =
{
   {MMAL_ENCODING_PCM_UNSIGNED, AV_SAMPLE_FMT_U8, 8},
   {MMAL_ENCODING_PCM_SIGNED,   AV_SAMPLE_FMT_S16, 16},
   {MMAL_ENCODING_PCM_SIGNED,   AV_SAMPLE_FMT_S32, 32},
   {MMAL_ENCODING_PCM_FLOAT,    AV_SAMPLE_FMT_FLT, 32},
   {MMAL_ENCODING_PCM_FLOAT,    AV_SAMPLE_FMT_DBL, 64},
   {MMAL_ENCODING_UNKNOWN,      AV_SAMPLE_FMT_NONE, 1}
};

static uint32_t samplefmt_to_encoding(enum AVSampleFormat samplefmt)
{
   unsigned int i;
   for(i = 0; samplefmt_to_encoding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(samplefmt_to_encoding_table[i].samplefmt == samplefmt) break;
   return samplefmt_to_encoding_table[i].encoding;
}

static unsigned int samplefmt_to_sample_size(enum AVSampleFormat samplefmt)
{
   unsigned int i;
   for(i = 0; samplefmt_to_encoding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(samplefmt_to_encoding_table[i].samplefmt == samplefmt) break;
   return samplefmt_to_encoding_table[i].sample_size;
}

MMAL_CONSTRUCTOR(mmal_register_component_avcodec_audio);
void mmal_register_component_avcodec_audio(void)
{
   avcodec_init();
   avcodec_register_all();

   mmal_component_supplier_register("avcodec", mmal_component_create_avcodec);
}
