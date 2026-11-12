/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __APPLE__
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
   fputs("Native VT/AAC RTMP acceptance requires Apple hardware\n", stderr);
   return 77;
}
#else
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <features/features_cpu.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include "../../../gfx/common/videotoolbox_encode.h"
#include "../../../record/record_stream_vt.h"
#include "../../../record/record_stream_output_vt.h"
#include "../videotoolbox_encode/native_packet.h"

#define FRAMES 24
#define CAPACITY (4 * 1024 * 1024)
typedef struct native_capture
{
   videotoolbox_encode_t *encoder;
   CVPixelBufferRef images[2];
   FILE *coded;
   uint8_t *staging;
   unsigned outputs, releases;
   int length;
   bool written;
} native_capture_t;

typedef struct native_stream_owner
{
   record_stream_vt_config_t config;
   record_avfoundation_session_output_ops_t callbacks;
   native_capture_t *capture;
   record_stream_output_vt_t *output;
   record_at_audio_t *audio;
   record_at_resample_t *converter;
   net_rtmp_client_t *client;
   CMVideoFormatDescriptionRef format;
   const uint64_t *accepted;
   uint64_t prebuffer,frames,source_frames;
   unsigned leading,trailing;
   bool adopted,complete,stats,retired;
} native_stream_owner_t;

static void *native_owned_open(void *context,void *sample,
      record_gpu_worker_vt_t *video,record_audio_queue_t *audio)
{
   native_stream_owner_t *o=(native_stream_owner_t*)context;
   assert(!o->format && !o->output);
   o->prebuffer=*o->accepted;
   o->format=(CMVideoFormatDescriptionRef)CFRetain(
         CMSampleBufferGetFormatDescription((CMSampleBufferRef)sample));
   o->config.format=(void*)o->format;
   o->output=record_stream_output_vt_new(video,o->audio,o->converter,o->client,&o->config,audio);
   if (!o->output) { CFRelease(o->format); o->format=NULL; return NULL; }
   o->adopted=true; o->client=NULL;
   return o;
}
static int native_owned_step(void *context)
{
   native_stream_owner_t *o=(native_stream_owner_t*)context;
   int result=o->callbacks.step(o->output);
   if (result==RECORD_AVF_WORKER_END) o->complete=true;
   return result;
}
static bool native_owned_cancel(void *context)
{
   native_stream_owner_t *o=(native_stream_owner_t*)context;
   return o->callbacks.cancel(o->output);
}
static bool native_owned_free_output(void *context)
{
   native_stream_owner_t *o=(native_stream_owner_t*)context;
   if (o->complete && !o->stats)
   {
      assert(record_at_audio_timing(o->audio,&o->leading,&o->trailing,&o->frames));
      if (o->converter)
      {
         uint64_t produced;
         assert(record_at_resample_timing(o->converter,&o->source_frames,&produced));
         assert(produced==o->frames);
      }
      else o->source_frames=o->frames;
      o->stats=true;
   }
   if (!o->callbacks.free_output(o->output)) return false;
   o->output=NULL; o->audio=NULL; o->converter=NULL;
   return true;
}
static bool native_owned_retry(const void *context)
{
   const native_stream_owner_t *o=(const native_stream_owner_t*)context;
   return o->callbacks.retry_needed(o->output);
}
static bool native_owned_network(const void *context,int *fd,bool *read,bool *write)
{
   const native_stream_owner_t *o=(const native_stream_owner_t*)context;
   return o->callbacks.network(o->output,fd,read,write);
}
static void native_owned_encoder_destroy(void *context)
{
   native_stream_owner_t *o=(native_stream_owner_t*)context;
   assert(!o->output);
   if (o->complete) assert(o->capture->encoder->drained && !o->capture->encoder->count);
   videotoolbox_encode_cancel(o->capture->encoder); o->capture->encoder=NULL;
}
static bool native_owned_context_destroy(void *context)
{
   native_stream_owner_t *o=(native_stream_owner_t*)context;
   assert(!o->capture->encoder && !o->output && !o->retired);
   if (!o->adopted)
   {
      if (o->converter)
      {
         if (!record_at_resample_cancel(o->converter) || !record_at_resample_free(o->converter)) return false;
         o->converter=NULL;
      }
      if (o->audio)
      {
         if (!record_at_audio_cancel(o->audio) || !record_at_audio_free(o->audio)) return false;
         o->audio=NULL;
      }
      if (o->client)
      {
         net_rtmp_client_cancel(o->client);
         if (!net_rtmp_client_free(o->client)) return false;
         o->client=NULL;
      }
   }
   if (o->format) { CFRelease(o->format); o->format=NULL; }
   o->retired=true; return true;
}

