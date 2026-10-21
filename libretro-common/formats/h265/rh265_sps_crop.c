/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <formats/rh265_sps_crop.h>
#include <stdlib.h>
#include <string.h>
typedef struct crop_bits
{ const uint8_t *data; size_t size,pos; bool error; } crop_bits_t;
static unsigned crop_read(crop_bits_t *b,unsigned count)
{
   unsigned value=0,i;
   if (count>32 || b->pos>b->size || count>b->size-b->pos)
   { b->error=true; return 0; }
   for (i=0;i<count;i++,b->pos++) value=(value<<1)|((b->data[b->pos/8]>>(7-(b->pos&7)))&1);
   return value;
}
static unsigned crop_ue(crop_bits_t *b)
{
   unsigned zeros=0,suffix;
   while (!b->error && !crop_read(b,1)) if (++zeros>30) { b->error=true; return 0; }
   suffix=crop_read(b,zeros);
   return ((unsigned)1<<zeros)-1+suffix;
}
static void crop_write(uint8_t *out,size_t *pos,unsigned value,unsigned count)
{
   unsigned i;
   for (i=count;i;i--,(*pos)++)
      if ((value>>(i-1))&1) out[*pos/8]|=(uint8_t)((unsigned)1<<(7-(*pos&7)));
}
static void crop_write_ue(uint8_t *out,size_t *pos,unsigned value)
{
   unsigned width=1,n=value+1;
   while (n>>width) width++;
   *pos+=width-1; crop_write(out,pos,value+1,width);
}
bool rh265_sps_crop(const uint8_t *nal,size_t size,unsigned coded_width,
      unsigned coded_height,unsigned visible_width,unsigned visible_height,
      uint8_t **output,size_t *output_size)
{
   uint8_t *raw=NULL,*bits=NULL,*escaped=NULL;
   size_t i,raw_size=0,meaningful,begin,end,pos=0,bytes,count;
   unsigned zeros=0,sub_layers,profile[7],level[7],width,height,old[4],last;
   crop_bits_t reader;
   bool ok=false;
   if (output) *output=NULL;
   if (output_size) *output_size=0;
   if (!output || !output_size || !nal || size<3 || size>RH265_SPS_CROP_MAX_NAL_SIZE
         || (nal[0]&128) || ((nal[0]>>1)&63)!=33 || !(nal[1]&7)
         || coded_width<2 || coded_height<2 || coded_width>16384 || coded_height>16384
         || visible_width<2 || visible_height<2 || visible_width>coded_width || visible_height>coded_height
         || ((coded_width|coded_height|visible_width|visible_height)&1)) return false;
   raw=(uint8_t*)malloc(size); bits=(uint8_t*)calloc(size+16,1);
   escaped=(uint8_t*)malloc(size*2+64);
   if (!raw || !bits || !escaped) goto done;
   for (i=2;i<size;i++)
   {
      unsigned value=nal[i];
      if (zeros>=2)
      {
         if (value==3)
         { if (i+1==size || nal[i+1]>3) goto done; zeros=0; continue; }
         if (value<3) goto done;
      }
      raw[raw_size++]=(uint8_t)value; zeros=value?0:zeros+1;
   }
   if (!raw_size || !raw[raw_size-1]) goto done;
   meaningful=raw_size*8; last=raw[raw_size-1];
   while (!(last&1)) { meaningful--; last>>=1; }
   reader.data=raw; reader.size=meaningful; reader.pos=0; reader.error=false;
   crop_read(&reader,4); sub_layers=crop_read(&reader,3); crop_read(&reader,1);
   if (sub_layers>6) goto done;
   /* profile_tier_level has 88 profile/constraint bits and 8 level bits. */
   for (i=0;i<3;i++) crop_read(&reader,32);
   for (i=0;i<sub_layers;i++) { profile[i]=crop_read(&reader,1); level[i]=crop_read(&reader,1); }
   if (sub_layers) for (i=sub_layers;i<8;i++) if (crop_read(&reader,2)) goto done;
   for (i=0;i<sub_layers;i++)
   {
      if (profile[i]) { crop_read(&reader,32); crop_read(&reader,32); crop_read(&reader,24); }
      if (level[i]) crop_read(&reader,8);
   }
   if (crop_ue(&reader)>15 || crop_ue(&reader)!=1) goto done;
   width=crop_ue(&reader); height=crop_ue(&reader);
   if (reader.error || width!=coded_width || height!=coded_height) goto done;
   begin=reader.pos;
   if (crop_read(&reader,1))
   {
      for (i=0;i<4;i++) old[i]=crop_ue(&reader);
      if (old[0]>=width/2 || old[1]>=width/2-old[0]
            || old[2]>=height/2 || old[3]>=height/2-old[2]) goto done;
   }
   end=reader.pos;
   if (reader.error || end>=meaningful) goto done;
   /* At most 56 new bits: size+16 bytes covers the bounded window growth. */
   for (i=0;i<begin;i++) crop_write(bits,&pos,(raw[i/8]>>(7-(i&7)))&1,1);
   crop_write(bits,&pos,(visible_width!=width || visible_height!=height)?1:0,1);
   if (visible_width!=width || visible_height!=height)
   {
      crop_write_ue(bits,&pos,0); crop_write_ue(bits,&pos,(width-visible_width)/2);
      crop_write_ue(bits,&pos,0); crop_write_ue(bits,&pos,(height-visible_height)/2);
   }
   for (i=end;i<meaningful;i++) crop_write(bits,&pos,(raw[i/8]>>(7-(i&7)))&1,1);
   bytes=(pos+7)/8; count=2; zeros=0;
   memcpy(escaped,nal,2);
   for (i=0;i<bytes;i++)
   {
      if (zeros>=2 && bits[i]<=3) { escaped[count++]=3; zeros=0; }
      escaped[count++]=bits[i]; zeros=bits[i]?0:zeros+1;
   }
   *output=escaped; *output_size=count; escaped=NULL; ok=true;
done:
   free(escaped); free(bits); free(raw); return ok;
}
