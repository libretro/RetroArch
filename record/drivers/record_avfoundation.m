/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 - RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>
#import <AudioToolbox/AudioToolbox.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <string/stdstring.h>

#include "record_avfoundation.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct record_avfoundation
{
   AVAssetWriter *assetWriter;
   AVAssetWriterInput *videoInput;
   AVAssetWriterInput *audioInput;
   AVAssetWriterInputPixelBufferAdaptor *pixelBufferAdaptor;

   /* Timing */
   CMTime lastVideoTime;
   CMTime lastAudioTime;
   int64_t videoFrameCount;
   int64_t audioSampleCount;
   double fps;
   double sourceSampleRate; /* actual rate of incoming audio data */
   double outputSampleRate; /* valid AAC rate for encoder output */
   bool hasStarted;

   /* Video properties */
   unsigned width;
   unsigned height;
   enum ffemu_pix_format pix_fmt;
   struct scaler_ctx scaler;
   unsigned scaler_in_width;
   unsigned scaler_in_height;

   /* Audio properties */
   unsigned channels;

   /* Dispatch queue for encoding */
   dispatch_queue_t encodingQueue;
} record_avfoundation_t;

/* Query AudioToolbox for the actual valid AAC encode sample rates
 * on this system, then pick the nearest one to the requested rate. */
static double avfoundation_nearest_aac_sample_rate(double rate)
{
   AudioFormatPropertyID prop = kAudioFormatProperty_AvailableEncodeSampleRates;
   AudioStreamBasicDescription desc;
   UInt32 size = 0;
   OSStatus status;

   memset(&desc, 0, sizeof(desc));
   desc.mFormatID = kAudioFormatMPEG4AAC;

   status = AudioFormatGetPropertyInfo(prop, sizeof(desc), &desc, &size);
   if (status == noErr && size > 0)
   {
      unsigned count = size / sizeof(AudioValueRange);
      AudioValueRange *ranges = (AudioValueRange*)malloc(size);
      if (ranges)
      {
         status = AudioFormatGetProperty(prop, sizeof(desc), &desc,
               &size, ranges);
         if (status == noErr)
         {
            double best      = 44100.0;
            double best_dist = 1e9;
            unsigned i;
            for (i = 0; i < count; i++)
            {
               /* Clamp rate into this range's bounds, then measure distance */
               double lo      = ranges[i].mMinimum;
               double hi      = ranges[i].mMaximum;
               double clamped = rate < lo ? lo : (rate > hi ? hi : rate);
               double dist    = rate > clamped
                              ? rate - clamped : clamped - rate;
               if (dist < best_dist)
               {
                  best_dist = dist;
                  best      = clamped;
               }
            }
            free(ranges);
            return best;
         }
         free(ranges);
      }
   }

   /* Fallback: common safe rates if query fails */
   {
      static const double fallback[] = {
         8000.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0,
         32000.0, 44100.0, 48000.0
      };
      double best      = 44100.0;
      double best_dist = 1e9;
      unsigned i;
      for (i = 0; i < sizeof(fallback) / sizeof(fallback[0]); i++)
      {
         double dist = rate > fallback[i]
                     ? rate - fallback[i] : fallback[i] - rate;
         if (dist < best_dist)
         {
            best_dist = dist;
            best      = fallback[i];
         }
      }
      return best;
   }
}

static void avfoundation_release_handle(record_avfoundation_t *handle)
{
   if (!handle)
      return;
   scaler_ctx_gen_reset(&handle->scaler);
   handle->pixelBufferAdaptor = nil;
   handle->videoInput         = nil;
   handle->audioInput         = nil;
   handle->assetWriter        = nil;
   handle->encodingQueue      = nil;
   free(handle);
}

