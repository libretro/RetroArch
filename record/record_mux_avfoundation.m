/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
/* Keep modern SDK inline spellings local to framework declarations in C89. */
#if defined(__APPLE__) && !defined(__cplusplus) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L) && !defined(inline)
#define inline __inline__
#define RECORD_AVF_UNDEF_INLINE
#endif
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#ifdef RECORD_AVF_UNDEF_INLINE
#undef inline
#undef RECORD_AVF_UNDEF_INLINE
#endif
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <defines/cocoa_defines.h>
#include "record_mux_avfoundation.h"
#include "drivers/record_avfoundation_audio.h"
/* These synchronous worker-side APIs retain the existing deployment floor;
 * their newer replacements require recent OSes/Swift concurrency. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
struct record_mux_avfoundation
{
   CFTypeRef writer, video, audio;
   CMFormatDescriptionRef format, pcm;
   CMTime epoch, video_end;
   uint64_t audio_end;
   unsigned rate, channels, max_audio_frames;
   retro_atomic_int_t finished; /* 0 open, 1 callback pending, 2 done, 3 failed */
   bool have_video, have_audio, closing, cancelled, failed;
};
static void record_avf_destroy(record_mux_avfoundation_t *mux)
{
   if (!mux) return;
   if (mux->audio) CFRelease(mux->audio);
   if (mux->video) CFRelease(mux->video);
   if (mux->writer) CFRelease(mux->writer);
   if (mux->format) CFRelease(mux->format);
   if (mux->pcm) CFRelease(mux->pcm);
   free(mux);
}
record_mux_avfoundation_t *record_mux_avfoundation_new(
      const record_mux_avfoundation_config_t *c,const char *path)
{
   record_mux_avfoundation_t *mux;
   unsigned count=0, mask;
   if (!c || !c->video_format || !path || !*path || c->epoch<0
         || !c->timescale || c->timescale>INT32_MAX) return NULL;
   mask=c->audio_layout;
   while (mask) { count+=mask&1; mask>>=1; }
   if (c->channels && (c->channels>8 || count!=c->channels || (c->audio_layout&~0x63fu)
            || c->audio_rate<8000 || c->audio_rate>192000 || c->audio_bitrate<32000
            || c->audio_bitrate>512000 || !c->max_audio_frames || c->max_audio_frames>65536)) return NULL;
   if (!c->channels && (c->audio_rate || c->audio_layout || c->audio_bitrate || c->max_audio_frames)) return NULL;
   if (CMFormatDescriptionGetMediaType((CMFormatDescriptionRef)c->video_format)!=kCMMediaType_Video
         || (CMFormatDescriptionGetMediaSubType((CMFormatDescriptionRef)c->video_format)!=kCMVideoCodecType_H264
            && CMFormatDescriptionGetMediaSubType((CMFormatDescriptionRef)c->video_format)!=kCMVideoCodecType_HEVC)) return NULL;
   mux=(record_mux_avfoundation_t*)calloc(1,sizeof(*mux)); if (!mux) return NULL;
   mux->format=(CMFormatDescriptionRef)CFRetain(c->video_format);
   mux->epoch=CMTimeMake(c->epoch,c->timescale); mux->rate=c->audio_rate;
   mux->channels=c->channels; mux->max_audio_frames=c->max_audio_frames;
   retro_atomic_int_init(&mux->finished,0);
   @autoreleasepool
   {
      NSError *error=nil;
      NSString *filename=[NSString stringWithUTF8String:path];
      AVAssetWriter *writer;
      AVAssetWriterInput *video;
      if (!filename || [[NSFileManager defaultManager] fileExistsAtPath:filename]) goto fail;
      writer=[[AVAssetWriter alloc] initWithURL:[NSURL fileURLWithPath:filename] fileType:AVFileTypeMPEG4 error:&error];
      if (writer) mux->writer=(CFTypeRef)RARCH_BRIDGE_RETAINED(writer);
      RARCH_RELEASE(writer); if (!mux->writer || error) goto fail;
      writer=(__bridge AVAssetWriter*)mux->writer;
      video=[[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo outputSettings:nil sourceFormatHint:mux->format];
      if (video) mux->video=(CFTypeRef)RARCH_BRIDGE_RETAINED(video);
      RARCH_RELEASE(video); if (!mux->video) goto fail;
      video=(__bridge AVAssetWriterInput*)mux->video; video.expectsMediaDataInRealTime=YES;
      video.mediaTimeScale=(CMTimeScale)c->timescale;
      if (![writer canAddInput:video]) goto fail;
      [writer addInput:video];
      if (c->channels)
      {
         AudioStreamBasicDescription pcm;
         AudioChannelLayout *layout;
         size_t layout_size=offsetof(AudioChannelLayout,mChannelDescriptions)
            +c->channels*sizeof(AudioChannelDescription);
         unsigned bit,index=0;
         OSStatus status;
         NSData *layout_data;
         NSDictionary *settings;
         AVAssetWriterInput *audio;
         memset(&pcm,0,sizeof(pcm));
         layout=(AudioChannelLayout*)calloc(1,layout_size); if (!layout) goto fail;
         layout->mChannelLayoutTag=kAudioChannelLayoutTag_UseChannelDescriptions;
         layout->mNumberChannelDescriptions=c->channels;
         for (bit=0;bit<=10;bit++)
            if (c->audio_layout&((unsigned)1<<bit))
            {
               AudioChannelLabel label=kAudioChannelLabel_Unknown;
               switch (bit)
               {
                  case 0: label=kAudioChannelLabel_Left; break;
                  case 1: label=kAudioChannelLabel_Right; break;
                  case 2: label=kAudioChannelLabel_Center; break;
                  case 3: label=kAudioChannelLabel_LFEScreen; break;
                  /* Core Audio documents WAVE back/side as Surround versus
                   * SurroundDirect, respectively. */
                  case 4: label=kAudioChannelLabel_LeftSurround; break;
                  case 5: label=kAudioChannelLabel_RightSurround; break;
                  case 9: label=kAudioChannelLabel_LeftSurroundDirect; break;
                  case 10: label=kAudioChannelLabel_RightSurroundDirect; break;
               }
               layout->mChannelDescriptions[index++].mChannelLabel=label;
            }
         pcm.mSampleRate=c->audio_rate; pcm.mFormatID=kAudioFormatLinearPCM;
         pcm.mFormatFlags=kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
         pcm.mBytesPerPacket=pcm.mBytesPerFrame=2*c->channels; pcm.mFramesPerPacket=1;
         pcm.mChannelsPerFrame=c->channels; pcm.mBitsPerChannel=16;
         status=CMAudioFormatDescriptionCreate(NULL,&pcm,layout_size,layout,0,NULL,NULL,&mux->pcm);
         layout_data=[NSData dataWithBytes:layout length:layout_size]; free(layout);
         if (status || !layout_data) goto fail;
         settings=@{AVFormatIDKey:@(kAudioFormatMPEG4AAC),
            AVSampleRateKey:@(avfoundation_nearest_aac_sample_rate(c->audio_rate)),
            AVNumberOfChannelsKey:@(c->channels), AVChannelLayoutKey:layout_data, AVEncoderBitRateKey:@(c->audio_bitrate)};
         audio=[[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeAudio outputSettings:settings sourceFormatHint:mux->pcm];
         if (audio) mux->audio=(CFTypeRef)RARCH_BRIDGE_RETAINED(audio);
         RARCH_RELEASE(audio); if (!mux->audio) goto fail;
         audio=(__bridge AVAssetWriterInput*)mux->audio; audio.expectsMediaDataInRealTime=YES;
         if (![writer canAddInput:audio]) goto fail;
         [writer addInput:audio];
      }
      if (![writer startWriting]) goto fail;
      [writer startSessionAtSourceTime:mux->epoch];
   }
   return mux;
fail:
   if (mux->writer && [(__bridge AVAssetWriter*)mux->writer status]==AVAssetWriterStatusWriting)
      [(__bridge AVAssetWriter*)mux->writer cancelWriting];
   record_avf_destroy(mux); return NULL;
}
int record_mux_avfoundation_video(record_mux_avfoundation_t *mux,void *data)
{
   CMSampleBufferRef sample=(CMSampleBufferRef)data;
   CMTime pts,duration,end,dts;
   AVAssetWriter *writer;
   AVAssetWriterInput *input;
   if (!mux || mux->closing || mux->failed || !sample || !CMSampleBufferDataIsReady(sample)
         || CMSampleBufferGetNumSamples(sample)!=1 || !CMSampleBufferGetFormatDescription(sample)
         || !CMFormatDescriptionEqual(mux->format,CMSampleBufferGetFormatDescription(sample))) return RECORD_AVF_ERROR;
   pts=CMSampleBufferGetPresentationTimeStamp(sample); duration=CMSampleBufferGetDuration(sample);
   dts=CMSampleBufferGetDecodeTimeStamp(sample); end=CMTimeAdd(pts,duration);
   if (!CMTIME_IS_NUMERIC(pts) || !CMTIME_IS_NUMERIC(duration) || !CMTIME_IS_NUMERIC(end)
         || pts.epoch || duration.epoch || end.epoch
         || CMTimeCompare(duration,kCMTimeZero)<=0 || CMTimeCompare(pts,mux->epoch)<0
         || (CMTIME_IS_VALID(dts) && (!CMTIME_IS_NUMERIC(dts) || dts.epoch || CMTimeCompare(dts,pts)))
         || (mux->have_video && CMTimeCompare(pts,mux->video_end)<0)) return RECORD_AVF_ERROR;
   writer=(__bridge AVAssetWriter*)mux->writer; input=(__bridge AVAssetWriterInput*)mux->video;
   if (writer.status!=AVAssetWriterStatusWriting) { mux->failed=true; return RECORD_AVF_ERROR; }
   if (!input.readyForMoreMediaData) return RECORD_AVF_BUSY;
   if (![input appendSampleBuffer:sample]) { mux->failed=true; return RECORD_AVF_ERROR; }
   mux->video_end=end; mux->have_video=true; return RECORD_AVF_OK;
}
int record_mux_avfoundation_audio(record_mux_avfoundation_t *mux,
      const int16_t *samples,size_t frames,uint64_t timestamp)
{
   CMBlockBufferRef block=NULL;
   CMSampleBufferRef sample=NULL;
   AVAssetWriter *writer;
   AVAssetWriterInput *input;
   CMTime pts;
   size_t size;
   OSStatus status;
   int result=RECORD_AVF_ERROR;
   if (!mux || !mux->audio || mux->closing || mux->failed || !samples || !frames
         || frames>mux->max_audio_frames || frames>(size_t)-1/2/mux->channels
         || timestamp>((uint64_t)1<<63)-1-frames
         || (mux->have_audio && timestamp<mux->audio_end)) return RECORD_AVF_ERROR;
   pts=CMTimeMake((int64_t)timestamp,mux->rate);
   if (CMTimeCompare(pts,mux->epoch)<0) return RECORD_AVF_ERROR;
   writer=(__bridge AVAssetWriter*)mux->writer; input=(__bridge AVAssetWriterInput*)mux->audio;
   if (writer.status!=AVAssetWriterStatusWriting) { mux->failed=true; return RECORD_AVF_ERROR; }
   if (!input.readyForMoreMediaData) return RECORD_AVF_BUSY;
   size=frames*2*mux->channels;
   status=CMBlockBufferCreateWithMemoryBlock(NULL,NULL,size,NULL,NULL,0,size,kCMBlockBufferAssureMemoryNowFlag,&block);
   if (!status) status=CMBlockBufferReplaceDataBytes(samples,block,0,size);
   if (!status) status=CMAudioSampleBufferCreateWithPacketDescriptions(NULL,block,true,NULL,NULL,
         mux->pcm,(CMItemCount)frames,pts,NULL,&sample);
   if (!status && [input appendSampleBuffer:sample])
   { mux->audio_end=timestamp+frames; mux->have_audio=true; result=RECORD_AVF_OK; }
   else mux->failed=true;
   if (sample) CFRelease(sample);
   if (block) CFRelease(block);
   return result;
}
int record_mux_avfoundation_finish(record_mux_avfoundation_t *mux)
{
   int state;
   if (!mux || mux->cancelled || mux->failed) return RECORD_AVF_ERROR;
   state=retro_atomic_load_acquire_int(&mux->finished);
   if (!state)
   {
      AVAssetWriter *writer=(__bridge AVAssetWriter*)mux->writer;
      if (!mux->have_video || writer.status!=AVAssetWriterStatusWriting) return RECORD_AVF_ERROR;
      mux->closing=true; retro_atomic_store_release_int(&mux->finished,1);
      {
         CMTime end=mux->video_end;
         if (mux->have_audio)
         {
            CMTime audio_end=CMTimeMake((int64_t)mux->audio_end,mux->rate);
            if (CMTimeCompare(audio_end,end)>0) end=audio_end;
         }
         [writer endSessionAtSourceTime:end];
      }
      [(__bridge AVAssetWriterInput*)mux->video markAsFinished];
      if (mux->audio) [(__bridge AVAssetWriterInput*)mux->audio markAsFinished];
      [writer finishWritingWithCompletionHandler:^{
         int done=writer.status==AVAssetWriterStatusCompleted?2:3;
         retro_atomic_store_release_int(&mux->finished,done);
      }];
      state=retro_atomic_load_acquire_int(&mux->finished);
   }
   return state==1?RECORD_AVF_BUSY:state==2?RECORD_AVF_OK:RECORD_AVF_ERROR;
}
bool record_mux_avfoundation_cancel(record_mux_avfoundation_t *mux)
{
   int state;
   if (!mux) return true;
   state=retro_atomic_load_acquire_int(&mux->finished);
   if (state==1) return false;
   mux->closing=mux->cancelled=true;
   if (!state) [(__bridge AVAssetWriter*)mux->writer cancelWriting];
   return true;
}
bool record_mux_avfoundation_free(record_mux_avfoundation_t *mux)
{
   int state;
   if (!mux) return true;
   state=retro_atomic_load_acquire_int(&mux->finished);
   if (state==1 || (!mux->cancelled && state!=2 && state!=3)) return false;
   record_avf_destroy(mux); return true;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
