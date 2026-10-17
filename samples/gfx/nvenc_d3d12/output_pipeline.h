/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <formats/rac3.h>
#include "../../../record/record_gpu_output_d3d12.h"
static FILE *nve_pipeline_raw;
static unsigned *nve_pipeline_received;
static int nve_pipeline_worker_step(record_gpu_worker_d3d12_t *worker, nvenc_d3d12_packet_t *packet)
{
   int result = record_gpu_worker_d3d12_step(worker, packet);
   if (result == RECORD_GPU_WORKER_PACKET)
   {
      if (packet->timestamp != *nve_pipeline_received || packet->duration != 1
            || fwrite(packet->data, 1, packet->size, nve_pipeline_raw) != packet->size)
         return RECORD_GPU_WORKER_ERROR;
      (*nve_pipeline_received)++;
   }
   return result;
}
#define record_gpu_worker_d3d12_step nve_pipeline_worker_step
#include "../../../record/record_gpu_output_d3d12.c"
#undef record_gpu_worker_d3d12_step
#include "../../../record/record_file_win32.c"
#include "../../../record/record_gpu_session_d3d12.c"
#include "../../../record/record_gpu_dispatch_d3d12.c"
struct nve_pcm_producer
{
   record_gpu_dispatch_d3d12_t *dispatch;
   record_gpu_output_d3d12_t *output;
   const int16_t *samples;
   unsigned frames, channels, made;
   retro_atomic_int_t stop;
   HANDLE notify;
};
static DWORD WINAPI nve_pcm_produce(void *opaque)
{
   struct nve_pcm_producer *p = (struct nve_pcm_producer*)opaque;
   unsigned idle = 0;
   while (p->made < p->frames && idle < 10000 && !retro_atomic_load_acquire_int(&p->stop))
   {
      unsigned n = 1 + p->made * 17u % 1023;
      int result;
      if (n > p->frames - p->made) n = p->frames - p->made;
      result = p->dispatch
         ? record_gpu_dispatch_d3d12_audio(p->dispatch, p->samples + p->made * p->channels, n)
         : record_gpu_output_d3d12_audio(p->output, p->samples + p->made * p->channels, n);
      if (result == RECORD_GPU_OUTPUT_ERROR) break;
      if (result == RECORD_GPU_OUTPUT_BUSY) { Sleep(1); idle++; }
      else { p->made += n; idle = 0; if (p->notify) SetEvent(p->notify); }
   }
   if (p->dispatch) record_gpu_dispatch_d3d12_audio_finish(p->dispatch);
   else record_gpu_output_d3d12_audio_finish(p->output);
   if (p->notify) SetEvent(p->notify);
   return 0;
}
static bool nve_pipeline_run(record_gpu_worker_d3d12_t *worker, nvenc_d3d12_t *encoder,
      const record_mp4_config_t *video, const char *path, FILE *raw, unsigned *received,
      ID3D12CommandQueue *queue, ID3D12Fence *ready, uint64_t *value, bool capturing,
      unsigned frames, HANDLE notify, const nvenc_d3d12_config_t *dispatch_config,
      struct nve_producer *video_producer)
{
   record_gpu_output_config_t config;
   record_gpu_output_d3d12_t *output = NULL;
   struct nve_pcm_producer producer;
   record_gpu_dispatch_d3d12_t *dispatch = NULL;
   HANDLE thread = NULL, video_thread = NULL;
   int16_t *samples = NULL;
   float *pcm = NULL;
   uint8_t *packet = NULL, *headers = NULL;
   char *movie = NULL, *sound = NULL;
   FILE *audio = NULL;
   rac3_encoder_t *reference = NULL;
   unsigned i, j, idle = 0, channels, total, rate;
   size_t length = strlen(path), size, headers_size;
   int result = RECORD_GPU_OUTPUT_BUSY;
   bool signalled = false, ok = false;
   const char *setting = getenv("NVENC_TEST_AUDIO_RATE");
   const char *ownership = getenv("NVENC_TEST_CAPTURE_OWNED");
   bool owned_capture = ownership && atoi(ownership) != 0;
   rate = setting ? (unsigned)atoi(setting) : 48000;
   channels = rate == 44100 ? 1 : rate == 32000 ? 6 : 2;
   memset(&config, 0, sizeof(config)); memset(&producer, 0, sizeof(producer));
   config.video = *video; config.audio_rate = rate; config.audio_kbps = 192;
   config.audio_layout = channels == 1 ? 4 : channels == 6 ? 0x60f : 3;
   config.audio_fifo_frames = 3072;
   total = frames * rate / 60;
   movie = (char*)malloc(length + 9); sound = (char*)malloc(length + 9);
   samples = (int16_t*)malloc(total * channels * sizeof(*samples));
   pcm = (float*)calloc(1536 * channels, sizeof(*pcm)); packet = (uint8_t*)malloc(RAC3_MAX_FRAME_BYTES);
   if (!movie || !sound || !samples || !pcm || !packet) goto done;
   memcpy(movie, path, length); memcpy(movie + length, ".av.mp4", 8);
   memcpy(sound, path, length); memcpy(sound + length, ".av.ac3", 8);
   for (i = 0; i < total; i++)
      for (j = 0; j < channels; j++) samples[i * channels + j] = (int16_t)((int)((i * 37 + j * 1009) % 60001) - 30000);
   reference = rac3_encoder_new(rate, config.audio_layout, 192); if (!reference) goto done;
   audio = fopen(sound, "wb"); if (!audio) goto done;
   for (i = 0; i < total; i += 1536)
   {
      unsigned n = total - i; if (n > 1536) n = 1536;
      memset(pcm, 0, 1536 * channels * sizeof(*pcm));
      for (j = 0; j < n * channels; j++) pcm[j] = samples[i * channels + j] * (1.0f / 32768.0f);
      size = rac3_encode_frame(reference, pcm, packet, RAC3_MAX_FRAME_BYTES);
      if (!size || fwrite(packet, 1, size, audio) != size) goto done;
   }
   if (fclose(audio)) { audio = NULL; goto done; } audio = NULL;
   nve_pipeline_raw = raw; nve_pipeline_received = received;
   if (dispatch_config)
   {
      dispatch = owned_capture
         ? record_gpu_dispatch_d3d12_new_owned(video_producer->capture, dispatch_config, &config, movie)
         : record_gpu_dispatch_d3d12_new(video_producer->capture, dispatch_config, &config, movie);
      if (!dispatch) goto done;
      video_producer->dispatch = dispatch;
      retro_atomic_int_init(&video_producer->stop, 0);
      video_thread = CreateThread(NULL, 0, nve_produce, video_producer, 0, NULL);
      if (!video_thread) goto done;
   }
   else
   {
      headers = (uint8_t*)malloc(65536); if (!headers) goto done;
      if (!nvenc_d3d12_headers(encoder, headers, 65536, &headers_size)) goto done;
      config.video.headers = headers; config.video.headers_size = headers_size;
      output = record_gpu_output_d3d12_new(worker, &config, movie); if (!output) goto done;
      free(headers); headers = NULL;
   }
   producer.dispatch = dispatch;
   producer.notify = notify;
   producer.output = output; producer.samples = samples; producer.frames = total; producer.channels = channels;
   retro_atomic_int_init(&producer.stop, 0);
   thread = CreateThread(NULL, 0, nve_pcm_produce, &producer, 0, NULL); if (!thread) goto done;
   while (idle < 10000)
   {
      if (dispatch)
      {
         enum record_gpu_session_error error;
         uint32_t file_error, system_error;
         enum record_gpu_dispatch_state state = record_gpu_dispatch_d3d12_status(
               dispatch, &error, &file_error, &system_error);
         if (state == RECORD_GPU_DISPATCH_DONE || state == RECORD_GPU_DISPATCH_FAILED)
         {
            result = state == RECORD_GPU_DISPATCH_DONE ? RECORD_GPU_OUTPUT_END : RECORD_GPU_OUTPUT_ERROR;
            if (result == RECORD_GPU_OUTPUT_ERROR)
               fprintf(stderr, "dispatcher failed: session %u, file %u, system %u\n",
                     (unsigned)error, (unsigned)file_error, (unsigned)system_error);
            break;
         }
         Sleep(1); idle++;
         continue;
      }
      result = record_gpu_output_d3d12_step(output);
      if (result == RECORD_GPU_OUTPUT_ERROR || result == RECORD_GPU_OUTPUT_END) break;
      if (!signalled && encoder->count == 4)
      {
         if (FAILED(capturing ? ID3D12Fence_Signal(ready, ++*value)
                  : ID3D12CommandQueue_Signal(queue, ready, ++*value))) goto done;
         signalled = true;
      }
      if (result == RECORD_GPU_OUTPUT_BUSY)
      {
         HANDLE handles[2];
         DWORD waited;
         handles[0] = notify;
         handles[1] = record_gpu_output_d3d12_wait_handle(output);
         waited = WaitForMultipleObjects(handles[1] ? 2 : 1, handles, FALSE,
               handles[1] ? 10000 : 1);
         if (waited == WAIT_FAILED || (handles[1] && waited == WAIT_TIMEOUT)) goto done;
         idle++;
      }
      else idle = 0;
   }
   ok = result == RECORD_GPU_OUTPUT_END;
done:
   if (dispatch && !ok)
   {
      record_gpu_dispatch_d3d12_cancel(dispatch);
      ID3D12Fence_Signal(ready, 1);
   }
   if (video_thread)
   {
      retro_atomic_store_release_int(&video_producer->stop, 1);
      if (WaitForSingleObject(video_thread, 10000) != WAIT_OBJECT_0) abort();
      CloseHandle(video_thread);
      ok = ok && video_producer->made == frames;
   }
   else if (dispatch) record_gpu_dispatch_d3d12_video_finish(dispatch);
   if (thread)
   {
      retro_atomic_store_release_int(&producer.stop, 1);
      if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0) abort();
      CloseHandle(thread);
      ok = ok && producer.made == total;
   }
   if (dispatch)
   {
      if (!thread) record_gpu_dispatch_d3d12_audio_finish(dispatch);
      for (i = 0; i < 10000 && !record_gpu_dispatch_d3d12_free(dispatch); i++) Sleep(1);
      if (i == 10000) abort();
      video_producer->dispatch = NULL;
      if (owned_capture) video_producer->capture = NULL;
   }
   ok = ok && *received == frames;
   if (output)
   {
      record_gpu_output_d3d12_audio_finish(output);
      if (!ok)
      {
         record_gpu_output_d3d12_abort(output);
         for (i = 0; i < 10000 && record_gpu_output_d3d12_step(output) == RECORD_GPU_OUTPUT_BUSY; i++) Sleep(1);
      }
      if (!record_gpu_output_d3d12_free(output)) abort();
   }
   nve_pipeline_raw = NULL; nve_pipeline_received = NULL;
   if (audio) fclose(audio);
   rac3_encoder_free(reference); free(movie); free(sound); free(samples); free(pcm); free(packet); free(headers);
   if (ok) printf("PASS: %s + concurrent PCM + native MP4/file output, %u Hz/%u ch\n",
         dispatch_config ? "GPU dispatcher/session" : "GPU worker", rate, channels);
   return ok;
}