static void *avfoundation_record_init(const struct record_params *params)
{
   record_avfoundation_t *handle = NULL;

   if (!params || !params->filename)
   {
      RARCH_ERR("[AVFoundation] Invalid parameters\n");
      return NULL;
   }

   handle = (record_avfoundation_t*)calloc(1, sizeof(record_avfoundation_t));
   if (!handle)
   {
      RARCH_ERR("[AVFoundation] Failed to allocate handle\n");
      return NULL;
   }

   @autoreleasepool
   {
      NSError *error                     = nil;
      NSDictionary *videoSettings        = nil;
      NSDictionary *audioSettings        = nil;
      NSDictionary *pixelBufferAttributes = nil;
      NSDictionary *compressionProperties = nil;

      /* Store parameters — apply scale factor to output dimensions */
      {
         unsigned scale = params->video_record_scale_factor > 0
                        ? params->video_record_scale_factor : 1;
         handle->width  = params->out_width  * scale;
         handle->height = params->out_height * scale;
         /* H.264 requires even dimensions */
         handle->width  = (handle->width  + 1) & ~1;
         handle->height = (handle->height + 1) & ~1;
      }
      handle->fps        = params->fps;
      handle->channels   = params->channels;
      handle->pix_fmt    = params->pix_fmt;
      handle->hasStarted = false;

      /* Source rate is the actual rate of incoming PCM data.
       * Output rate must be a valid AAC rate; the encoder
       * handles the conversion internally. */
      handle->sourceSampleRate = params->samplerate;
      handle->outputSampleRate = avfoundation_nearest_aac_sample_rate(
            params->samplerate);
      RARCH_LOG("[AVFoundation] Audio: source %.2f Hz, AAC output %.0f Hz\n",
                handle->sourceSampleRate, handle->outputSampleRate);

      /* Create output URL */
      NSURL *outputURL = [NSURL fileURLWithPath:@(params->filename)];

      /* Determine file type from extension */
      NSString *pathExtension = [[outputURL pathExtension] lowercaseString];
      AVFileType fileType     = AVFileTypeQuickTimeMovie;

      if (   [pathExtension isEqualToString:@"mp4"]
          || [pathExtension isEqualToString:@"m4v"])
         fileType = AVFileTypeMPEG4;

      /* Create asset writer */
      handle->assetWriter = [[AVAssetWriter alloc] initWithURL:outputURL
                                                      fileType:fileType
                                                         error:&error];
      if (error)
      {
         RARCH_ERR("[AVFoundation] Failed to create asset writer: %s\n",
                   [[error localizedDescription] UTF8String]);
         free(handle);
         return NULL;
      }

      /* Select bitrate based on recording quality preset */
      unsigned videoBitRate;
      unsigned audioBitRate;
      switch (params->preset)
      {
         case RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY:
            videoBitRate = 2000000;
            audioBitRate = 96000;
            break;
         case RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY:
         case RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY:
            videoBitRate = 10000000;
            audioBitRate = 192000;
            break;
         case RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY:
         default:
            videoBitRate = 5000000;
            audioBitRate = 128000;
            break;
      }

      /* Configure video codec */
      NSString *videoCodec = AVVideoCodecTypeH264;
      bool useHEVC         = false;
#if defined(MAC_OS_X_VERSION_10_13) || defined(__IPHONE_11_0)
      if (@available(macOS 10.13, iOS 11.0, tvOS 11.0, *))
      {
         if (params->preset == RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY
               || params->preset == RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY)
         {
            videoCodec = AVVideoCodecTypeHEVC;
            useHEVC    = true;
         }
      }
#endif

      /* Build compression properties depending on codec */
      if (useHEVC)
      {
         compressionProperties = @{
            AVVideoAverageBitRateKey: @(videoBitRate),
            AVVideoExpectedSourceFrameRateKey: @(handle->fps),
            AVVideoMaxKeyFrameIntervalKey: @(60)
         };
      }
      else
      {
         compressionProperties = @{
            AVVideoAverageBitRateKey: @(videoBitRate),
            AVVideoExpectedSourceFrameRateKey: @(handle->fps),
            AVVideoMaxKeyFrameIntervalKey: @(60),
            AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel
         };
      }

      videoSettings = @{
         AVVideoCodecKey: videoCodec,
         AVVideoWidthKey: @(handle->width),
         AVVideoHeightKey: @(handle->height),
         AVVideoCompressionPropertiesKey: compressionProperties
      };

      /* Create video input */
      handle->videoInput = [[AVAssetWriterInput alloc]
                            initWithMediaType:AVMediaTypeVideo
                            outputSettings:videoSettings];
      handle->videoInput.expectsMediaDataInRealTime = YES;

      /* Configure pixel buffer attributes */
      pixelBufferAttributes = @{
         (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
         (NSString*)kCVPixelBufferWidthKey: @(handle->width),
         (NSString*)kCVPixelBufferHeightKey: @(handle->height),
         (NSString*)kCVPixelBufferIOSurfacePropertiesKey: @{}
      };

      /* Create pixel buffer adaptor */
      handle->pixelBufferAdaptor = [[AVAssetWriterInputPixelBufferAdaptor alloc]
                                    initWithAssetWriterInput:handle->videoInput
                                    sourcePixelBufferAttributes:pixelBufferAttributes];

      /* Add video input to writer */
      if ([handle->assetWriter canAddInput:handle->videoInput])
         [handle->assetWriter addInput:handle->videoInput];
      else
      {
         RARCH_ERR("[AVFoundation] Cannot add video input\n");
         avfoundation_release_handle(handle);
         return NULL;
      }

      /* Configure audio settings */
      if (handle->channels > 0 && handle->sourceSampleRate > 0)
      {
         AudioChannelLayout channelLayout;
         memset(&channelLayout, 0, sizeof(channelLayout));
         channelLayout.mChannelLayoutTag = handle->channels == 1
                                         ? kAudioChannelLayoutTag_Mono
                                         : kAudioChannelLayoutTag_Stereo;

         NSData *channelLayoutData = [NSData dataWithBytes:&channelLayout
                                                    length:sizeof(channelLayout)];

         audioSettings = @{
            AVFormatIDKey: @(kAudioFormatMPEG4AAC),
            AVSampleRateKey: @(handle->outputSampleRate),
            AVNumberOfChannelsKey: @(handle->channels),
            AVChannelLayoutKey: channelLayoutData,
            AVEncoderBitRateKey: @(audioBitRate)
         };

         /* Create audio input */
         handle->audioInput = [[AVAssetWriterInput alloc]
                               initWithMediaType:AVMediaTypeAudio
                               outputSettings:audioSettings];
         handle->audioInput.expectsMediaDataInRealTime = YES;

         /* Add audio input to writer */
         if ([handle->assetWriter canAddInput:handle->audioInput])
            [handle->assetWriter addInput:handle->audioInput];
         else
         {
            RARCH_WARN("[AVFoundation] Cannot add audio input, continuing without audio\n");
            handle->audioInput = nil;
         }
      }

      /* Create encoding queue */
      handle->encodingQueue = dispatch_queue_create(
            "com.retroarch.avfoundation.encoding",
            DISPATCH_QUEUE_SERIAL);

      /* Start writing - pool becomes available after this */
      if (![handle->assetWriter startWriting])
      {
         RARCH_ERR("[AVFoundation] Failed to start writing: %s\n",
                   [[handle->assetWriter.error localizedDescription] UTF8String]);
         avfoundation_release_handle(handle);
         return NULL;
      }

      RARCH_LOG("[AVFoundation] Initialized recording to %s (%ux%u @ %.2f fps, "
                "video %u kbps, audio %u kbps)\n",
                params->filename, handle->width, handle->height,
                handle->fps, videoBitRate / 1000, audioBitRate / 1000);
   }

   return handle;
}

static void avfoundation_record_free(void *data)
{
   record_avfoundation_t *handle = (record_avfoundation_t*)data;

   if (!handle)
      return;

   @autoreleasepool
   {
      /* Finalize if still writing */
      if (   handle->assetWriter
          && handle->assetWriter.status == AVAssetWriterStatusWriting)
      {
         dispatch_semaphore_t sem = dispatch_semaphore_create(0);

         if (handle->encodingQueue)
         {
            dispatch_sync(handle->encodingQueue, ^{
               [handle->videoInput markAsFinished];
               if (handle->audioInput)
                  [handle->audioInput markAsFinished];
            });
         }

         [handle->assetWriter finishWritingWithCompletionHandler:^{
            dispatch_semaphore_signal(sem);
         }];

         dispatch_semaphore_wait(sem,
               dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
      }

      /* Drain encoding queue */
      if (handle->encodingQueue)
         dispatch_sync(handle->encodingQueue, ^{});

      avfoundation_release_handle(handle);
   }
}

static bool avfoundation_record_push_video(void *data,
      const struct record_video_data *video_data)
{
   record_avfoundation_t *handle = (record_avfoundation_t*)data;
   CVPixelBufferRef pixelBuffer  = NULL;
   CVReturn result;

   if (!handle || !video_data)
      return false;

   if (video_data->is_dupe)
      return true;

   if (!video_data->data || video_data->pitch == 0)
      return false;

   @autoreleasepool
   {
      /* Start session on first frame */
      if (!handle->hasStarted)
      {
         CMTime startTime = CMTimeMake(0, (int32_t)(handle->fps * 1000));
         [handle->assetWriter startSessionAtSourceTime:startTime];
         handle->hasStarted      = true;
         handle->videoFrameCount = 0;
         handle->audioSampleCount = 0;
      }

      /* Presentation time based on frame count for monotonic timestamps */
      CMTime presentationTime = CMTimeMake(
            handle->videoFrameCount * 1000,
            (int32_t)(handle->fps * 1000));
      handle->videoFrameCount++;

      /* Get pixel buffer from pool (available after startWriting+startSession) */
      CVPixelBufferPoolRef pool = handle->pixelBufferAdaptor.pixelBufferPool;
      if (pool)
      {
         result = CVPixelBufferPoolCreatePixelBuffer(NULL, pool, &pixelBuffer);
         if (result != kCVReturnSuccess)
            pool = NULL;
      }

      if (!pool)
      {
         result = CVPixelBufferCreate(NULL,
               handle->width, handle->height,
               kCVPixelFormatType_32BGRA,
               NULL, &pixelBuffer);
         if (result != kCVReturnSuccess)
         {
            RARCH_WARN("[AVFoundation] Failed to create pixel buffer\n");
            return false;
         }
      }

      CVPixelBufferLockBaseAddress(pixelBuffer, 0);

      void  *baseAddress = CVPixelBufferGetBaseAddress(pixelBuffer);
      size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);

      /* Clamp destination to source size (like ffmpeg does) */
      unsigned dst_w = video_data->width < handle->width
                     ? video_data->width : handle->width;
      unsigned dst_h = video_data->height < handle->height
                     ? video_data->height : handle->height;
      bool shrunk    = dst_w < video_data->width
                     || dst_h < video_data->height;

      /* Regenerate scaler filter when input dimensions change */
      if (   handle->scaler_in_width  != video_data->width
          || handle->scaler_in_height != video_data->height)
      {
         struct scaler_ctx *scaler = &handle->scaler;
         scaler_ctx_gen_reset(scaler);

         scaler->in_width   = video_data->width;
         scaler->in_height  = video_data->height;
         scaler->in_stride  = abs(video_data->pitch);
         scaler->out_width  = dst_w;
         scaler->out_height = dst_h;
         scaler->out_stride = (int)bytesPerRow;
         scaler->out_fmt    = SCALER_FMT_ARGB8888;
         scaler->scaler_type = shrunk
                             ? SCALER_TYPE_BILINEAR : SCALER_TYPE_POINT;

         switch (handle->pix_fmt)
         {
            case FFEMU_PIX_RGB565:
               scaler->in_fmt = SCALER_FMT_RGB565;
               break;
            case FFEMU_PIX_BGR24:
               scaler->in_fmt = SCALER_FMT_BGR24;
               break;
            case FFEMU_PIX_ARGB8888:
            default:
               scaler->in_fmt = SCALER_FMT_ARGB8888;
               break;
         }

         if (!scaler_ctx_gen_filter(scaler))
         {
            RARCH_ERR("[AVFoundation] Failed to generate scaler filter\n");
            CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
            CVPixelBufferRelease(pixelBuffer);
            return false;
         }

         handle->scaler_in_width  = video_data->width;
         handle->scaler_in_height = video_data->height;
      }

      {
         struct scaler_ctx *scaler = &handle->scaler;
         const void *frame_data    = video_data->data;
         bool flip                 = video_data->pitch < 0;

         /* Negative pitch means the frame is bottom-up (GPU readback).
          * The data pointer is at the last row; walk back to the first
          * so the scaler can read forward without overrunning the buffer. */
         if (flip)
            frame_data = (const uint8_t*)video_data->data
                       + (int)video_data->pitch * ((int)video_data->height - 1);

         scaler->in_stride  = abs(video_data->pitch);
         scaler->out_stride = (int)bytesPerRow;
         scaler_ctx_scale_direct(scaler, baseAddress, frame_data);

         /* The scaler wrote rows top-to-bottom from the buffer start,
          * but the negative pitch meant the image should be flipped.
          * Swap rows in-place to restore correct orientation. */
         if (flip)
         {
            uint8_t *top = (uint8_t*)baseAddress;
            uint8_t *bot = top + bytesPerRow * (dst_h - 1);
            while (top < bot)
            {
               for (size_t i = 0; i < bytesPerRow; i++)
               {
                  uint8_t tmp = top[i];
                  top[i]      = bot[i];
                  bot[i]      = tmp;
               }
               top += bytesPerRow;
               bot -= bytesPerRow;
            }
         }
      }

      CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

      /* Append pixel buffer on encoding queue */
      dispatch_sync(handle->encodingQueue, ^{
         if (handle->videoInput.readyForMoreMediaData)
            [handle->pixelBufferAdaptor appendPixelBuffer:pixelBuffer
                                  withPresentationTime:presentationTime];
      });

      CVPixelBufferRelease(pixelBuffer);
   }

   return true;
}

static bool avfoundation_record_push_audio(void *data,
      const struct record_audio_data *audio_data)
{
   record_avfoundation_t *handle = (record_avfoundation_t*)data;
   CMSampleBufferRef sampleBuffer       = NULL;
   CMBlockBufferRef blockBuffer         = NULL;
   CMFormatDescriptionRef formatDesc    = NULL;
   OSStatus status;

   if (!handle || !handle->audioInput || !audio_data || !audio_data->data)
      return false;

   if (!handle->hasStarted)
      return false;

   @autoreleasepool
   {
      size_t dataSize = audio_data->frames * sizeof(int16_t) * handle->channels;

      /* Presentation time from cumulative sample count at source rate */
      CMTime presentationTime = CMTimeMake(
            handle->audioSampleCount, (int32_t)handle->sourceSampleRate);
      handle->audioSampleCount += (int64_t)audio_data->frames;

      /* Create format description for raw PCM input at source rate.
       * AVAssetWriter handles conversion to the AAC output rate. */
      AudioStreamBasicDescription audioFormat;
      memset(&audioFormat, 0, sizeof(audioFormat));
      audioFormat.mSampleRate       = handle->sourceSampleRate;
      audioFormat.mFormatID         = kAudioFormatLinearPCM;
      audioFormat.mFormatFlags      = kAudioFormatFlagIsSignedInteger
                                    | kAudioFormatFlagIsPacked;
      audioFormat.mBytesPerPacket   = sizeof(int16_t) * handle->channels;
      audioFormat.mFramesPerPacket  = 1;
      audioFormat.mBytesPerFrame    = sizeof(int16_t) * handle->channels;
      audioFormat.mChannelsPerFrame = handle->channels;
      audioFormat.mBitsPerChannel   = 16;

      status = CMAudioFormatDescriptionCreate(NULL, &audioFormat,
            0, NULL, 0, NULL, NULL, &formatDesc);
      if (status != noErr)
      {
         RARCH_WARN("[AVFoundation] Failed to create audio format description\n");
         return false;
      }

      /* Create block buffer with copy of audio data */
      status = CMBlockBufferCreateWithMemoryBlock(NULL,
            NULL, dataSize, NULL, NULL, 0, dataSize,
            kCMBlockBufferAssureMemoryNowFlag, &blockBuffer);
      if (status != noErr)
      {
         CFRelease(formatDesc);
         RARCH_WARN("[AVFoundation] Failed to create block buffer\n");
         return false;
      }

      status = CMBlockBufferReplaceDataBytes(
            audio_data->data, blockBuffer, 0, dataSize);
      if (status != noErr)
      {
         CFRelease(blockBuffer);
         CFRelease(formatDesc);
         RARCH_WARN("[AVFoundation] Failed to copy audio data\n");
         return false;
      }

      /* Create sample buffer */
      status = CMAudioSampleBufferCreateWithPacketDescriptions(NULL,
            blockBuffer, true, NULL, NULL, formatDesc,
            (CMItemCount)audio_data->frames,
            presentationTime, NULL, &sampleBuffer);

      CFRelease(blockBuffer);
      CFRelease(formatDesc);

      if (status != noErr)
      {
         RARCH_WARN("[AVFoundation] Failed to create audio sample buffer\n");
         return false;
      }

      /* Append on encoding queue */
      dispatch_sync(handle->encodingQueue, ^{
         if (handle->audioInput.readyForMoreMediaData)
            [handle->audioInput appendSampleBuffer:sampleBuffer];
      });

      CFRelease(sampleBuffer);
   }

   return true;
}

static bool avfoundation_record_finalize(void *data)
{
   record_avfoundation_t *handle = (record_avfoundation_t*)data;
   __block BOOL success          = NO;

   if (!handle || !handle->assetWriter)
      return false;

   if (handle->assetWriter.status != AVAssetWriterStatusWriting)
      return false;

   @autoreleasepool
   {
      dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

      RARCH_LOG("[AVFoundation] Finalizing recording...\n");

      /* Mark inputs as finished */
      dispatch_sync(handle->encodingQueue, ^{
         [handle->videoInput markAsFinished];
         if (handle->audioInput)
            [handle->audioInput markAsFinished];
      });

      /* Finish writing */
      [handle->assetWriter finishWritingWithCompletionHandler:^{
         if (handle->assetWriter.status == AVAssetWriterStatusCompleted)
         {
            RARCH_LOG("[AVFoundation] Recording completed successfully\n");
            success = YES;
         }
         else if (handle->assetWriter.status == AVAssetWriterStatusFailed)
            RARCH_ERR("[AVFoundation] Recording failed: %s\n",
                     [[handle->assetWriter.error localizedDescription] UTF8String]);
         else
            RARCH_WARN("[AVFoundation] Recording ended with status: %ld\n",
                      (long)handle->assetWriter.status);
         dispatch_semaphore_signal(semaphore);
      }];

      dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC);
      if (dispatch_semaphore_wait(semaphore, timeout) != 0)
      {
         RARCH_WARN("[AVFoundation] Timeout waiting for finalization\n");
         return false;
      }
   }

   return success;
}

const record_driver_t record_avfoundation = {
   avfoundation_record_init,
   avfoundation_record_free,
   avfoundation_record_push_video,
   avfoundation_record_push_audio,
   avfoundation_record_finalize,
   "avfoundation"
};