static bool resolve_dispatch(void (*work)(void*), void *job, void *data)
{
   sthread_t **thread = (sthread_t**)data;
   *thread = sthread_create(work, job);
   return *thread != NULL;
}

/* Only the standalone fixture waits; the production stream step never does. */
static void wait_network(net_rtmp_client_view_t *view)
{
   if (!view->again)
   {
      if (view->fd >= 0)
         socket_wait(view->fd, &view->read, &view->write, 1);
      else
         retro_sleep(1);
   }
}

static int ready(void *data, unsigned slot)
{
   native_capture_t *capture = (native_capture_t*)data;
   return slot < 2 && capture->images[slot] ? 1 : -1;
}

static void *buffer(void *data, unsigned slot)
{
   native_capture_t *capture = (native_capture_t*)data;
   return slot < 2 ? capture->images[slot] : NULL;
}

static bool release(void *data, unsigned slot)
{
   native_capture_t *capture = (native_capture_t*)data;
   assert(slot < 2);
   capture->releases++;
   return true;
}

static int submit(void *data, unsigned slot, void *image, uint64_t pts, uint64_t duration)
{
   native_capture_t *capture = (native_capture_t*)data;
   return videotoolbox_encode_submit(capture->encoder, slot,
         (CVPixelBufferRef)image, (int64_t)pts, (int64_t)duration);
}

static bool write_packet(native_capture_t *capture, CMSampleBufferRef sample)
{
   CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample);
   size_t size = block ? CMBlockBufferGetDataLength(block) : 0;
   size_t contiguous = 0, total = 0, at = 0, n;
   char *pointer = NULL;
   const uint8_t *bytes;
   unsigned i;
   if (!size || size > CAPACITY)
      return false;
   if (!capture->outputs && !headers(capture->coded,
            CMSampleBufferGetFormatDescription(sample),
            capture->encoder->config.hevc, &capture->length))
      return false;
   if (!CMBlockBufferGetDataPointer(block, 0, &contiguous, &total, &pointer)
         && contiguous == size && total == size)
      bytes = (const uint8_t*)pointer;
   else
   {
      if (CMBlockBufferCopyDataBytes(block, 0, size, capture->staging))
         return false;
      bytes = capture->staging;
   }
   while (at < size)
   {
      if (size - at < (unsigned)capture->length)
         return false;
      n = 0;
      for (i = 0; i < (unsigned)capture->length; i++)
         n = (n << 8) | bytes[at++];
      if (!n || n > size - at || !prefix(capture->coded, bytes + at, n))
         return false;
      at += n;
   }
   capture->outputs++;
   return true;
}

static int peek(void *data, unsigned *slot, void **sample)
{
   native_capture_t *capture = (native_capture_t*)data;
   const videotoolbox_encode_slot_t *output;
   if (capture->encoder->error)
      return -1;
   output = videotoolbox_encode_peek(capture->encoder, slot);
   if (!output)
      return 0;
   if (output->status || !output->sample || (output->flags & kVTEncodeInfo_FrameDropped))
      return -1;
   if (!capture->written && !write_packet(capture, output->sample))
      return -1;
   capture->written = true;
   *sample = (void*)output->sample;
   return 1;
}

