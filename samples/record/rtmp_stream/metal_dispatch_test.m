/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VT_METAL_NO_MAIN
#include "metal_vt_test.m"
#include "../../../record/record_gpu_dispatch_metal.h"

int main(int argc,char **argv)
{
   assert(argc == 7 && network_init());
   @autoreleasepool
   {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      id<MTLCommandQueue> commands;
      id<MTLCommandBuffer> held = nil;
      record_gpu_dispatch_metal_t *job;
      record_gpu_session_metal_config_t video;
      record_gpu_dispatch_metal_stream_config_t stream;
      record_at_audio_t *oracle;
      enum record_gpu_dispatch_metal_state state;
      enum record_gpu_session_metal_startup startup;
      enum record_avfoundation_session_error error;
      int32_t native;
      int system, result;
      const char *rate_text = getenv("VT_STREAM_SOURCE_RATE");
      const char *ca = getenv("RTMPS_CA");
      char *pem = NULL;
      int64_t pem_size = 0;
      int16_t *pcm;
      unsigned produced = 0, leading, trailing, i;
      uint64_t input = 0, target, ignored, prebuffer = 0;
      size_t batch;
      retro_time_t deadline;
      bool audio_closed = false, video_closed = false;
      bool cancel = getenv("VT_STREAM_CANCEL_OWNED") != NULL;
      if (!device)
         return 77;
      memset(&video,0,sizeof(video));
      memset(&stream,0,sizeof(stream));
      video.width = WIDTH;
      video.height = HEIGHT;
      video.slots = 2;
      video.bitrate = 16000000;
      video.key_interval = 6;
      video.timescale = 60000;
      video.bit_depth = (unsigned)atoi(argv[2]);
      video.hevc = atoi(argv[1]) != 0;
      stream.audio_rate = (unsigned)atoi(argv[3]);
      stream.channels = (unsigned)atoi(argv[4]);
      stream.source_rate = rate_text ? strtod(rate_text,NULL) : stream.audio_rate;
      stream.max_audio_frames = 1024;
      stream.audio_capacity_bytes = 2048*stream.channels*2+32;
      stream.max_packet_size = CAPACITY;
      stream.audio_bitrate = 128000;
      stream.color.primaries = stream.color.transfer = stream.color.matrix = stream.color.range = 1;
      oracle = record_at_audio_new(stream.audio_rate,stream.channels,stream.audio_bitrate);
      assert(oracle && record_at_audio_timing(oracle,&leading,&trailing,&ignored) && !ignored);
      assert(record_at_audio_cancel(oracle) && record_at_audio_free(oracle));
      if (ca)
      {
         assert(filestream_read_file(ca,(void**)&pem,&pem_size) && pem_size > 0);
         ssl_socket_retro_set_trust_pem(pem,(size_t)pem_size);
      }
      commands = [device newCommandQueue];
      pcm = (int16_t*)malloc(1024*stream.channels*sizeof(*pcm));
      assert(commands && pcm);
      job = record_gpu_dispatch_metal_new_stream((__bridge void*)device,&video,&stream,argv[5]);
      assert(job);
      deadline = cpu_features_get_time_usec()+20000000;
      do
      {
         state = record_gpu_dispatch_metal_status(job,&startup,&native,&error,&system);
         assert(cpu_features_get_time_usec() < deadline);
         if (state != RECORD_METAL_DISPATCH_STARTING)
            break;
         retro_sleep(1);
      } while (true);
      if (state != RECORD_METAL_DISPATCH_READY)
      {
         fprintf(stderr,"dispatcher startup=%d native=%d error=%d system=%d\n",
               (int)startup,(int)native,(int)error,system);
         assert(record_gpu_dispatch_metal_video_finish(job));
         assert(record_gpu_dispatch_metal_audio_finish(job));
         while (!record_gpu_dispatch_metal_free(job))
         {
            assert(cpu_features_get_time_usec() < deadline);
            retro_sleep(1);
         }
         ssl_socket_retro_set_trust_pem(NULL,0);
         free(pem);
         free(pcm);
         RARCH_RELEASE(commands);
         RARCH_RELEASE(device);
         return 78;
      }
      ssl_socket_retro_set_trust_pem(NULL,0);
      free(pem);
      target = (uint64_t)ceil((double)FRAMES*1001*stream.source_rate/60000.0);
      deadline = cpu_features_get_time_usec()+20000000;
      do
      {
         assert(cpu_features_get_time_usec() < deadline);
         if (!audio_closed)
         {
            batch = (size_t)(target-input);
            if (batch > 1024)
               batch = 1024;
            for (i = 0; i < batch*stream.channels; i++)
               pcm[i] = (int16_t)(sin((input+i/stream.channels)*6.283185307179586
                        *(i%stream.channels ? 1499 : 997)/stream.source_rate)*12000);
            result = record_gpu_dispatch_metal_audio(job,pcm,batch,input);
            assert(result >= 0);
            if (result == RECORD_AUDIO_OK)
               input += batch;
            if (input == target)
            {
               assert(record_gpu_dispatch_metal_audio_finish(job));
               audio_closed = true;
            }
         }
         if (!produced)
            prebuffer = input;
         if (!video_closed)
         {
            id<MTLCommandBuffer> buffer = [commands commandBuffer];
            id<MTLTexture> texture = stream_texture(device,produced);
            assert(buffer);
            result = record_gpu_dispatch_metal_video(job,(__bridge void*)buffer,
                  (__bridge void*)texture,2,2,(uint64_t)produced*1001,1001);
            assert(result >= 0);
            if (cancel && result == RECORD_GPU_QUEUE_OK && produced == 1)
               held = buffer;
            else
               [buffer commit];
            RARCH_RELEASE(texture);
            if (result == RECORD_GPU_QUEUE_OK)
            {
               produced++;
               if (cancel && produced == 2)
               {
                  record_gpu_dispatch_metal_cancel(job);
                  assert(record_gpu_dispatch_metal_video_finish(job));
                  assert(record_gpu_dispatch_metal_audio_finish(job));
                  video_closed = audio_closed = true;
               }
               else if (produced == FRAMES)
               {
                  assert(record_gpu_dispatch_metal_video_finish(job));
                  video_closed = true;
               }
            }
         }
         if (held && video_closed && audio_closed)
         {
            retro_sleep(20); /* Fixture: leave accepted GPU work uncommitted. */
            assert(!record_gpu_dispatch_metal_free(job));
            assert(record_gpu_dispatch_metal_status(job,NULL,NULL,NULL,NULL) != RECORD_METAL_DISPATCH_FAILED);
            [held commit];
            held = nil;
         }
         state = record_gpu_dispatch_metal_status(job,&startup,&native,&error,&system);
         if ((!cancel && error) || system)
            fprintf(stderr,"dispatcher state=%d error=%d system=%d\n",(int)state,(int)error,system);
         assert(!system && (cancel ? error == RECORD_AVF_SESSION_CANCELLED || !error
                  : !error && state != RECORD_METAL_DISPATCH_FAILED));
         retro_sleep(1);
      } while (state != RECORD_METAL_DISPATCH_DONE && state != RECORD_METAL_DISPATCH_FAILED);
      assert(video_closed && audio_closed && !held);
      assert(cancel ? produced == 2 && state == RECORD_METAL_DISPATCH_FAILED
            && error == RECORD_AVF_SESSION_CANCELLED : produced == FRAMES && input == target);
      assert(record_gpu_dispatch_metal_free(job));
      free(pcm);
      RARCH_RELEASE(commands);
      RARCH_RELEASE(device);
      if (cancel)
         printf("PASS cancelled=1 submitted=%u held=1 owned=1 metal=1 dispatcher=1\n",produced);
      else
         printf("PASS frames=%u input=%lu leading=%u prebuffer=%lu owned=1 metal=1 dispatcher=1 width=%u height=%u\n",
               produced,(unsigned long)input,leading,(unsigned long)prebuffer,WIDTH,HEIGHT);
   }
   return 0;
}
