/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/mediacodec_hevc_packet.h"
int main(int argc,char **argv)
{
 FILE *file;long length;uint8_t *data,*csd,*packet;
 rh265_video *decoder;rh265_nal_t sets[3],retained[3],nal;
 rh265_nal_t *slices;rh265_access_unit_t *au;rh265_video_params_t *params;
 size_t size,pos=0,needed,csd_size;unsigned i=0;
 assert(argc==2);file=fopen(argv[1],"rb");assert(file);
 assert(!fseek(file,0,SEEK_END));length=ftell(file);assert(length>0 && length<=4*1024*1024);
 assert(!fseek(file,0,SEEK_SET));size=(size_t)length;
 data=(uint8_t*)malloc(size);assert(data);assert(fread(data,1,size,file)==size);fclose(file);
 memset(sets,0,sizeof(sets));
 while(rh265_nal_next(data,size,0,&pos,&nal)==1) {
  unsigned type=(nal.data[0]>>1)&63;
  if(type>=32 && type<=34)sets[type-32]=nal;
 }
 assert(mediacodec_hevc_csd(sets,NULL,0,&needed)==0 && needed);
 csd_size=needed;csd=(uint8_t*)malloc(needed);assert(csd);
 assert(mediacodec_hevc_csd(sets,csd,needed,&needed)==1);
 decoder=rh265_video_open();assert(decoder);
 assert(!rh265_video_retain_parameter_sets(decoder));
 assert(!rh265_video_set_extradata(decoder,csd,csd_size));
 slices=(rh265_nal_t*)calloc(64,sizeof(*slices));au=(rh265_access_unit_t*)calloc(1,sizeof(*au));
 params=(rh265_video_params_t*)calloc(1,sizeof(*params));assert(slices && au && params);
 assert(!rh265_video_get_access_unit(decoder,data,size,0,slices,64,au));
 assert(!rh265_video_get_params(decoder,(unsigned)au->first.pps_id,params));
 assert(!rh265_video_get_parameter_sets(decoder,(unsigned)au->first.pps_id,retained));
 for(i=0;i<3;i++)assert(retained[i].size==sets[i].size && !memcmp(retained[i].data,sets[i].data,sets[i].size));
 assert(mediacodec_hevc_packet(au,slices,64,NULL,0,&needed)==0 && needed);
 packet=(uint8_t*)malloc(needed);assert(packet);
 assert(mediacodec_hevc_packet(au,slices,64,packet,needed,&needed)==1);
 pos=0;i=0;
 while(rh265_nal_next(packet,needed,0,&pos,&nal)==1) {
  assert(i<au->num_slices && nal.size==slices[i].size && !memcmp(nal.data,slices[i].data,nal.size));i++;
 }
 assert(i==au->num_slices);
 printf("PASS: shipping HEVC parser -> retained parameter CSD -> Annex-B VCL roundtrip, %d-bit, %u slices\n",params->bit_depth,i);
 free(packet);free(params);free(au);free(slices);rh265_video_close(decoder);free(csd);free(data);return 0;
}