static bool ack(void *data)
{
   native_capture_t *capture = (native_capture_t*)data;
   if (!videotoolbox_encode_ack(capture->encoder))
      return false;
   capture->written = false;
   return true;
}

static int finish(void *data)
{
   native_capture_t *capture = (native_capture_t*)data;
   return videotoolbox_encode_finish(capture->encoder) ? 1 : -1;
}

/* Test input only. Shipping capture writes the compatible planes on Metal. */
static void fill(CVPixelBufferRef image, unsigned frame, unsigned depth)
{
   unsigned plane, y, x, value;
   size_t stride;
   uint8_t *row;
   assert(!CVPixelBufferLockBaseAddress(image, 0));
   for (plane = 0; plane < 2; plane++)
   {
      row = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(image, plane);
      stride = CVPixelBufferGetBytesPerRowOfPlane(image, plane);
      for (y = 0; y < (plane ? 24u : 48u); y++)
         for (x = 0; x < 64; x++)
         {
            value = plane ? (x & 1 ? 150 - frame : 100 + frame) : 64 + frame * 4;
            if (depth == 10)
               ((uint16_t*)(row + y * stride))[x] = (uint16_t)(value << 8);
            else
               row[y * stride + x] = (uint8_t)value;
         }
   }
   assert(!CVPixelBufferUnlockBaseAddress(image, 0));
   assert(!CVPixelBufferFillExtendedPixels(image));
}

static void produce_audio(record_stream_vt_t *stream, record_audio_queue_t *source,
      record_avfoundation_session_t *session,
      int16_t *pcm, double rate, unsigned channels, uint64_t frames,
      uint64_t *accepted, bool *closed)
{
   size_t batch;
   unsigned i;
   int result;
   if (*accepted < frames)
   {
      batch = (size_t)(frames - *accepted);
      if (batch > 1024) batch = 1024;
      for (i = 0; i < batch * channels; i++)
         pcm[i] = (int16_t)(sin((*accepted + i / channels) * 6.283185307179586
                  * (i % channels ? 1499 : 997) / rate) * 12000);
      result = session ? record_avfoundation_session_audio(session,pcm,batch,*accepted)
         :source ? record_audio_queue_push(source, pcm, batch, *accepted)
         : record_stream_vt_audio(stream, pcm, batch);
      assert(result >= 0);
      if (source ? result == RECORD_AUDIO_OK : result == RECORD_STREAM_VT_PROGRESS)
         *accepted += batch;
   }
   if (*accepted == frames && !*closed)
   {
      assert(session ? record_avfoundation_session_audio_finish(session)
            :source ? record_audio_queue_finish(source) : record_stream_vt_audio_finish(stream));
      *closed = true;
   }
}

