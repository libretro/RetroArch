/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_android_image.h"
static int fail_at,step,live,bad;
static VkDevice device;
#define HANDLE(t,n) ((t)(uintptr_t)(n))
static VkResult action(void){return ++step==fail_at?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS;}
static VKAPI_ATTR VkResult VKAPI_CALL properties(VkDevice d,const struct AHardwareBuffer *b,VkAndroidHardwareBufferPropertiesANDROID *p)
{
 VkAndroidHardwareBufferFormatPropertiesANDROID *f=(VkAndroidHardwareBufferFormatPropertiesANDROID*)p->pNext;
 assert(d==device && b && p->sType==VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID);
 if(action()!=VK_SUCCESS)return VK_ERROR_INVALID_EXTERNAL_HANDLE;
 p->allocationSize=4096;p->memoryTypeBits=bad?0:2;
 f->externalFormat=123;f->formatFeatures=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;
 f->suggestedXChromaOffset=f->suggestedYChromaOffset=VK_CHROMA_LOCATION_MIDPOINT;
 return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL image(VkDevice d,const VkImageCreateInfo *p,const VkAllocationCallbacks *a,VkImage *out)
{
 const VkExternalMemoryImageCreateInfo *e=(const VkExternalMemoryImageCreateInfo*)p->pNext;
 const VkExternalFormatANDROID *f=(const VkExternalFormatANDROID*)e->pNext;
 assert(d==device && !a && p->format==VK_FORMAT_UNDEFINED && p->extent.width==64 && p->extent.height==32);
 assert(e->handleTypes==VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID && f->externalFormat==123);
 if(action()!=VK_SUCCESS)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
 *out=HANDLE(VkImage,1);live|=1;return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice d,const VkMemoryAllocateInfo *p,const VkAllocationCallbacks *a,VkDeviceMemory *out)
{
 const VkMemoryDedicatedAllocateInfo *ded=(const VkMemoryDedicatedAllocateInfo*)p->pNext;
 const VkImportAndroidHardwareBufferInfoANDROID *im=(const VkImportAndroidHardwareBufferInfoANDROID*)ded->pNext;
 assert(d==device && !a && p->allocationSize==4096 && p->memoryTypeIndex==1);
 assert(ded->image==HANDLE(VkImage,1) && im->buffer);
 if(action()!=VK_SUCCESS)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
 *out=HANDLE(VkDeviceMemory,2);live|=2;return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL bind(VkDevice d,VkImage im,VkDeviceMemory mem,VkDeviceSize offset)
{assert(d==device && im==HANDLE(VkImage,1) && mem==HANDLE(VkDeviceMemory,2) && !offset);return action();}
static VKAPI_ATTR VkResult VKAPI_CALL conversion(VkDevice d,const VkSamplerYcbcrConversionCreateInfo *p,const VkAllocationCallbacks *a,VkSamplerYcbcrConversion *out)
{
 assert(d==device && !a && p->format==VK_FORMAT_UNDEFINED && p->ycbcrModel==VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020);
 assert(p->ycbcrRange==VK_SAMPLER_YCBCR_RANGE_ITU_NARROW && p->chromaFilter==VK_FILTER_NEAREST);
 if(action()!=VK_SUCCESS)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
 *out=HANDLE(VkSamplerYcbcrConversion,3);live|=4;return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL sampler(VkDevice d,const VkSamplerCreateInfo *p,const VkAllocationCallbacks *a,VkSampler *out)
{
 const VkSamplerYcbcrConversionInfo *c=(const VkSamplerYcbcrConversionInfo*)p->pNext;
 assert(d==device && !a && c->conversion==HANDLE(VkSamplerYcbcrConversion,3) && !p->anisotropyEnable && !p->unnormalizedCoordinates);
 if(action()!=VK_SUCCESS)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
 *out=HANDLE(VkSampler,4);live|=8;return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL view(VkDevice d,const VkImageViewCreateInfo *p,const VkAllocationCallbacks *a,VkImageView *out)
{
 const VkSamplerYcbcrConversionInfo *c=(const VkSamplerYcbcrConversionInfo*)p->pNext;
 assert(d==device && !a && c->conversion==HANDLE(VkSamplerYcbcrConversion,3));
 assert(p->format==VK_FORMAT_UNDEFINED && p->subresourceRange.aspectMask==VK_IMAGE_ASPECT_COLOR_BIT);
 if(action()!=VK_SUCCESS)return VK_ERROR_OUT_OF_DEVICE_MEMORY;
 *out=HANDLE(VkImageView,5);live|=16;return VK_SUCCESS;
}
#define DESTROY(name,type,bit,prereq) \
static VKAPI_ATTR void VKAPI_CALL name(VkDevice d,type h,const VkAllocationCallbacks *a) \
{assert(d==device && h && !a && (live&(bit)) && !(live&(prereq)));live&=~(bit);}
DESTROY(dview,VkImageView,16,0)
DESTROY(dsampler,VkSampler,8,16)
DESTROY(dconversion,VkSamplerYcbcrConversion,4,24)
DESTROY(dimage,VkImage,1,28)
DESTROY(dmemory,VkDeviceMemory,2,29)
int main(void)
{
 static VkPhysicalDeviceMemoryProperties memory;
 vulkan_android_image_ops_t ops;
 vulkan_android_image_t out,empty;
 int i;
 device=HANDLE(VkDevice,99);memset(&ops,0,sizeof(ops));memset(&empty,0,sizeof(empty));
 ops.properties=properties;ops.create_image=image;ops.destroy_image=dimage;ops.allocate=allocate;
 ops.free_memory=dmemory;ops.bind=bind;ops.create_conversion=conversion;ops.destroy_conversion=dconversion;
 ops.create_sampler=sampler;ops.destroy_sampler=dsampler;ops.create_view=view;ops.destroy_view=dview;
 memory.memoryTypeCount=2;
 for(i=1;i<=7;i++) {
  fail_at=i;step=0;
  assert(!vulkan_android_image_create(device,&ops,HANDLE(struct AHardwareBuffer*,1),64,32,&memory,
        VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020,VK_SAMPLER_YCBCR_RANGE_ITU_NARROW,&out));
  assert(!live && !memcmp(&out,&empty,sizeof(out)));
 }
 fail_at=0;step=0;bad=1;
 assert(!vulkan_android_image_create(device,&ops,HANDLE(struct AHardwareBuffer*,1),64,32,&memory,
       VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020,VK_SAMPLER_YCBCR_RANGE_ITU_NARROW,&out));
 bad=0;step=0;
 assert(vulkan_android_image_create(device,&ops,HANDLE(struct AHardwareBuffer*,1),64,32,&memory,
       VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020,VK_SAMPLER_YCBCR_RANGE_ITU_NARROW,&out));
 assert(live==31 && out.external_format==123);
 vulkan_android_image_destroy(device,&ops,&out);assert(!live && !memcmp(&out,&empty,sizeof(out)));
 puts("PASS: Vulkan AHB dedicated import, external YCbCr resources and every partial failure unwind");
 return 0;
}
