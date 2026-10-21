/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/videotoolbox_encode.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); goto done; } } while (0)
#define FRAMES 12
#include "native_packet.h"
/* Test-only CPU fixture creation. Production Metal capture must fill these
 * compatible planes on the GPU, not call this helper or lock their address. */
static bool fill(CVPixelBufferRef image, FILE *reference, unsigned frame, unsigned depth)
{
   unsigned plane, y, x, v;
   uint8_t *row;
   size_t stride;
   bool ok=true;
   if (CVPixelBufferLockBaseAddress(image,0)) return false;
   for (plane=0;plane<2;plane++)
   {
      row=(uint8_t*)CVPixelBufferGetBaseAddressOfPlane(image,plane);
      stride=CVPixelBufferGetBytesPerRowOfPlane(image,plane);
      for (y=0;y<(plane?24u:48u);y++)
         for (x=0;x<64;x++)
         {
            v=plane ? (x&1 ? 150-frame : 100+frame) : 64+frame*8;
            if (depth==10) ((uint16_t*)(row+y*stride))[x]=(uint16_t)(v<<8);
            else row[y*stride+x]=(uint8_t)v;
         }
   }
   CVPixelBufferUnlockBaseAddress(image,0);
   if (CVPixelBufferFillExtendedPixels(image)) return false;
   /* Planar 10-bit reference for an independent decoder. */
   for (plane=0;plane<3;plane++)
      for (x=0;x<(plane?64u*48u/4:64u*48u);x++)
      {
         uint8_t bytes[2];
         v=(plane==0?64+frame*8:plane==1?100+frame:150-frame)*4;
         bytes[0]=(uint8_t)v; bytes[1]=(uint8_t)(v>>8);
         if (fwrite(bytes,1,2,reference)!=2) ok=false;
      }
   return ok;
}
int main(int argc, char **argv)
{
   videotoolbox_encode_config_t config;
   videotoolbox_encode_t *encoder=NULL;
   CVPixelBufferRef images[FRAMES];
   CVPixelBufferPoolRef pool;
   const videotoolbox_encode_slot_t *result;
   FILE *out=NULL, *reference=NULL;
   OSStatus status;
   unsigned i, index;
   int length=0, rc=1;
   if (argc!=6) return 1;
   memset(images,0,sizeof(images)); memset(&config,0,sizeof(config));
   config.width=64; config.height=48; config.slots=FRAMES;
   config.bitrate=1000000; config.key_interval=6; config.timescale=60000;
   config.hevc=atoi(argv[1])!=0; config.bit_depth=(unsigned)atoi(argv[2]); config.hdr=atoi(argv[3])!=0;
   encoder=videotoolbox_encode_new(&config,&status);
   if (!encoder)
   {
      fprintf(stderr,"UNAVAILABLE: hardware encode setup status %d\n",(int)status);
      return status==kVTCouldNotFindVideoEncoderErr ? 77 : 1;
   }
   out=fopen(argv[4],"wb"); reference=fopen(argv[5],"wb"); CHECK(out && reference);
   pool=VTCompressionSessionGetPixelBufferPool(encoder->session); CHECK(pool);
   for (i=0;i<FRAMES;i++)
   {
      CHECK(!CVPixelBufferPoolCreatePixelBuffer(NULL,pool,&images[i]));
      CHECK(fill(images[i],reference,i,config.bit_depth));
      CHECK(videotoolbox_encode_submit(encoder,i,images[i],(int64_t)i*1001,1001)==1);
   }
   CHECK(videotoolbox_encode_finish(encoder));
   for (i=0;i<FRAMES;i++)
   {
      result=videotoolbox_encode_peek(encoder,&index);
      CHECK(result && index==i && !result->status && result->sample);
      CHECK(!CMTimeCompare(CMSampleBufferGetPresentationTimeStamp(result->sample),CMTimeMake((int64_t)i*1001,60000)));
      CHECK(!CMTimeCompare(CMSampleBufferGetDuration(result->sample),CMTimeMake(1001,60000)));
      if (!i) CHECK(headers(out,CMSampleBufferGetFormatDescription(result->sample),config.hevc,&length));
      CHECK(packet(out,result->sample,length));
      CHECK(videotoolbox_encode_ack(encoder));
   }
   CHECK(videotoolbox_encode_free(encoder)); encoder=NULL;
   rc=0;
done:
   videotoolbox_encode_cancel(encoder);
   for (i=0;i<FRAMES;i++) if (images[i]) CVPixelBufferRelease(images[i]);
   if (out && fclose(out)) rc=1;
   if (reference && fclose(reference)) rc=1;
   if (!rc) puts("PASS: required-hardware VT encode, source PTS, borrowed packets and drain");
   return rc;
}
