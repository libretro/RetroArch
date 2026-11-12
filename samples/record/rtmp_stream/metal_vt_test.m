/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main vt_cpu_fixture_main
#include "native_vt_test.c"
#undef main
#include "../videotoolbox_encode/capture_reference.h"

static id<MTLTexture> stream_texture(id<MTLDevice> device,unsigned frame)
{
   MTLTextureDescriptor *descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
      MTLPixelFormatRGBA8Unorm width:WIDTH+4 height:HEIGHT+4 mipmapped:NO];
   id<MTLTexture> texture;
   size_t stride=(WIDTH+4)*4;
   uint8_t *pixels=(uint8_t*)malloc(stride*(HEIGHT+4));
   unsigned x,y;
   assert(pixels);
   descriptor.storageMode=MTLStorageModeShared;
   descriptor.usage=MTLTextureUsageShaderRead;
   texture=[device newTextureWithDescriptor:descriptor];
   assert(texture);
   for (y=0;y<HEIGHT+4;y++)
      for (x=0;x<WIDTH+4;x++)
      {
         uint8_t *pixel=pixels+y*stride+x*4;
         pixel[0]=(uint8_t)((frame*37+64)%192+32);
         pixel[1]=(uint8_t)((frame*53+100)%192+32);
         pixel[2]=(uint8_t)((frame*29+150)%192+32);
         pixel[3]=255;
      }
   [texture replaceRegion:MTLRegionMake2D(0,0,WIDTH+4,HEIGHT+4) mipmapLevel:0
      withBytes:pixels bytesPerRow:stride];
   free(pixels);
   return texture;
}

