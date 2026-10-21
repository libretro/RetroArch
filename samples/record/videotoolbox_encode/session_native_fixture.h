/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_avfoundation_session_native.h"
int main(int argc,char **argv)
{
   if (argc!=8) return 1;
   @autoreleasepool
   {
      id<MTLDevice> device=MTLCreateSystemDefaultDevice();
      id<MTLCommandQueue> commands;
      id<MTLTexture> source;
      videotoolbox_encode_config_t config;
      record_mux_avfoundation_config_t output;
      videotoolbox_encode_t *encoder;
      record_gpu_capture_metal_t *capture;
      record_gpu_queue_t *queue;
      record_avfoundation_session_t *session;
      CVPixelBufferPoolRef pool;
      CVPixelBufferRef images[2]={NULL,NULL};
      void *buffers[2];
      int16_t *pcm;
      FILE *raw;
      OSStatus status;
      unsigned mode=(unsigned)atoi(argv[3]),produced=0,i,rate=(unsigned)atoi(argv[7]);
      uint64_t audio_produced=0,audio_target=0;
      bool video_closed=false,audio_closed=false;
      int result=RECORD_AVF_WORKER_BUSY;
      CFAbsoluteTime deadline;
      if (!device) return 77;
      memset(&config,0,sizeof(config)); memset(&output,0,sizeof(output));
      config.width=WIDTH; config.height=HEIGHT; config.slots=2;
      config.bitrate=16000000; config.key_interval=6; config.timescale=60000;
      config.hevc=atoi(argv[1])!=0; config.bit_depth=(unsigned)atoi(argv[2]); config.hdr=mode!=0;
      if (config.hevc && config.bit_depth==10)
      { config.width=(WIDTH+31)&~31u; config.height=(HEIGHT+31)&~31u; }
      encoder=videotoolbox_encode_new(&config,&status);
      if (!encoder)
      {
         fprintf(stderr,"UNAVAILABLE: session encoder status %d\n",(int)status);
         RARCH_RELEASE(device); return status==kVTCouldNotFindVideoEncoderErr?77:1;
      }
      pool=VTCompressionSessionGetPixelBufferPool(encoder->session); assert(pool);
      for (i=0;i<2;i++)
      { assert(!CVPixelBufferPoolCreatePixelBuffer(NULL,pool,&images[i])); buffers[i]=images[i]; }
      capture=record_gpu_capture_metal_new((__bridge void*)device,buffers,2,WIDTH,HEIGHT,config.bit_depth,mode);
      queue=record_gpu_queue_new(2); assert(capture && queue);
      output.timescale=60000; output.audio_rate=rate; output.channels=2; output.audio_layout=3;
      output.audio_bitrate=128000; output.max_audio_frames=1024;
      session=record_avfoundation_session_new_native(queue,capture,encoder,WIDTH,HEIGHT,
            &output,4*(16+4096),argv[6]); assert(session);
      /* Session now owns all three components and its bounded audio queue. */
      assert(!record_avfoundation_session_free(session));
      commands=[device newCommandQueue]; assert(commands);
      source=source_new_extent(device,mode,false,config.width,config.height);
      pcm=(int16_t*)malloc(1024*2*sizeof(*pcm)); raw=fopen(argv[5],"wb"); assert(pcm && raw);
      deadline=CFAbsoluteTimeGetCurrent()+20;
      while (result!=RECORD_AVF_WORKER_END && result!=RECORD_AVF_WORKER_ERROR)
      {
         bool progress=false;
         assert(CFAbsoluteTimeGetCurrent()<deadline);
         if (produced<PIPELINE_FRAMES && !video_closed)
         {
            record_gpu_frame_t frame;
            int available=record_gpu_queue_acquire(queue,&frame.slot);
            assert(available==RECORD_GPU_QUEUE_OK || available==RECORD_GPU_QUEUE_BUSY);
            if (available==RECORD_GPU_QUEUE_OK)
            {
               id<MTLCommandBuffer> cb=[commands commandBuffer]; assert(cb);
               if (getenv("PIPELINE_DYNAMIC_CONTROL"))
               {
                  reference_frame=produced;
                  RARCH_RELEASE(source); source=source_new_extent(device,mode,false,config.width,config.height);
               }
               assert(record_gpu_capture_metal_encode(capture,frame.slot,(__bridge void*)cb,(__bridge void*)source,2,2)==1);
               frame.timestamp=(uint64_t)produced*1001; frame.duration=1001; frame.ready_value=0; frame.keyframe=produced==0;
               assert(record_gpu_queue_submit(queue,&frame)); [cb commit];
               write_reference(raw,config.bit_depth,mode); produced++; progress=true;
               audio_target=((uint64_t)produced*1001*rate+59999)/60000;
            }
         }
         if (!audio_closed && audio_produced<audio_target)
         {
            size_t frames=audio_target-audio_produced>1024?1024:(size_t)(audio_target-audio_produced),n;
            int pushed;
            for (n=0;n<frames;n++)
            {
               pcm[n*2]=(int16_t)(.2*32767*sin((double)(audio_produced+n)*440*6.283185307179586/rate));
               pcm[n*2+1]=(int16_t)(.2*32767*sin((double)(audio_produced+n)*660*6.283185307179586/rate));
            }
            pushed=record_avfoundation_session_audio(session,pcm,frames,audio_produced);
            assert(pushed==RECORD_AUDIO_OK || pushed==RECORD_AUDIO_BUSY);
            if (pushed==RECORD_AUDIO_OK) { audio_produced+=frames; progress=true; }
         }
         if (produced==PIPELINE_FRAMES && !video_closed)
         { assert(record_avfoundation_session_video_finish(session)); video_closed=true; }
         if (video_closed && audio_produced==audio_target && !audio_closed)
         { assert(record_avfoundation_session_audio_finish(session)); audio_closed=true; }
         result=record_avfoundation_session_step(session);
         assert(record_avfoundation_session_error(session)==RECORD_AVF_SESSION_NONE);
         assert(result!=RECORD_AVF_WORKER_ERROR);
         if (!progress && result==RECORD_AVF_WORKER_BUSY) usleep(1000); /* Fixture only. */
      }
      assert(produced==PIPELINE_FRAMES && video_closed && audio_closed);
      assert(record_avfoundation_session_step(session)==RECORD_AVF_WORKER_END);
      assert(record_avfoundation_session_free(session));
      /* No direct capture/queue/encoder access after producer closure. */
      for (i=0;i<2;i++) CVPixelBufferRelease(images[i]);
      assert(!fclose(raw)); free(pcm);
      RARCH_RELEASE(source); RARCH_RELEASE(commands); RARCH_RELEASE(device);
      puts("PASS: native owned session first-sample mux, source clocks, dual close and complete resource retirement");
   }
   return 0;
}
