/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <CoreMedia/CoreMedia.h>
#include "../../../record/record_vt_packet.h"
static unsigned copies;
static uint8_t config_bytes[23]={1,0,0,0,3};
static const int config_token=7, atoms_token=8;
static unsigned config_fault;
static bool config_hevc;
CFTypeRef CMFormatDescriptionGetExtension(CMFormatDescriptionRef f, const void *key)
{
   assert(key==kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
   config_bytes[21]=3;
   config_hevc=f->codec==kCMVideoCodecType_HEVC;
   return config_fault==1 ? NULL : config_fault==2 ? &config_token : &atoms_token;
}
unsigned CFDataGetTypeID(void) { return 3; }
CFIndex CFDataGetLength(CFDataRef data)
{ assert(data==&config_token); return config_fault==5 ? 6 : config_fault==6 ? 65537 : 23; }
const uint8_t *CFDataGetBytePtr(CFDataRef data)
{ assert(data==&config_token); return config_fault==7 ? NULL : config_bytes; }
static const int yes = 1, no = 0;
unsigned CMFormatDescriptionGetMediaType(CMFormatDescriptionRef f) { return f->media; }
unsigned CMFormatDescriptionGetMediaSubType(CMFormatDescriptionRef f) { return f->codec; }
int CMFormatDescriptionEqual(CMFormatDescriptionRef a, CMFormatDescriptionRef b) { return a == b; }
int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(CMFormatDescriptionRef f, size_t i,
      const uint8_t **p, size_t *n, size_t *count, int *lengths)
{
   (void)i; (void)p; (void)n; (void)count;
   *lengths = f->lengths; return f->error;
}
int CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(CMFormatDescriptionRef f, size_t i,
      const uint8_t **p, size_t *n, size_t *count, int *lengths)
{ return CMVideoFormatDescriptionGetH264ParameterSetAtIndex(f, i, p, n, count, lengths); }
CMTime CMTimeConvertScale(CMTime t, int32_t s, int method)
{ (void)method; t.value = t.value * s / t.timescale; t.timescale = s; return t; }
int CMTimeCompare(CMTime a, CMTime b)
{ return a.value * b.timescale != b.value * a.timescale; }
int CMSampleBufferDataIsReady(CMSampleBufferRef s) { return s->ready; }
int CMSampleBufferGetNumSamples(CMSampleBufferRef s) { return s->samples; }
CMFormatDescriptionRef CMSampleBufferGetFormatDescription(CMSampleBufferRef s) { return s->format; }
CMTime CMSampleBufferGetPresentationTimeStamp(CMSampleBufferRef s) { return s->pts; }
CMTime CMSampleBufferGetDecodeTimeStamp(CMSampleBufferRef s) { return s->dts; }
CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef s) { return s->block; }
size_t CMSampleBufferGetSampleSize(CMSampleBufferRef s, size_t i) { (void)i; return s->sample_size; }
CFArrayRef CMSampleBufferGetSampleAttachmentsArray(CMSampleBufferRef s, bool create)
{ assert(!create); return s->attachments ? s : NULL; }
int CMBlockBufferGetDataPointer(CMBlockBufferRef b, size_t off, size_t *n, size_t *total, char **p)
{ assert(!off); *n=b->contiguous; *total=b->size; *p=(char*)b->bytes; return b->error; }
int CMBlockBufferCopyDataBytes(CMBlockBufferRef b, size_t off, size_t n, void *p)
{ assert(!off && n==b->size); copies++; if (b->copy_error) return -1; memcpy(p,b->bytes,n); return 0; }
long CFArrayGetCount(CFArrayRef a) { return ((CMSampleBufferRef)a)->attachments; }
const void *CFArrayGetValueAtIndex(CFArrayRef a, long i) { assert(!i); return a; }
unsigned CFGetTypeID(CFTypeRef p) { return p==&config_token ? 3 : p==&yes || p==&no ? 2 : 1; }
unsigned CFDictionaryGetTypeID(void) { return 1; }
unsigned CFBooleanGetTypeID(void) { return 2; }
const void *CFDictionaryGetValue(CFDictionaryRef d, const void *key)
{
   CMSampleBufferRef s=(CMSampleBufferRef)d;
   if (d==&atoms_token)
   {
      assert(!strcmp((const char*)key,config_hevc?"hvcC":"avcC"));
      return config_fault==3 ? NULL : config_fault==4 ? &yes : &config_token;
   }
   assert(key==kCMSampleAttachmentKey_NotSync);
   return s->not_sync<0 ? NULL : s->not_sync ? &yes : &no;
}
bool CFBooleanGetValue(CFBooleanRef b) { return *(const int*)b!=0; }
int main(void)
{
   struct mock_format format={kCMVideoCodecType_H264,kCMMediaType_Video,4,0}, other;
   struct mock_block block;
   struct mock_sample sample, second;
   record_vt_packet_t *p;
   record_vt_packet_view_t v;
   const void *headers;
   size_t header_size;
   bool hevc;
   memset(&block,0,sizeof(block)); memset(&sample,0,sizeof(sample));
   block.size=block.contiguous=16; memset(block.bytes,0x5a,16);
   sample.format=&format; sample.block=&block; sample.sample_size=16;
   sample.ready=sample.samples=1; sample.pts.value=90; sample.pts.timescale=90; sample.pts.flags=1;
   p=record_vt_packet_new(&format,1000,32); assert(p);
   assert(record_vt_packet_config(p,&headers,&header_size,&hevc));
   assert(headers==config_bytes && header_size==23 && !hevc && !copies);
   for (config_fault=1;config_fault<=7;config_fault++)
      assert(!record_vt_packet_config(p,&headers,&header_size,&hevc));
   config_fault=0; config_bytes[0]=0;
   assert(!record_vt_packet_config(p,&headers,&header_size,&hevc));
   config_bytes[0]=1; config_bytes[4]=2;
   assert(!record_vt_packet_config(p,&headers,&header_size,&hevc));
   config_bytes[4]=3;
   assert(record_vt_packet_config(p,&headers,&header_size,&hevc));
   assert(record_vt_packet_read(p,&sample,&v)==1);
   assert(v.data==block.bytes && v.size==16 && v.pts==1000 && v.dts==1000 && v.keyframe && !copies);
   assert(!record_vt_packet_free(p));
   second=sample; assert(record_vt_packet_read(p,&second,&v)==0);
   assert(record_vt_packet_read(p,&sample,&v)==1 && v.data==block.bytes && !copies);
   assert(record_vt_packet_release(p) && !record_vt_packet_release(p));
   block.contiguous=7; sample.attachments=1; sample.not_sync=1;
   sample.dts=sample.pts; sample.dts.value=45;
   assert(record_vt_packet_read(p,&sample,&v)==1 && v.data!=block.bytes && copies==1);
   assert(!memcmp(v.data,block.bytes,16) && !v.keyframe && v.dts==500);
   assert(record_vt_packet_read(p,&sample,&v)==1 && copies==1);
   assert(record_vt_packet_release(p));
   block.copy_error=1; assert(record_vt_packet_read(p,&sample,&v)==-1);
   block.copy_error=0; block.contiguous=16;
   sample.pts.value=1; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.pts.value=90; sample.pts.epoch=1; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.pts.epoch=0; sample.dts.flags=3; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.dts.flags=0; sample.sample_size=15; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.sample_size=16; sample.samples=2; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.samples=1; sample.ready=0; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.ready=1; sample.attachments=2; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.attachments=0; other=format; sample.format=&other; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.format=NULL; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.format=&format; sample.block=NULL; assert(record_vt_packet_read(p,&sample,&v)==-1);
   sample.block=&block; block.size=33; assert(record_vt_packet_read(p,&sample,&v)==-1);
   block.size=16; block.error=1; assert(record_vt_packet_read(p,&sample,&v)==-1);
   block.error=0; assert(record_vt_packet_read(p,&sample,&v)==1); assert(record_vt_packet_release(p));
   assert(record_vt_packet_free(p));
   p=record_vt_packet_new(&format,1000,RECORD_VT_PACKET_MAX_SIZE);
   assert(p && record_vt_packet_free(p));
   assert(!record_vt_packet_new(&format,1000,RECORD_VT_PACKET_MAX_SIZE+1u));
   assert(!record_vt_packet_new(&format,1000,0xfffffau));
   format.lengths=2; assert(!record_vt_packet_new(&format,1000,32));
   format.lengths=4; format.codec=kCMVideoCodecType_HEVC; p=record_vt_packet_new(&format,1000,32); assert(p);
   assert(record_vt_packet_config(p,&headers,&header_size,&hevc) && hevc && headers==config_bytes);
   assert(record_vt_packet_free(p));
   format.error=1; assert(!record_vt_packet_new(&format,1000,32));
   assert(!record_vt_packet_new(&format,0,32)); assert(!record_vt_packet_new(&format,1000,0));
   puts("PASS VT packet loans, zero-copy blocks, bounded fragmented fallback, source clocks and format admission");
   return 0;
}
