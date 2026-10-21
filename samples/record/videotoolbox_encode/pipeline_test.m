/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "capture_reference.h"
#include "../../../record/record_gpu_worker_vt_native.h"
#include "native_packet.h"
#include <unistd.h>
#define PIPELINE_FRAMES 24
static void write_reference(FILE *out,unsigned depth,unsigned mode)
{
   unsigned plane,x,y;
   for (plane=0;plane<3;plane++)
      for (y=0;y<(plane?HEIGHT/2:HEIGHT);y++)
         for (x=0;x<(plane?WIDTH/2:WIDTH);x++)
         {
            double value;
            unsigned code;
            uint8_t bytes[2];
            if (!plane)
            {
               double rgb[3]; reference(x,y,mode,rgb);
               value=(depth==10?64:16)+(depth==10?876:219)*rgb[0];
            }
            else
            {
               unsigned px=x*2,py=y*2;
               double a[3],b[3],c[3],d[3],e[3],f[3];
               reference(px,py,mode,a); reference(px+1,py,mode,b);
               reference(px,py+1,mode,c); reference(px+1,py+1,mode,d);
               reference(px?px-1:0,py,mode,e); reference(px?px-1:0,py+1,mode,f);
               value=(depth==10?512:128)+(depth==10?896:224)
                  *(e[plane]+f[plane]+2*(a[plane]+c[plane])+b[plane]+d[plane])/8;
            }
            code=(unsigned)floor(value+.5); if (depth==8) code*=4;
            bytes[0]=(uint8_t)code; bytes[1]=(uint8_t)(code>>8);
            assert(fwrite(bytes,1,2,out)==2);
         }
}
/* Diagnostic only: verify every GPU input, and optionally substitute a fresh
 * CPU-filled encoder-pool buffer to isolate capture/reuse from native coding. */
