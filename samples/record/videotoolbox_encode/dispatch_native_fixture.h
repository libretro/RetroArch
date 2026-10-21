/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_gpu_dispatch_metal.h"
static int dispatch_native_main(int argc,char **argv)
{
   if (argc!=8) return 1;
   @autoreleasepool
   {
      id<MTLDevice> device=MTLCreateSystemDefaultDevice();
      id<MTLCommandQueue> commands;
      id<MTLCommandBuffer> held=nil;
      id<MTLTexture> source;
      record_gpu_session_metal_config_t config;
      record_mux_avfoundation_config_t output;
      record_gpu_dispatch_metal_t *job;
      enum record_gpu_dispatch_metal_state state;
      enum record_gpu_session_metal_startup startup;
      enum record_avfoundation_session_error error;
      int32_t native_status;
      int system_error;
      unsigned mode=(unsigned)atoi(argv[3]),produced=0,rate=(unsigned)atoi(argv[7]);
      unsigned fault=getenv("SESSION_FAULT_CONTROL")?(unsigned)atoi(getenv("SESSION_FAULT_CONTROL")):0;
      uint64_t audio_produced=0,audio_target=0;
      bool stopping=fault==4,video_closed=false,audio_closed=false;
      CFAbsoluteTime deadline;
      int16_t *pcm;
      FILE *raw;
      if (!device) return 77;
      memset(&config,0,sizeof(config)); memset(&output,0,sizeof(output));
      config.width=WIDTH; config.height=HEIGHT; config.slots=2; config.bitrate=16000000;
      config.key_interval=6; config.timescale=60000; config.bit_depth=(unsigned)atoi(argv[2]);
      config.hevc=atoi(argv[1])!=0; config.mode=mode;
      output.timescale=60000; output.audio_rate=rate; output.channels=2; output.audio_layout=3;
      output.audio_bitrate=128000; output.max_audio_frames=1024;
      if (fault==3)
      {
         FILE *guard=fopen(argv[6],"wb"); assert(guard);
         assert(fwrite("keep",1,4,guard)==4 && !fclose(guard));
      }
      job=record_gpu_dispatch_metal_new((__bridge void*)device,&config,&output,4*(16+4096),argv[6]); assert(job);
      assert(!record_gpu_dispatch_metal_free(job));
      commands=[device newCommandQueue]; assert(commands);
      source=source_new_extent(device,mode,false,(WIDTH+31)&~31u,(HEIGHT+31)&~31u);
      raw=fopen(argv[5],"wb"); pcm=(int16_t*)malloc(4096); assert(raw && pcm);
      deadline=CFAbsoluteTimeGetCurrent()+30;
      do
      {
         bool progress=false;
         assert(CFAbsoluteTimeGetCurrent()<deadline);
         state=record_gpu_dispatch_metal_status(job,&startup,&native_status,&error,&system_error);
         if (state==RECORD_METAL_DISPATCH_DRAINING || state==RECORD_METAL_DISPATCH_FAILED) stopping=true;
         if (!stopping && state==RECORD_METAL_DISPATCH_READY && produced<PIPELINE_FRAMES)
         {
            id<MTLCommandBuffer> cb=[commands commandBuffer];
            int pushed;
            assert(cb);
            if (getenv("PIPELINE_DYNAMIC_CONTROL"))
            {
               reference_frame=produced;
               RARCH_RELEASE(source); source=source_new_extent(device,mode,false,(WIDTH+31)&~31u,(HEIGHT+31)&~31u);
            }
            pushed=record_gpu_dispatch_metal_video(job,(__bridge void*)cb,(__bridge void*)source,
                  2,2,(uint64_t)produced*1001,1001);
            if (pushed==RECORD_GPU_QUEUE_OK)
            {
               if (fault==1 && !produced) held=cb;
               else [cb commit];
               write_reference(raw,config.bit_depth,mode); produced++; progress=true;
               audio_target=((uint64_t)produced*1001*rate+59999)/60000;
               if ((fault==1 && produced==1) || (fault==2 && produced==6))
               { record_gpu_dispatch_metal_cancel(job); stopping=true; }
            }
            else
            {
               [cb commit]; /* Rendering still owns command-buffer submission. */
               if (pushed==RECORD_GPU_QUEUE_ERROR && fault) stopping=true;
               else assert(pushed==RECORD_GPU_QUEUE_BUSY); /* Unexpected normal refusal fails acceptance. */
            }
         }
         if (!audio_closed && !stopping && audio_produced<audio_target)
         {
            size_t frames=audio_target-audio_produced>1024?1024:(size_t)(audio_target-audio_produced),n;
            int pushed;
            for (n=0;n<frames;n++)
            {
               pcm[n*2]=(int16_t)(.2*32767*sin((double)(audio_produced+n)*440*6.283185307179586/rate));
               pcm[n*2+1]=(int16_t)(.2*32767*sin((double)(audio_produced+n)*660*6.283185307179586/rate));
            }
            pushed=record_gpu_dispatch_metal_audio(job,pcm,frames,audio_produced);
            if (pushed==RECORD_AUDIO_OK) { audio_produced+=frames; progress=true; }
            else if (pushed==RECORD_AUDIO_ERROR) stopping=true;
            else assert(pushed==RECORD_AUDIO_BUSY);
         }
         if (!video_closed && (stopping || produced==PIPELINE_FRAMES))
            video_closed=record_gpu_dispatch_metal_video_finish(job);
         if (!audio_closed && video_closed && (stopping || audio_produced==audio_target))
            audio_closed=record_gpu_dispatch_metal_audio_finish(job);
         if (held && video_closed && audio_closed)
         {
            usleep(20000); /* Fixture: GPU lease deliberately remains uncommitted. */
            assert(!record_gpu_dispatch_metal_free(job));
            assert(record_gpu_dispatch_metal_status(job,NULL,NULL,NULL,NULL)!=RECORD_METAL_DISPATCH_FAILED);
            [held commit]; held=nil;
         }
         if (!progress) usleep(1000); /* Fixture controller polling only. */
      } while (state!=RECORD_METAL_DISPATCH_DONE && state!=RECORD_METAL_DISPATCH_FAILED);
      assert(video_closed && audio_closed && !held && !system_error);
      if (startup!=RECORD_METAL_STARTUP_NONE)
      {
         fprintf(stderr,"UNAVAILABLE: dispatcher startup %d native %d\n",(int)startup,(int)native_status);
         while (!record_gpu_dispatch_metal_free(job)) { assert(CFAbsoluteTimeGetCurrent()<deadline); usleep(1000); }
         assert(!fclose(raw)); free(pcm); RARCH_RELEASE(source); RARCH_RELEASE(commands); RARCH_RELEASE(device);
         return native_status==kVTCouldNotFindVideoEncoderErr?77:1;
      }
      if (fault) assert(state==RECORD_METAL_DISPATCH_FAILED && error==(fault==3?RECORD_AVF_SESSION_MUX
               :fault==4?RECORD_AVF_SESSION_EMPTY:RECORD_AVF_SESSION_CANCELLED));
      else assert(state==RECORD_METAL_DISPATCH_DONE && error==RECORD_AVF_SESSION_NONE && produced==PIPELINE_FRAMES);
      while (!record_gpu_dispatch_metal_free(job)) { assert(CFAbsoluteTimeGetCurrent()<deadline); usleep(1000); }
      if (fault==3)
      {
         char bytes[4]; FILE *guard=fopen(argv[6],"rb"); assert(guard);
         assert(fread(bytes,1,4,guard)==4 && !memcmp(bytes,"keep",4) && fgetc(guard)==EOF && !fclose(guard));
      }
      assert(!fclose(raw)); free(pcm);
      RARCH_RELEASE(source); RARCH_RELEASE(commands); RARCH_RELEASE(device);
      printf("PASS: native dedicated Metal dispatcher fault=%u, producer closure and worker retirement\n",fault);
   }
   return 0;
}