int main(int argc, char **argv)
{
   native_capture_t capture;
   videotoolbox_encode_config_t video_config;
   record_stream_vt_config_t stream_config;
   record_gpu_worker_vt_ops_t ops;
   net_rtmp_client_config_t network_config;
   net_rtmp_codec_capability_t video = {NET_RTMP_CODEC_ENCODE, "avc1"};
   net_rtmp_codec_capability_t audio_cap = {NET_RTMP_CODEC_ENCODE, "mp4a"};
   net_rtmp_codec_caps_t caps;
   record_stream_vt_wait_t wait;
   net_rtmp_client_view_t view;
   enum net_rtmp_client_state state = NET_RTMP_CLIENT_RESOLVING, previous;
   record_gpu_queue_t *queue;
   record_gpu_worker_vt_t *worker;
   record_at_audio_t *audio;
   record_stream_vt_t *stream = NULL;
   record_audio_queue_t *source = NULL;
   record_at_resample_t *resampler = NULL;
   record_avfoundation_session_t *owned_session = NULL;
   native_stream_owner_t *owner = NULL;
   CMVideoFormatDescriptionRef format = NULL;
   CVPixelBufferPoolRef pool;
   sthread_t *resolver = NULL;
   retro_time_t deadline;
   record_gpu_frame_t frame;
   void *sample = NULL;
   int16_t *pcm;
   uint8_t *random;
   char *pem = NULL;
   const char *ca = getenv("RTMPS_CA");
   const char *source_rate_text = getenv("VT_STREAM_SOURCE_RATE");
   double source_rate;
   int64_t pem_size = 0;
   uint64_t accepted = 0, frames, source_frames, prebuffer = 0;
   unsigned leading, trailing, rate, channels, i, count = 0, slot;
   OSStatus status;
   int result;
   bool video_closed = false, audio_closed = false;
   bool owned = getenv("VT_STREAM_OWNED_OUTPUT") != NULL;
   bool queued = false;
   assert(argc == 7 && network_init());
   rate = (unsigned)atoi(argv[3]);
   channels = (unsigned)atoi(argv[4]);
   assert((rate == 44100 || rate == 48000) && channels && channels <= 2);
   source_rate = source_rate_text ? strtod(source_rate_text, NULL) : rate;
   assert(source_rate >= 8000.0 && source_rate <= 192000.0);
   memset(&capture, 0, sizeof(capture));
   memset(&video_config, 0, sizeof(video_config));
   video_config.width = 64; video_config.height = 48; video_config.slots = 2;
   video_config.bitrate = 1000000; video_config.key_interval = 6;
   video_config.timescale = 60000;
   video_config.hevc = atoi(argv[1]) != 0;
   video_config.bit_depth = (unsigned)atoi(argv[2]);
   capture.encoder = videotoolbox_encode_new(&video_config, &status);
   if (!capture.encoder)
   {
      fprintf(stderr, "Native VT setup failed: %d\n", (int)status);
      return status == kVTCouldNotFindVideoEncoderErr ? 77 : 1;
   }
   audio = record_at_audio_new(rate, channels, 128000);
   assert(audio && record_at_audio_timing(audio, &leading, &trailing, &frames) && !frames);
   if (ca)
   {
      assert(filestream_read_file(ca, (void**)&pem, &pem_size) && pem_size > 0);
      ssl_socket_retro_set_trust_pem(pem, (size_t)pem_size);
   }
   random = (uint8_t*)malloc(1528);
   assert(random);
   for (i = 0; i < 1528; i++) random[i] = (uint8_t)(i * 13 + 1);
   memset(&network_config, 0, sizeof(network_config));
   network_config.url = argv[5]; network_config.random = random;
   network_config.dispatch = resolve_dispatch; network_config.userdata = &resolver;
   network_config.max_reply = 65536; network_config.streams = 8;
   if (video_config.hevc) memcpy(video.fourcc, "hvc1", 4);
   caps.video = &video; caps.audio = &audio_cap;
   caps.video_count = caps.audio_count = 1;
   memset(&wait, 0, sizeof(wait));
   /* Borrowed here by the stream after setup, freed only after EOS retirement. */
   {
      net_rtmp_client_t *client = net_rtmp_client_new_caps(&network_config, &caps);
      assert(client);
      free(random);
      deadline = cpu_features_get_time_usec() + 15000000;
      do
      {
         assert(cpu_features_get_time_usec() < deadline);
         previous = state;
         state = net_rtmp_client_step(client, 0, &view);
         if (state == NET_RTMP_CLIENT_FAILED)
         {
            assert(previous == NET_RTMP_CLIENT_RESOLVING || previous == NET_RTMP_CLIENT_CONNECTING);
            assert(net_rtmp_client_free(client));
            sthread_join(resolver);
            assert(record_at_audio_cancel(audio) && record_at_audio_free(audio));
            videotoolbox_encode_cancel(capture.encoder);
            ssl_socket_retro_set_trust_pem(NULL, 0); free(pem);
            return 78;
         }
         wait_network(&view);
      } while (state != NET_RTMP_CLIENT_READY);
      ssl_socket_retro_set_trust_pem(NULL, 0); free(pem);
      capture.coded = fopen(argv[6], "wb");
      capture.staging = (uint8_t*)malloc(CAPACITY);
      pcm = (int16_t*)malloc(1024 * channels * sizeof(*pcm));
      assert(capture.coded && capture.staging && pcm);
      if (getenv("VT_STREAM_SOURCE_QUEUE") || source_rate != rate || owned)
      {
         source = record_audio_queue_new(channels, 1024, 2048 * channels * 2 + 32);
         assert(source);
         if (source_rate != rate)
         {
            resampler = record_at_resample_new(source, source_rate, rate);
            assert(resampler);
         }
      }
      queue = record_gpu_queue_new(2);
      queued = source != NULL;
      assert(queue);
      pool = VTCompressionSessionGetPixelBufferPool(capture.encoder->session);
      assert(pool);
      for (i = 0; i < 2; i++)
         assert(!CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &capture.images[i]));
      ops.ready = ready; ops.buffer = buffer; ops.release = release;
      ops.submit = submit; ops.peek = peek; ops.ack = ack; ops.finish = finish;
      worker = record_gpu_worker_vt_new(queue, &capture, &capture, &ops);
      assert(worker);
      frames = (uint64_t)ceil((double)FRAMES * 1001 * source_rate / 60000.0);
      memset(&stream_config, 0, sizeof(stream_config));
      stream_config.max_packet_size = CAPACITY;
      stream_config.video_timescale = 60000;
      stream_config.bit_depth = video_config.bit_depth;
      stream_config.audio_rate = rate; stream_config.channels = channels;
      stream_config.audio_fifo_frames = source ? 0 : 2048;
      stream_config.audio_epoch_frames = leading;
      stream_config.video_epoch_ticks = ((uint64_t)leading * 60000 + rate - 1) / rate;
      stream_config.color.primaries = 1; stream_config.color.transfer = 1;
      stream_config.color.matrix = 1; stream_config.color.range = 1;
      if (owned)
      {
         record_avfoundation_session_output_ops_t output_ops;
         owner=(native_stream_owner_t*)calloc(1,sizeof(*owner)); assert(owner);
         owner->config=stream_config; owner->capture=&capture; owner->accepted=&accepted;
         owner->audio=audio; owner->converter=resampler; owner->client=client;
         assert(record_stream_output_vt_set_ops(&owner->callbacks));
         memset(&output_ops,0,sizeof(output_ops));
         output_ops.open=native_owned_open; output_ops.step=native_owned_step;
         output_ops.cancel=native_owned_cancel; output_ops.free_output=native_owned_free_output;
         output_ops.retry_needed=native_owned_retry; output_ops.network=native_owned_network;
         output_ops.destroy_encoder=native_owned_encoder_destroy;
         output_ops.destroy_context=native_owned_context_destroy;
         owned_session=record_avfoundation_session_new_output(queue,worker,source,owner,&output_ops);
         assert(owned_session);
      }
      memset(&frame, 0, sizeof(frame));
      deadline = cpu_features_get_time_usec() + 20000000;
      do
      {
         assert(cpu_features_get_time_usec() < deadline);
         if (queued && !audio_closed)
            produce_audio(stream, source, owned_session, pcm, source_rate, channels, frames, &accepted, &audio_closed);
         if (count < FRAMES && record_gpu_queue_acquire(queue, &slot) == RECORD_GPU_QUEUE_OK)
         {
            fill(capture.images[slot], count, video_config.bit_depth);
            frame.slot = slot; frame.timestamp = (uint64_t)count * 1001; frame.duration = 1001;
            assert(record_gpu_queue_submit(queue, &frame));
            count++;
         }
         if (count == FRAMES && !video_closed)
         {
            assert(owned_session ? record_avfoundation_session_video_finish(owned_session)
                  : record_gpu_queue_finish(queue));
            video_closed = true;
         }
         if (!owned_session && !stream)
         {
            result = record_gpu_worker_vt_step(worker, &sample);
            assert(result >= 0 && result != RECORD_GPU_VT_END);
            if (result != RECORD_GPU_VT_PACKET)
            {
               retro_sleep(1);
               continue;
            }
            prebuffer = accepted;
            format = (CMVideoFormatDescriptionRef)CFRetain(
                  CMSampleBufferGetFormatDescription((CMSampleBufferRef)sample));
            stream_config.format = (void*)format;
            stream = resampler ? record_stream_vt_new_resampled(worker, audio, client, &stream_config, resampler)
               : source ? record_stream_vt_new_queued(worker, audio, client, &stream_config, source)
               : record_stream_vt_new(worker, audio, client, &stream_config);
            assert(stream);
         }
         if (!queued)
            produce_audio(stream, NULL, NULL, pcm, rate, channels, frames, &accepted, &audio_closed);
         result = owned_session ? record_avfoundation_session_step(owned_session)
            : record_stream_vt_step(stream, 0, &wait);
         if (result == RECORD_STREAM_VT_ERROR)
            fprintf(stderr, "Native stream failure: %d\n", owned_session
                  ?(int)record_avfoundation_session_error(owned_session):(int)record_stream_vt_error(stream));
         assert(result >= 0);
         if (result == RECORD_STREAM_VT_BUSY)
         {
            if (owned_session)
            {
               wait.network.again=false;
               record_avfoundation_session_network(owned_session,&wait.network.fd,
                     &wait.network.read,&wait.network.write);
            }
            wait_network(&wait.network);
         }
      } while (result != (owned_session ? (int)RECORD_AVF_WORKER_END : (int)RECORD_STREAM_VT_END));
      assert(count == FRAMES && capture.outputs == FRAMES && capture.releases == FRAMES);
      if (owned_session)
      {
         assert(owner->complete && owner->stats && owner->retired && !owner->output && !capture.encoder);
         assert(owner->source_frames==frames);
         source_frames=owner->frames; leading=owner->leading; trailing=owner->trailing; prebuffer=owner->prebuffer;
         assert(fabs((double)source_frames - frames * rate / source_rate) <= 1.0);
         assert(record_avfoundation_session_free(owned_session));
         free(owner);
      }
      else
      {
         assert(record_at_audio_timing(audio, &leading, &trailing, &source_frames));
         if (resampler)
         {
            uint64_t converted_input, converted_output;
            assert(record_at_resample_timing(resampler, &converted_input, &converted_output));
            assert(converted_input == frames && converted_output == source_frames);
            assert(fabs((double)source_frames - frames * rate / source_rate) <= 1.0);
         }
         else assert(source_frames == frames);
         assert(record_stream_vt_free(stream));
         assert(record_at_resample_free(resampler));
         assert(record_audio_queue_free(source));
         assert(record_gpu_worker_vt_free(worker) && record_gpu_queue_free(queue));
         assert(record_at_audio_free(audio));
         assert(videotoolbox_encode_free(capture.encoder));
         assert(net_rtmp_client_free(client));
      }
      sthread_join(resolver);
   }
   for (i = 0; i < 2; i++) CVPixelBufferRelease(capture.images[i]);
   if (format) CFRelease(format);
   assert(!fclose(capture.coded));
   free(capture.staging); free(pcm);
   printf("PASS frames=%u audio=%lu leading=%u trailing=%u prebuffer=%lu owned=%u\n", FRAMES,
         (unsigned long)source_frames, leading, trailing, (unsigned long)prebuffer,owned?1u:0u);
   return 0;
}
#endif
