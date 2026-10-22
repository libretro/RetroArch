/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../../gfx/common/mediacodec_hevc_packet.h"
int main(void)
{
 static const uint8_t vps[]={64,1,128},sps[]={66,1,0,0,3,1,128},pps[]={68,1,128};
 static const uint8_t slice[]={38,1,128};
 rh265_nal_t sets[3],slices[2];rh265_access_unit_t au;uint8_t output[64],before[64];size_t needed;unsigned i;
 sets[0].data=vps;sets[0].size=sizeof(vps);sets[1].data=sps;sets[1].size=sizeof(sps);sets[2].data=pps;sets[2].size=sizeof(pps);
 memset(output,0x5a,sizeof(output));memcpy(before,output,sizeof(output));
 assert(mediacodec_hevc_csd(sets,NULL,0,&needed)==0 && needed==25);
 assert(mediacodec_hevc_csd(sets,output,24,&needed)==0 && needed==25 && !memcmp(output,before,sizeof(output)));
 assert(mediacodec_hevc_csd(sets,output,sizeof(output),&needed)==1 && needed==25);
 assert(!memcmp(output,"\0\0\0\1",4) && !memcmp(output+4,vps,sizeof(vps)));
 assert(!memcmp(output+11,sps,sizeof(sps)) && !memcmp(output+22,pps,sizeof(pps)));
 memset(&au,0,sizeof(au));au.first.nal_type=19;au.num_slices=2;au.coded_bytes=6;
 for(i=0;i<2;i++){slices[i].data=slice;slices[i].size=sizeof(slice);}
 assert(mediacodec_hevc_packet(&au,slices,2,output,sizeof(output),&needed)==1 && needed==14);
 memcpy(before,output,sizeof(output));au.coded_bytes++;
 assert(mediacodec_hevc_packet(&au,slices,2,output,sizeof(output),&needed)==-1 && !needed && !memcmp(output,before,sizeof(output)));
 au.coded_bytes--;au.first.temporal_id=1;
 assert(mediacodec_hevc_packet(&au,slices,2,output,sizeof(output),&needed)==-1);
 au.first.temporal_id=0;
 assert(mediacodec_hevc_packet(&au,slices,1,output,sizeof(output),&needed)==-1);
 memcpy(output,slice,sizeof(slice));slices[0].data=output;
 assert(mediacodec_hevc_packet(&au,slices,2,output,sizeof(output),&needed)==-1 && !needed);
 sets[1].data=pps;
 assert(mediacodec_hevc_csd(sets,output,sizeof(output),&needed)==-1);
 sets[1].data=sps;sets[1].size=(size_t)-1;
 assert(mediacodec_hevc_csd(sets,output,sizeof(output),&needed)==-1);
 puts("PASS: Android HEVC Annex-B CSD/access-unit packing, exact sizes, emulation prevention, BUSY and alias refusal");return 0;
}
