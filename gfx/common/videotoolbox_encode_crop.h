/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VIDEOTOOLBOX_ENCODE_CROP_H
#define VIDEOTOOLBOX_ENCODE_CROP_H
#include "videotoolbox_encode_slots.h"
#include <formats/rh265_sps_crop.h>
#include <string.h>
/* Worker-only format conversion. VCL data, other parameter sets and unrelated
 * extensions are unchanged. The stale hvcC atom must be regenerated from the
 * rewritten SPS. Returned format is owned by the caller. */
static INLINE bool videotoolbox_encode_crop_format(CMVideoFormatDescriptionRef base,
      unsigned coded_width,unsigned coded_height,unsigned width,unsigned height,
      CMVideoFormatDescriptionRef *output)
{
   const uint8_t *sets[16];
   uint8_t *owned[16];
   size_t sizes[16],count=0,i;
   int length=0;
   bool found=false,ok=false;
   CFDictionaryRef extensions,old_atoms,generated_atoms;
   CFTypeRef configuration,clean_aperture;
   CMVideoFormatDescriptionRef generated=NULL;
   CFMutableDictionaryRef copy=NULL,atoms=NULL;
   if (output) *output=NULL;
   if (!base || !output) return false;
   memset(owned,0,sizeof(owned));
   for (i=0;!i || i<count;i++)
   {
      if (CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(base,i,&sets[i],&sizes[i],&count,&length)
            || !count || count>16 || length<1 || length>4 || sizes[i]<2
            || sizes[i]>RH265_SPS_CROP_MAX_NAL_SIZE) goto done;
      if (((sets[i][0]>>1)&63)==33)
      {
         if (!rh265_sps_crop(sets[i],sizes[i],coded_width,coded_height,width,height,&owned[i],&sizes[i])) goto done;
         sets[i]=owned[i]; found=true;
      }
   }
   if (!found) goto done;
   /* CoreMedia generates hvcC from the parameter sets only when not
    * overridden by SampleDescriptionExtensionAtoms in the extensions. */
   if (CMVideoFormatDescriptionCreateFromHEVCParameterSets(NULL,count,sets,sizes,length,NULL,&generated)
         || !generated) goto done;
   extensions=CMFormatDescriptionGetExtensions(base);
   if (extensions)
   {
      copy=CFDictionaryCreateMutableCopy(NULL,0,extensions); if (!copy) goto done;
      old_atoms=(CFDictionaryRef)CFDictionaryGetValue(copy,kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
      if (old_atoms && CFGetTypeID(old_atoms)!=CFDictionaryGetTypeID()) goto done;
      atoms=old_atoms?CFDictionaryCreateMutableCopy(NULL,0,old_atoms)
         :CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
      if (!atoms) goto done;
      generated_atoms=(CFDictionaryRef)CFDictionaryGetValue(CMFormatDescriptionGetExtensions(generated),
            kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms);
      if (!generated_atoms || CFGetTypeID(generated_atoms)!=CFDictionaryGetTypeID()) goto done;
      configuration=CFDictionaryGetValue(generated_atoms,CFSTR("hvcC")); if (!configuration) goto done;
      CFDictionarySetValue(atoms,CFSTR("hvcC"),configuration);
      /* The SPS now defines the visible rectangle. Original aperture offsets
       * refer to the larger encoder surface and would crop the MP4 twice.
       * Use CoreMedia's geometry derived from the rewritten SPS instead. */
      CFDictionaryRemoveValue(atoms,CFSTR("clap"));
      clean_aperture=CFDictionaryGetValue(CMFormatDescriptionGetExtensions(generated),
            kCMFormatDescriptionExtension_CleanAperture);
      if (clean_aperture) CFDictionarySetValue(copy,kCMFormatDescriptionExtension_CleanAperture,clean_aperture);
      else CFDictionaryRemoveValue(copy,kCMFormatDescriptionExtension_CleanAperture);
      CFDictionarySetValue(copy,kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,atoms);
      if (CMVideoFormatDescriptionCreateFromHEVCParameterSets(NULL,count,sets,sizes,length,copy,output)) goto done;
   }
   else { *output=generated; generated=NULL; }
   ok=*output!=NULL;
done:
   if (!ok && *output) { CFRelease(*output); *output=NULL; }
   if (generated) CFRelease(generated);
   if (atoms) CFRelease(atoms);
   if (copy) CFRelease(copy);
   for (i=0;i<16;i++) free(owned[i]);
   return ok;
}
static void videotoolbox_crop_copy_attachment(const void *key,const void *value,void *context)
{ CFDictionarySetValue((CFMutableDictionaryRef)context,key,value); }
/* Shares the original compressed data buffer; no picture-byte copy. */
static INLINE bool videotoolbox_encode_crop_sample(CMSampleBufferRef source,
      CMVideoFormatDescriptionRef format,CMSampleBufferRef *output)
{
   CMSampleTimingInfo timing;
   size_t size;
   CMBlockBufferRef block;
   CFArrayRef attachments,created;
   CFDictionaryRef metadata;
   unsigned i;
   CMAttachmentMode modes[2]={kCMAttachmentMode_ShouldPropagate,kCMAttachmentMode_ShouldNotPropagate};
   if (output) *output=NULL;
   if (!source || !format || !output || CMSampleBufferGetNumSamples(source)!=1
         || !CMSampleBufferDataIsReady(source)) return false;
   block=CMSampleBufferGetDataBuffer(source); size=CMSampleBufferGetSampleSize(source,0);
   if (!block || !size || CMSampleBufferGetSampleTimingInfo(source,0,&timing)
         || CMSampleBufferCreateReady(NULL,block,format,1,1,&timing,1,&size,output)) goto failed;
   if (CMSampleBufferGetDataBuffer(*output)!=block) goto failed;
   for (i=0;i<2;i++)
   {
      metadata=CMCopyDictionaryOfAttachments(NULL,source,modes[i]);
      if (metadata) { CMSetAttachments(*output,metadata,modes[i]); CFRelease(metadata); }
   }
   attachments=CMSampleBufferGetSampleAttachmentsArray(source,false);
   if (attachments)
   {
      CFDictionaryRef original;
      CFMutableDictionaryRef target;
      if (CFArrayGetCount(attachments)!=1) goto failed;
      created=CMSampleBufferGetSampleAttachmentsArray(*output,true);
      if (!created || CFArrayGetCount(created)!=1) goto failed;
      original=(CFDictionaryRef)CFArrayGetValueAtIndex(attachments,0);
      target=(CFMutableDictionaryRef)CFArrayGetValueAtIndex(created,0);
      if (!original || !target || CFGetTypeID(original)!=CFDictionaryGetTypeID()
            || CFGetTypeID(target)!=CFDictionaryGetTypeID()) goto failed;
      CFDictionaryRemoveAllValues(target);
      CFDictionaryApplyFunction(original,videotoolbox_crop_copy_attachment,target);
      if (!CFEqual(original,target)) goto failed;
   }
   return true;
failed:
   if (*output) { CFRelease(*output); *output=NULL; }
   return false;
}
#endif