int main(int argc,char **argv)
{
   assert(argc==7 && network_init());
   @autoreleasepool
   {
      id<MTLDevice> device=MTLCreateSystemDefaultDevice();
      id<MTLCommandQueue> commands;
      record_gpu_session_metal_config_t video;
      record_stream_vt_config_t config;
      record_gpu_capture_metal_t *capture;
      record_gpu_queue_t *queue;
      record_avfoundation_session_t *session;
      record_audio_queue_t *source;
      record_at_audio_t *audio;
      record_at_resample_t *converter=NULL;
      net_rtmp_client_config_t network;
      net_rtmp_client_t *client;
      net_rtmp_client_view_t view;
      net_rtmp_codec_capability_t video_cap={NET_RTMP_CODEC_ENCODE,"avc1"};
      net_rtmp_codec_capability_t audio_cap={NET_RTMP_CODEC_ENCODE,"mp4a"};
      net_rtmp_codec_caps_t caps;
      enum net_rtmp_client_state state,previous;
      enum record_gpu_session_metal_startup stage;
      int32_t status;
      sthread_t *resolver=NULL;
      retro_time_t deadline;
      uint8_t *random;
      int16_t *pcm;
      char *pem=NULL;
      const char *ca=getenv("RTMPS_CA");
      const char *rate_text=getenv("VT_STREAM_SOURCE_RATE");
      int64_t pem_size=0;
      double source_rate;
      uint64_t input,target,prebuffer=0,ignored;
      unsigned rate=(unsigned)atoi(argv[3]),channels=(unsigned)atoi(argv[4]);
      unsigned leading,trailing,produced=0,i;
      bool audio_closed=false,video_closed=false;
      int result=RECORD_AVF_WORKER_BUSY;
      if (!device) return 77;
      source_rate=rate_text?strtod(rate_text,NULL):rate;
      assert((rate==44100 || rate==48000) && channels && channels<=2);
      assert(source_rate>=8000 && source_rate<=192000);
      memset(&video,0,sizeof(video));
      video.width=WIDTH; video.height=HEIGHT; video.slots=2;
      video.bitrate=16000000; video.key_interval=6; video.timescale=60000;
      video.bit_depth=(unsigned)atoi(argv[2]); video.hevc=atoi(argv[1])!=0;
      audio=record_at_audio_new(rate,channels,128000);
      source=record_audio_queue_new(channels,1024,2048*channels*2+32);
      assert(audio && source && record_at_audio_timing(audio,&leading,&trailing,&ignored) && !ignored);
      if (source_rate!=rate)
      {
         converter=record_at_resample_new(source,source_rate,rate);
         assert(converter);
      }
      random=(uint8_t*)malloc(1528);
      pcm=(int16_t*)malloc(1024*channels*sizeof(*pcm));
      assert(random && pcm);
      for (i=0;i<1528;i++) random[i]=(uint8_t)(i*13+1);
      memset(&network,0,sizeof(network));
      network.url=argv[5]; network.random=random; network.dispatch=resolve_dispatch;
      network.userdata=&resolver; network.max_reply=65536; network.streams=8;
      if (video.hevc) memcpy(video_cap.fourcc,"hvc1",4);
      caps.video=&video_cap; caps.audio=&audio_cap; caps.video_count=caps.audio_count=1;
      if (ca)
      {
         assert(filestream_read_file(ca,(void**)&pem,&pem_size) && pem_size>0);
         ssl_socket_retro_set_trust_pem(pem,(size_t)pem_size);
      }
      client=net_rtmp_client_new_caps(&network,&caps);
      assert(client);
      free(random);
      state=NET_RTMP_CLIENT_RESOLVING;
      deadline=cpu_features_get_time_usec()+15000000;
      do
      {
         assert(cpu_features_get_time_usec()<deadline);
         previous=state;
         state=net_rtmp_client_step(client,0,&view);
         if (state==NET_RTMP_CLIENT_FAILED)
         {
            assert(previous==NET_RTMP_CLIENT_RESOLVING || previous==NET_RTMP_CLIENT_CONNECTING);
            assert(net_rtmp_client_free(client));
            sthread_join(resolver);
            assert(record_at_resample_cancel(converter) || !converter);
            assert(record_at_resample_free(converter) && record_audio_queue_free(source));
            assert(record_at_audio_cancel(audio) && record_at_audio_free(audio));
            ssl_socket_retro_set_trust_pem(NULL,0); free(pem); free(pcm); RARCH_RELEASE(device);
            return 78;
         }
         wait_network(&view);
      } while (state!=NET_RTMP_CLIENT_READY);
      ssl_socket_retro_set_trust_pem(NULL,0); free(pem);
      memset(&config,0,sizeof(config));
      config.max_packet_size=CAPACITY; config.video_timescale=video.timescale;
      config.bit_depth=video.bit_depth; config.audio_rate=rate; config.channels=channels;
      config.audio_epoch_frames=leading;
      config.video_epoch_ticks=((uint64_t)leading*video.timescale+rate-1)/rate;
      config.color.primaries=config.color.transfer=config.color.matrix=config.color.range=1;
      session=record_stream_session_vt_new_metal((__bridge void*)device,&video,source,audio,
            converter,client,&config,&capture,&queue,&stage,&status);
      if (!session) fprintf(stderr,"Metal factory startup stage=%d native=%d\n",(int)stage,(int)status);
      assert(session && capture && queue && stage==RECORD_METAL_STARTUP_NONE && !status);
      commands=[device newCommandQueue];
      assert(commands);
      input=0;
      target=(uint64_t)ceil((double)FRAMES*1001*source_rate/60000.0);
      deadline=cpu_features_get_time_usec()+20000000;
      do
      {
         assert(cpu_features_get_time_usec()<deadline);
         if (!audio_closed)
            produce_audio(NULL,source,session,pcm,source_rate,channels,target,&input,&audio_closed);
         if (!produced) prebuffer=input;
         if (!video_closed)
         {
            record_gpu_frame_t frame;
            memset(&frame,0,sizeof(frame));
            if (record_gpu_queue_acquire(queue,&frame.slot)==RECORD_GPU_QUEUE_OK)
            {
               id<MTLCommandBuffer> buffer=[commands commandBuffer];
               id<MTLTexture> texture=stream_texture(device,produced);
               assert(buffer && record_gpu_capture_metal_encode(capture,frame.slot,
                        (__bridge void*)buffer,(__bridge void*)texture,2,2)==1);
               frame.timestamp=(uint64_t)produced*1001; frame.duration=1001;
               frame.ready_value=0; frame.keyframe=produced==0;
               assert(record_gpu_queue_submit(queue,&frame));
               [buffer commit]; RARCH_RELEASE(texture);
               if (++produced==FRAMES)
               {
                  assert(record_avfoundation_session_video_finish(session));
                  video_closed=true;
               }
            }
         }
         result=record_avfoundation_session_step(session);
         if (result<0) fprintf(stderr,"Metal stream session error=%d\n",
               (int)record_avfoundation_session_error(session));
         assert(result>=0);
         if (result==RECORD_AVF_WORKER_BUSY)
         {
            view.again=false;
            record_avfoundation_session_network(session,&view.fd,&view.read,&view.write);
            wait_network(&view);
         }
      } while (result!=RECORD_AVF_WORKER_END);
      assert(video_closed && audio_closed && produced==FRAMES && input==target);
      assert(record_avfoundation_session_free(session));
      sthread_join(resolver);
      free(pcm); RARCH_RELEASE(commands); RARCH_RELEASE(device);
      printf("PASS frames=%u input=%lu leading=%u prebuffer=%lu owned=1 metal=1 width=%u height=%u\n",
            produced,(unsigned long)input,leading,(unsigned long)prebuffer,WIDTH,HEIGHT);
   }
   return 0;
}
