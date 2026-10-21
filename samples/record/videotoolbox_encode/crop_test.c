/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned allocations,fail_allocation;
static void *test_alloc(size_t size) { return ++allocations==fail_allocation?NULL:malloc(size); }
static void *test_zero(size_t count,size_t size) { return ++allocations==fail_allocation?NULL:calloc(count,size); }
#define malloc test_alloc
#define calloc test_zero
#include "../../../libretro-common/formats/h265/rh265_sps_crop.c"
#undef malloc
#undef calloc
static const uint8_t native_sps[]={0x42,0x01,0x01,0x02,0x20,0x00,0x00,0x03,0x00,0xb0,0x00,0x00,0x03,0x00,0x00,0x03,0x00,0x7b,0xa0,0x18,0x20,0x28,0x4d,0x88,0x17,0xb9,0x16,0x45,0x2f,0xfc,0xb9,0xfc,0x4f,0xea,0x6a,0x12,0x20,0x12,0x01};
static const uint8_t golden[]={0x42,0x01,0x01,0x02,0x20,0x00,0x00,0x03,0x00,0xb0,0x00,0x00,0x03,0x00,0x00,0x03,0x00,0x7b,0xa0,0x18,0x20,0x28,0x70,0x84,0x20,0xd8,0x81,0x7b,0x91,0x64,0x52,0xff,0xcb,0x9f,0xc4,0xfe,0xa6,0xa1,0x22,0x01,0x20,0x10};
static int convert(int argc,char **argv)
{
   FILE *file;
   long size;
   uint8_t *data,*out=NULL;
   size_t bytes=0;
   (void)argc;
   file=fopen(argv[1],"rb"); assert(file); assert(!fseek(file,0,SEEK_END)); size=ftell(file);
   assert(size>0 && size<=RH265_SPS_CROP_MAX_NAL_SIZE && !fseek(file,0,SEEK_SET));
   data=(uint8_t*)malloc((size_t)size); assert(data); assert(fread(data,1,(size_t)size,file)==(size_t)size); assert(!fclose(file));
   assert(rh265_sps_crop(data,(size_t)size,(unsigned)atoi(argv[3]),(unsigned)atoi(argv[4]),
            (unsigned)atoi(argv[5]),(unsigned)atoi(argv[6]),&out,&bytes));
   file=fopen(argv[2],"wb"); assert(file); assert(fwrite(out,1,bytes,file)==bytes && !fclose(file));
   free(out); free(data); return 0;
}
int main(int argc,char **argv)
{
   uint8_t *out=NULL,*again=NULL;
   uint8_t mutation[sizeof(native_sps)];
   size_t size=0,other=0,i;
   unsigned seed=1,trial;
   if (argc==7) return convert(argc,argv);
   assert(argc==1);
   assert(rh265_sps_crop(native_sps,sizeof(native_sps),192,160,162,130,&out,&size));
   assert(size==sizeof(golden) && !memcmp(out,golden,size));
   assert(rh265_sps_crop(out,size,192,160,162,130,&again,&other));
   assert(size==other && !memcmp(out,again,size)); free(again); again=NULL;
   assert(rh265_sps_crop(out,size,192,160,192,160,&again,&other));
   assert(other==sizeof(native_sps) && !memcmp(again,native_sps,other)); free(again); free(out); out=NULL;
   for (trial=1;trial<=3;trial++)
   {
      allocations=0; fail_allocation=trial;
      assert(!rh265_sps_crop(native_sps,sizeof(native_sps),192,160,162,130,&out,&size));
      assert(!out && !size);
   }
   fail_allocation=0;
   assert(!rh265_sps_crop(NULL,0,192,160,162,130,&out,&size));
   assert(!rh265_sps_crop(native_sps,RH265_SPS_CROP_MAX_NAL_SIZE+1,192,160,162,130,&out,&size));
   assert(!rh265_sps_crop(native_sps,sizeof(native_sps),190,160,162,130,&out,&size));
   assert(!rh265_sps_crop(native_sps,sizeof(native_sps),192,160,163,130,&out,&size));
   assert(!rh265_sps_crop(native_sps,sizeof(native_sps),192,160,194,130,&out,&size));
   for (i=0;i<16;i++) assert(!rh265_sps_crop(native_sps,i,192,160,162,130,&out,&size));
   for (trial=0;trial<20000;trial++)
   {
      bool ok;
      memcpy(mutation,native_sps,sizeof(mutation));
      seed=seed*1664525u+1013904223u;
      mutation[seed%sizeof(mutation)]^=(uint8_t)(1u<<((seed>>16)&7));
      ok=rh265_sps_crop(mutation,sizeof(mutation),192,160,162,130,&out,&size);
      if (ok)
      {
         assert(size<=sizeof(mutation)*2+64);
         assert(rh265_sps_crop(out,size,192,160,162,130,&again,&other));
         assert(other==size && !memcmp(again,out,size)); free(again); free(out);
      }
      else assert(!out && !size);
   }
   puts("PASS: native SPS golden crop, exact restoration/idempotence, allocation faults, bounds and 20000 mutations");
   return 0;
}
