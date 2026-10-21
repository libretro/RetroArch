/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__<199901L) && !defined(inline)
#define inline __inline__
#define DEVICE_TEST_UNDEF_INLINE
#endif
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <VideoToolbox/VideoToolbox.h>
#ifdef DEVICE_TEST_UNDEF_INLINE
#undef inline
#undef DEVICE_TEST_UNDEF_INLINE
#endif
#include <assert.h>
#include <stdio.h>
#include <defines/cocoa_defines.h>
int main(void)
{
   @autoreleasepool
   {
      NSArray *devices=MTLCopyAllDevices();
      NSMutableArray *gpu_records=[NSMutableArray array];
      NSMutableArray *encoder_records=[NSMutableArray array];
      CFArrayRef encoders=NULL;
      CFIndex i;
      OSStatus status=VTCopyVideoEncoderList(NULL,&encoders);
      NSError *error=nil;
      NSData *json;
      for (id<MTLDevice> device in devices)
      {
         id unified=[NSNull null];
         if (__builtin_available(macOS 10.15,*)) unified=@(device.hasUnifiedMemory);
         [gpu_records addObject:@{@"name":device.name,@"registry_id":@(device.registryID),@"unified_memory":unified}];
      }
      if (!status && encoders) for (i=0;i<CFArrayGetCount(encoders);i++)
      {
         NSDictionary *entry=(__bridge NSDictionary*)CFArrayGetValueAtIndex(encoders,i);
         id gpu=entry[(__bridge NSString*)kVTVideoEncoderList_GPURegistryID];
         id hardware=entry[(__bridge NSString*)kVTVideoEncoderList_IsHardwareAccelerated];
         id codec=entry[(__bridge NSString*)kVTVideoEncoderList_CodecType];
         id identifier=entry[(__bridge NSString*)kVTVideoEncoderList_EncoderID];
         id properties=entry[(__bridge NSString*)kVTVideoEncoderList_SupportedSelectionProperties];
         [encoder_records addObject:@{@"id":identifier?identifier:@"",@"codec":codec?codec:[NSNull null],
            @"hardware":hardware?hardware:[NSNull null],@"gpu_registry_id":gpu?gpu:[NSNull null],
            @"selection_properties":properties?[properties description]:@""}];
      }
      json=[NSJSONSerialization dataWithJSONObject:@{@"diagnostic_only":@YES,@"status":@(status),
            @"metal_devices":gpu_records,@"encoders":encoder_records} options:NSJSONWritingPrettyPrinted error:&error];
      assert(json && !error && fwrite(json.bytes,1,json.length,stdout)==json.length);
      putchar('\n');
      if (encoders) CFRelease(encoders);
      RARCH_RELEASE(devices);
      return status?1:0;
   }
}
