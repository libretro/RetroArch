/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
typedef int OSStatus;
typedef unsigned VTEncodeInfoFlags;
typedef struct { int64_t value; int32_t scale; } CMTime;
static const CMTime kCMTimeInvalid = {0,0};
typedef const char *CFStringRef;
typedef struct object object_t;
typedef object_t *CFTypeRef;
typedef object_t *CFNumberRef;
typedef object_t *CFBooleanRef;
typedef object_t *CFMutableDictionaryRef;
typedef object_t *CVPixelBufferRef;
typedef object_t *CMSampleBufferRef;
typedef object_t *VTCompressionSessionRef;
typedef void (*callback_t)(void*, void*, OSStatus, VTEncodeInfoFlags, CMSampleBufferRef);
struct object
{
   unsigned refs, type, width, height, format, count;
   int64_t number;
   const char *text;
   CFStringRef keys[24];
   const void *values[24];
   int64_t numbers[24];
   callback_t callback;
   void *tickets[16];
   CVPixelBufferRef inputs[16];
};
static unsigned live, allocations, allocation_fail, fail_stage, encode_mode, submits;
static bool hardware = true;
static bool expected_hdr, expected_hevc;
static unsigned expected_depth = 8;
static uint64_t expected_gpu;
static CFStringRef expected_id;
#ifndef VIDEOTOOLBOX_ENCODE_TEST_NO_GPU_SELECTION
#define VIDEOTOOLBOX_ENCODE_TEST_GPU_SELECTION
#endif
static object_t truth, falsity, image, sample;
#define kCFBooleanTrue (&truth)
#define kCFBooleanFalse (&falsity)
#define kCFNumberSInt32Type 1
#define kCFNumberSInt64Type 2
static int kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks;
#define kVTParameterErr (-1)
#define kVTAllocationFailedErr (-2)
#define kVTCouldNotFindVideoEncoderErr (-3)
#define kCMVideoCodecType_H264 1
#define kCMVideoCodecType_HEVC 2
#define kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange 10
#define kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange 11
/* Keys and constant string values are distinct stable strings. */
#define kCVPixelBufferWidthKey "width"
#define kCVPixelBufferHeightKey "height"
#define kCVPixelBufferExtendedPixelsRightKey "right"
#define kCVPixelBufferExtendedPixelsBottomKey "bottom"
#define kCVPixelBufferPixelFormatTypeKey "format"
#define kCVPixelBufferMetalCompatibilityKey "metal"
#define kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder "require-hardware"
#define kVTVideoEncoderSpecification_RequiredEncoderGPURegistryID "require-gpu"
#define kVTVideoEncoderSpecification_EncoderID "encoder-id"
#define kVTCompressionPropertyKey_EncoderID "encoder-id-result"
#define kVTCompressionPropertyKey_AverageBitRate "bitrate"
#define kVTCompressionPropertyKey_MaxKeyFrameInterval "key-interval"
#define kVTCompressionPropertyKey_MaxFrameDelayCount "frame-delay"
#define kVTCompressionPropertyKey_RealTime "realtime"
#define kVTCompressionPropertyKey_AllowFrameReordering "reorder"
#define kVTCompressionPropertyKey_ProfileLevel "profile"
#define kVTCompressionPropertyKey_ColorPrimaries "primaries"
#define kVTCompressionPropertyKey_TransferFunction "transfer"
#define kVTCompressionPropertyKey_YCbCrMatrix "matrix"
#define kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder "hardware"
#define kVTProfileLevel_H264_High_AutoLevel "h264-high"
#define kVTProfileLevel_HEVC_Main_AutoLevel "hevc-main"
#define kVTProfileLevel_HEVC_Main10_AutoLevel "hevc-main10"
#define kCVImageBufferColorPrimaries_ITU_R_2020 "2020-primary"
#define kCVImageBufferColorPrimaries_ITU_R_709_2 "709-primary"
#define kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ "pq"
#define kCVImageBufferTransferFunction_ITU_R_709_2 "709-transfer"
#define kCVImageBufferYCbCrMatrix_ITU_R_2020 "2020-matrix"
#define kCVImageBufferYCbCrMatrix_ITU_R_709_2 "709-matrix"
static object_t *object_new(unsigned type)
{
   object_t *p;
   if (++allocations == allocation_fail) return NULL;
   p = (object_t*)calloc(1, sizeof(*p));
   assert(p); p->refs = 1; p->type = type; live++; return p;
}
static CFTypeRef CFRetain(CFTypeRef p) { assert(p && p->refs); p->refs++; return p; }
static void CFRelease(CFTypeRef p)
{
   assert(p && p->refs);
   if (!--p->refs) { assert(p->type != 4); live--; free(p); }
}
static unsigned CFGetTypeID(CFTypeRef p) { return p->type; }
static unsigned CFBooleanGetTypeID(void) { return 4; }
static unsigned CFStringGetTypeID(void) { return 5; }
static size_t CFStringGetLength(CFStringRef s) { return strlen(s); }
static bool CFEqual(CFTypeRef a,CFStringRef b) { assert(a->type==5); return !strcmp(a->text,b); }
static bool CFBooleanGetValue(CFBooleanRef p) { return p == &truth; }
static CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef p) { return CFRetain(p); }
static void CVPixelBufferRelease(CVPixelBufferRef p) { CFRelease(p); }
static unsigned CVPixelBufferGetWidth(CVPixelBufferRef p) { return p->width; }
static unsigned CVPixelBufferGetHeight(CVPixelBufferRef p) { return p->height; }
static unsigned CVPixelBufferGetPixelFormatType(CVPixelBufferRef p) { return p->format; }
static CFMutableDictionaryRef CFDictionaryCreateMutable(void *a, int n, void *k, void *v)
{ (void)a; (void)n; (void)k; (void)v; return object_new(1); }
static CFNumberRef CFNumberCreate(void *a, int t, const void *value)
{
   object_t *p=object_new(2); (void)a;
   if (p)
   {
      if (t==kCFNumberSInt64Type) memcpy(&p->number,value,sizeof(p->number));
      else { int32_t number; memcpy(&number,value,sizeof(number)); p->number=number; }
   }
   return p;
}
static bool number_key(CFStringRef key)
{ return !strcmp(key,"require-gpu") || !strcmp(key,"right") || !strcmp(key,"bottom") || !strcmp(key,"width") || !strcmp(key,"height") || !strcmp(key,"format") || !strcmp(key,"bitrate") || !strcmp(key,"key-interval") || !strcmp(key,"frame-delay"); }
static void CFDictionarySetValue(CFMutableDictionaryRef d, CFStringRef key, const void *value)
{
   unsigned i = d->count++;
   assert(i < 24); d->keys[i] = key;
   /* Copy numbers immediately; the real CF dictionary retains them. */
   if (number_key(key)) d->numbers[i] = ((const object_t*)value)->number;
   else d->values[i] = value;
}
static unsigned entry(CFMutableDictionaryRef d, CFStringRef key)
{ unsigned i; for (i=0;i<d->count;i++) if (!strcmp(d->keys[i],key)) return i; abort(); }
static CMTime CMTimeMake(int64_t v, int32_t s) { CMTime t; t.value=v; t.scale=s; return t; }
static OSStatus VTCompressionSessionCreate(void *a, int32_t w, int32_t h, unsigned codec,
      CFMutableDictionaryRef spec, CFMutableDictionaryRef attrs, void *b,
      callback_t callback, void *ref, VTCompressionSessionRef *out)
{
   (void)a; (void)b; (void)ref;
   assert(w==64 && h==48 && (codec==1 || codec==2));
   assert(codec == (expected_hevc ? 2u : 1u));
   assert(spec->values[entry(spec,"require-hardware")] == &truth);
   if (expected_id) assert(!strcmp((const char*)spec->values[entry(spec,"encoder-id")],expected_id));
   if (expected_gpu) assert((uint64_t)spec->numbers[entry(spec,"require-gpu")]==expected_gpu);
   else
   {
      unsigned i;
      for (i=0;i<spec->count;i++) assert(strcmp(spec->keys[i],"require-gpu"));
   }
   assert(attrs->values[entry(attrs,"metal")] == &truth);
   assert(attrs->numbers[entry(attrs,"width")] == w);
   assert(attrs->numbers[entry(attrs,"height")] == h);
   assert(attrs->numbers[entry(attrs,"format")] == (expected_depth==10?11:10));
   if (expected_hevc && expected_depth==10)
   {
      assert(attrs->numbers[entry(attrs,"right")]==0);
      assert(attrs->numbers[entry(attrs,"bottom")]==16);
   }
   if (fail_stage==1) return -101;
   *out=object_new(3); if (!*out) return -2;
   (*out)->callback=callback; (*out)->text=expected_id; return 0;
}
static OSStatus VTSessionSetProperties(VTCompressionSessionRef s, CFMutableDictionaryRef p)
{
   const char *profile=(const char*)p->values[entry(p,"profile")];
   (void)s;
   assert(p->values[entry(p,"reorder")] == &falsity);
   assert(p->values[entry(p,"realtime")] == &truth);
   assert(!strcmp(profile,expected_hevc?(expected_depth==10?"hevc-main10":"hevc-main"):"h264-high"));
   assert(!strcmp((const char*)p->values[entry(p,"primaries")],expected_hdr?"2020-primary":"709-primary"));
   assert(!strcmp((const char*)p->values[entry(p,"transfer")],expected_hdr?"pq":"709-transfer"));
   assert(!strcmp((const char*)p->values[entry(p,"matrix")],expected_hdr?"2020-matrix":"709-matrix"));
   assert(p->numbers[entry(p,"bitrate")] == 1000000);
   assert(p->numbers[entry(p,"frame-delay")] == 1);
   return fail_stage==2 ? -102 : 0;
}
static OSStatus VTCompressionSessionPrepareToEncodeFrames(VTCompressionSessionRef s)
{ (void)s; return fail_stage==3 ? -103 : 0; }
static OSStatus VTSessionCopyProperty(VTCompressionSessionRef s, CFStringRef k, void *a, CFTypeRef *out)
{
   (void)a;
   if (!strcmp(k,kVTCompressionPropertyKey_EncoderID))
   {
      if (fail_stage==7) return -107;
      if (fail_stage==8) { *out=CFRetain(&truth); return 0; }
      *out=object_new(5); if (!*out) return kVTAllocationFailedErr;
      (*out)->text=fail_stage==6?"unexpected-encoder":s->text; return 0;
   }
   if (fail_stage==4) return -104;
   *out=CFRetain(hardware?&truth:&falsity); return 0;
}
static OSStatus VTCompressionSessionEncodeFrame(VTCompressionSessionRef s, CVPixelBufferRef p,
      CMTime pts, CMTime duration, void *props, void *ticket, VTEncodeInfoFlags *flags)
{
   (void)props; assert(pts.scale==60000 && duration.value>0); submits++; *flags=0;
   if (encode_mode==1) return -201;
   if (encode_mode==2)
   { s->tickets[s->count]=ticket; s->inputs[s->count++]=p; *flags=1; }
   else s->callback(NULL,ticket,0,encode_mode==3?2:0,encode_mode==3?NULL:&sample);
   return 0;
}
static OSStatus VTCompressionSessionCompleteFrames(VTCompressionSessionRef s, CMTime until)
{
   unsigned i;
   (void)until;
   if (fail_stage==5) return -105;
   for (i=s->count;i;i--) s->callback(NULL,s->tickets[i-1],0,1,&sample);
   s->count=0; return 0;
}
static void VTCompressionSessionInvalidate(VTCompressionSessionRef s)
{ s->count=0; }
#define VIDEOTOOLBOX_ENCODE_TEST_TYPES
#include "../../../gfx/common/videotoolbox_encode.h"
int main(void)
{
   videotoolbox_encode_config_t c;
   videotoolbox_encode_t *e;
   const videotoolbox_encode_slot_t *packet;
   OSStatus error;
   unsigned i, slot;
   memset(&c,0,sizeof(c)); c.width=64; c.height=48; c.slots=2;
   c.bitrate=1000000; c.key_interval=60; c.timescale=60000; c.bit_depth=8;
   truth.refs=falsity.refs=1; truth.type=falsity.type=4;
   image.refs=sample.refs=1; image.type=sample.type=4;
   image.width=64; image.height=48; image.format=10;
   for (i=1;i<=5;i++)
   { fail_stage=i; e=videotoolbox_encode_new(&c,&error); if (i<5) {assert(!e && error); assert(!live);} else {assert(e); assert(!videotoolbox_encode_finish(e)); videotoolbox_encode_cancel(e);} }
   fail_stage=0; hardware=false; assert(!videotoolbox_encode_new(&c,&error)); assert(!live); hardware=true;
   for (i=1;i<=10;i++)
   { allocations=0; allocation_fail=i; e=videotoolbox_encode_new(&c,&error); assert(!e && error && !live); }
   allocation_fail=0;
   c.slots=1; assert(!videotoolbox_encode_new(&c,&error)); c.slots=2;
   e=videotoolbox_encode_new(&c,&error); assert(e && !error);
   assert(!videotoolbox_encode_free(e));
   assert(videotoolbox_encode_submit(e,0,&image,0,1001)==1);
   assert(videotoolbox_encode_submit(e,0,&image,1001,1001)==0);
   assert(videotoolbox_encode_submit(e,1,&image,0,1001)==-1);
   packet=videotoolbox_encode_peek(e,&slot); assert(packet && slot==0 && packet->sample==&sample);
   assert(videotoolbox_encode_ack(e)); assert(!videotoolbox_encode_ack(e));
   encode_mode=2;
   assert(videotoolbox_encode_submit(e,1,&image,1001,1001)==1);
   assert(videotoolbox_encode_submit(e,0,&image,2002,1001)==1);
   assert(videotoolbox_encode_submit(e,0,&image,3003,1001)==0);
   assert(!videotoolbox_encode_peek(e,&slot));
   assert(videotoolbox_encode_finish(e)); assert(videotoolbox_encode_finish(e));
   assert(!videotoolbox_encode_free(e));
   assert(videotoolbox_encode_peek(e,&slot) && slot==1); assert(videotoolbox_encode_ack(e));
   assert(videotoolbox_encode_peek(e,&slot) && slot==0); assert(videotoolbox_encode_ack(e));
   assert(videotoolbox_encode_free(e));
   e=videotoolbox_encode_new(&c,&error); assert(e);
   assert(videotoolbox_encode_submit(e,0,&image,0,1001)==1);
   fail_stage=5; assert(!videotoolbox_encode_finish(e));
   videotoolbox_encode_cancel(e); fail_stage=0;
   for (i=1;i<=3;i+=2)
   {
      encode_mode=i; e=videotoolbox_encode_new(&c,&error); assert(e);
      assert(videotoolbox_encode_submit(e,0,&image,0,1001)==(i==1?-1:1));
      if (i==3) {packet=videotoolbox_encode_peek(e,&slot); assert(packet && !packet->sample && packet->flags==2);}
      videotoolbox_encode_cancel(e);
   }
   assert(!live && image.refs==1 && sample.refs==1 && truth.refs==1 && falsity.refs==1);
   c.hdr=true; assert(!videotoolbox_encode_new(&c,&error));
   c.bit_depth=10; assert(!videotoolbox_encode_new(&c,&error)); c.hevc=true;
   expected_hdr=expected_hevc=true; expected_depth=10;
   for (i=1;i<=12;i++)
   { allocations=0; allocation_fail=i; e=videotoolbox_encode_new(&c,&error); assert(!e && error && !live); }
   allocation_fail=0;
   e=videotoolbox_encode_new(&c,&error); assert(e); videotoolbox_encode_cancel(e);
   c.hdr=expected_hdr=false;
   e=videotoolbox_encode_new(&c,&error); assert(e); videotoolbox_encode_cancel(e);
   c.bit_depth=expected_depth=8;
   e=videotoolbox_encode_new(&c,&error); assert(e); videotoolbox_encode_cancel(e);
   assert(!live);
#ifdef VIDEOTOOLBOX_ENCODE_TEST_GPU_SELECTION
   {
      unsigned total;
      expected_gpu=((uint64_t)1<<48)|0x1234;
      allocations=0; e=videotoolbox_encode_new_with_gpu(&c,expected_gpu,&error);
      assert(e && !error); total=allocations; videotoolbox_encode_cancel(e);
      for (i=1;i<=total;i++)
      {
         allocations=0; allocation_fail=i;
         assert(!videotoolbox_encode_new_with_gpu(&c,expected_gpu,&error) && error && !live);
      }
      allocation_fail=0; expected_gpu=0;
   }
#else
   /* Unsupported SDK/runtime selection cannot silently choose another GPU. */
   assert(!videotoolbox_encode_new_with_gpu(&c,1,&error) && error==kVTParameterErr && !live);
#endif
   {
      unsigned total;
      expected_id="advertised-hardware-encoder"; allocations=0;
      e=videotoolbox_encode_new_selected(&c,expected_id,0,&error); assert(e && !error);
      total=allocations; videotoolbox_encode_cancel(e);
      for (i=1;i<=total;i++)
      {
         allocations=0; allocation_fail=i;
         assert(!videotoolbox_encode_new_selected(&c,expected_id,0,&error) && error && !live);
      }
      allocation_fail=0;
      for (i=6;i<=8;i++)
      { fail_stage=i; assert(!videotoolbox_encode_new_selected(&c,expected_id,0,&error) && error && !live); }
      fail_stage=0; expected_id=NULL;
      assert(!videotoolbox_encode_new_selected(&c,"",0,&error) && error==kVTParameterErr && !live);
   }
   puts("PASS: VT adapter hardware admission, startup faults, FIFO, synchronous/drop callbacks and drain");
   return 0;
}