static unsigned diagnostic_depth,diagnostic_mode;
static int diagnostic_submit(void *data,unsigned slot,void *buffer,uint64_t pts,uint64_t duration)
{
   videotoolbox_encode_t *encoder=(videotoolbox_encode_t*)data;
   CVPixelBufferRef image=(CVPixelBufferRef)buffer,copy=NULL;
   unsigned plane,x,y;
   int result;
   check_pixels(image,diagnostic_depth,diagnostic_mode);
   if (getenv("PIPELINE_CPU_CONTROL"))
   {
      assert(!CVPixelBufferPoolCreatePixelBuffer(NULL,VTCompressionSessionGetPixelBufferPool(encoder->session),&copy));
      assert(!CVPixelBufferLockBaseAddress(image,kCVPixelBufferLock_ReadOnly));
      assert(!CVPixelBufferLockBaseAddress(copy,0));
      for (plane=0;plane<2;plane++)
      {
         size_t stride=CVPixelBufferGetBytesPerRowOfPlane(copy,plane);
         unsigned height=(unsigned)CVPixelBufferGetHeightOfPlane(copy,plane);
         uint8_t *base=(uint8_t*)CVPixelBufferGetBaseAddressOfPlane(copy,plane);
         for (y=0;y<height;y++)
            for (x=0;x<stride/(diagnostic_depth==10?2:1);x++)
            {
               unsigned px=x<WIDTH?x:plane?WIDTH-2+(x&1):WIDTH-1;
               unsigned value=read_code(image,plane,px,y,diagnostic_depth);
               if (diagnostic_depth==10) ((uint16_t*)(base+y*stride))[x]=(uint16_t)(value<<6);
               else base[y*stride+x]=(uint8_t)value;
            }
      }
      CVPixelBufferUnlockBaseAddress(copy,0);
      CVPixelBufferUnlockBaseAddress(image,kCVPixelBufferLock_ReadOnly);
      image=copy;
   }
   result=record_vt_native_submit(data,slot,image,pts,duration);
   if (copy) CVPixelBufferRelease(copy);
   return result;
}
int main(int argc,char **argv)
{
   int rc=0;
   if (argc!=6) return 1;
   @autoreleasepool
   {
      id<MTLDevice> device=MTLCreateSystemDefaultDevice();
      id<MTLCommandQueue> commands_queue;
      id<MTLTexture> source;
      videotoolbox_encode_config_t config;
      videotoolbox_encode_t *encoder;
      record_gpu_capture_metal_t *capture;
      record_gpu_worker_vt_t *worker;
      record_gpu_queue_t *queue;
      CVPixelBufferRef images[2]={NULL,NULL};
      CVPixelBufferPoolRef pool;
      void *buffers[2];
      FILE *coded,*raw;
      OSStatus status;
      unsigned mode=(unsigned)atoi(argv[3]),produced=0,received=0,i;
      int length=0,result=0;
      bool closed=false;
      CFAbsoluteTime deadline;
      if (!device) return 77;
      memset(&config,0,sizeof(config));
      config.width=WIDTH; config.height=HEIGHT; config.slots=2;
      config.bitrate=16000000; config.key_interval=6; config.timescale=60000;
      config.hevc=atoi(argv[1])!=0; config.bit_depth=(unsigned)atoi(argv[2]); config.hdr=mode!=0;
      encoder=videotoolbox_encode_new(&config,&status);
      if (!encoder)
      {
         fprintf(stderr,"UNAVAILABLE: pipeline hardware status %d\n",(int)status);
         RARCH_RELEASE(device); return status==kVTCouldNotFindVideoEncoderErr?77:1;
      }
      pool=VTCompressionSessionGetPixelBufferPool(encoder->session); assert(pool);
      for (i=0;i<2;i++)
      { assert(!CVPixelBufferPoolCreatePixelBuffer(NULL,pool,&images[i])); buffers[i]=images[i]; }
      capture=record_gpu_capture_metal_new((__bridge void*)device,buffers,2,WIDTH,HEIGHT,config.bit_depth,mode);
      assert(capture);
      queue=record_gpu_queue_new(2); assert(queue);
      {
         record_gpu_worker_vt_ops_t ops;
         ops.ready=record_vt_native_ready; ops.buffer=record_vt_native_buffer;
         ops.release=record_vt_native_release; ops.submit=diagnostic_submit;
         ops.peek=record_vt_native_peek; ops.ack=record_vt_native_ack; ops.finish=record_vt_native_finish;
         diagnostic_depth=config.bit_depth; diagnostic_mode=mode;
         worker=record_gpu_worker_vt_new(queue,capture,encoder,&ops); assert(worker);
      }
      commands_queue=[device newCommandQueue]; assert(commands_queue);
      source=source_new(device,mode,false);
      coded=fopen(argv[4],"wb"); raw=fopen(argv[5],"wb"); assert(coded && raw);
      deadline=CFAbsoluteTimeGetCurrent()+20;
      while (result!=RECORD_GPU_VT_END)
      {
         void *sample=NULL;
         bool progress=false;
         assert(CFAbsoluteTimeGetCurrent()<deadline);
         if (produced<PIPELINE_FRAMES)
         {
            record_gpu_frame_t frame;
            int available=record_gpu_queue_acquire(queue,&frame.slot);
            assert(available==RECORD_GPU_QUEUE_OK || available==RECORD_GPU_QUEUE_BUSY);
            if (available==RECORD_GPU_QUEUE_OK)
            {
               id<MTLCommandBuffer> cb=[commands_queue commandBuffer]; assert(cb);
               assert(record_gpu_capture_metal_encode(capture,frame.slot,(__bridge void*)cb,
                        (__bridge void*)source,2,2)==1);
               frame.timestamp=(uint64_t)produced*1001; frame.duration=1001;
               frame.ready_value=0; frame.keyframe=produced==0;
               assert(record_gpu_queue_submit(queue,&frame));
               [cb commit];
               write_reference(raw,config.bit_depth,mode); produced++; progress=true;
            }
         }
         if (produced==PIPELINE_FRAMES && !closed)
         { assert(record_gpu_queue_finish(queue)); closed=true; }
         result=record_gpu_worker_vt_step(worker,&sample); assert(result!=RECORD_GPU_VT_ERROR);
         if (result==RECORD_GPU_VT_PACKET)
         {
            CMSampleBufferRef encoded=(CMSampleBufferRef)sample;
            void *again=NULL;
            assert(record_gpu_worker_vt_step(worker,&again)==RECORD_GPU_VT_PACKET && again==sample);
            assert(!CMTimeCompare(CMSampleBufferGetPresentationTimeStamp(encoded),CMTimeMake((int64_t)received*1001,60000)));
            assert(!CMTimeCompare(CMSampleBufferGetDuration(encoded),CMTimeMake(1001,60000)));
            if (!received) assert(headers(coded,CMSampleBufferGetFormatDescription(encoded),config.hevc,&length));
            assert(packet(coded,encoded,length));
            assert(record_gpu_worker_vt_release(worker)); received++; progress=true;
         }
         if (!progress && result==RECORD_GPU_VT_BUSY) usleep(1000); /* Test-only deadline polling. */
      }
      assert(produced==PIPELINE_FRAMES && received==PIPELINE_FRAMES);
      assert(record_gpu_worker_vt_free(worker)); assert(videotoolbox_encode_free(encoder));
      assert(record_gpu_capture_metal_free(capture)); assert(record_gpu_queue_free(queue));
      for (i=0;i<2;i++) CVPixelBufferRelease(images[i]);
      if (fclose(coded) || fclose(raw)) rc=1;
      RARCH_RELEASE(source); RARCH_RELEASE(commands_queue); RARCH_RELEASE(device);
      if (!rc) puts("PASS: two-slot Metal capture->queue->VT worker, borrowed output, timestamps and drain");
   }
   return rc;
}
