/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_PRIME_DEVICE_H
#define RARCH_VULKAN_PRIME_DEVICE_H
#include "vulkan_drm_identity.h"
#ifndef VULKAN_PRIME_ALLOC
#define VULKAN_PRIME_ALLOC malloc
#endif
typedef VkResult (*vulkan_prime_create_t)(void*,VkPhysicalDevice,const VkDeviceCreateInfo*,VkDevice*);
static INLINE unsigned vulkan_prime_device_extension(const char *name)
{
 static const char *const names[]={"VK_KHR_external_memory_fd","VK_EXT_external_memory_dma_buf",
  "VK_EXT_image_drm_format_modifier","VK_EXT_queue_family_foreign","VK_KHR_image_format_list"};
 unsigned i;
 if(name)for(i=0;i<5;i++)if(!strcmp(name,names[i]))return 1u<<i;
 return 0;
}
/* Cold own-device negotiation. Names are the actual enable list. Incomplete
 * PRIME additions are removed together; other extensions retain their order.
 * A core-owned device's explicit requests are never changed. This gate does
 * not prove a particular format/modifier or exported FD can be imported. */
static INLINE bool vulkan_prime_device_filter(uint32_t device_version,uint32_t instance_version,
 bool own_device,bool render_node,const char **enabled,unsigned *count)
{
 unsigned i,kept=0,mask=0;
 bool supported;
 if(!own_device || !enabled || !count)return false;
 for(i=0;i<*count;i++)mask|=vulkan_prime_device_extension(enabled[i]);
 supported=device_version>=VK_API_VERSION_1_1 && instance_version>=VK_API_VERSION_1_1
    && render_node && (mask&15u)==15u
    && ((device_version>=VK_API_VERSION_1_2 && instance_version>=VK_API_VERSION_1_2) || (mask&16u));
 if(!supported)
 {
  for(i=0;i<*count;i++)if(!vulkan_prime_device_extension(enabled[i]))enabled[kept++]=enabled[i];
  *count=kept;
 }
 return supported;
}
/* Cold optional-device retry. Preserve the candidate's queues/features and
 * caller's enable list, including when the first candidate adds video queues. */
static INLINE VkResult vulkan_prime_device_create(void *user,VkPhysicalDevice gpu,
 const VkDeviceCreateInfo *info,VkDevice *device,bool *prime,vulkan_prime_create_t create)
{
 VkDeviceCreateInfo candidate;
 const char **names;
 unsigned count,i;
 bool present=false;
 VkResult result;
 if(!info || !device || !prime || !create || info->enabledExtensionCount>4096
       || (info->enabledExtensionCount && !info->ppEnabledExtensionNames))return VK_ERROR_INITIALIZATION_FAILED;
 *device=VK_NULL_HANDLE;
 for(i=0;i<info->enabledExtensionCount;i++)
  if(vulkan_prime_device_extension(info->ppEnabledExtensionNames[i]))present=true;
 if(!present)
 {
  *prime=false;result=create(user,gpu,info,device);
  if(result!=VK_SUCCESS)*device=VK_NULL_HANDLE;
  return result;
 }
 count=info->enabledExtensionCount;
 names=(const char**)VULKAN_PRIME_ALLOC(count*sizeof(*names));
 if(!names){*prime=false;return VK_ERROR_OUT_OF_HOST_MEMORY;}
 memcpy(names,info->ppEnabledExtensionNames,count*sizeof(*names));candidate=*info;
 candidate.ppEnabledExtensionNames=names;
 if(!*prime)vulkan_prime_device_filter(0,0,true,false,names,&count);
 candidate.enabledExtensionCount=count;
 result=create(user,gpu,&candidate,device);
 if(result!=VK_SUCCESS && *prime)
 {
  *prime=false;*device=VK_NULL_HANDLE;
  vulkan_prime_device_filter(0,0,true,false,names,&count);candidate.enabledExtensionCount=count;
  result=create(user,gpu,&candidate,device);
 }
 if(result!=VK_SUCCESS)*device=VK_NULL_HANDLE;
 free(names);return result;
}
#endif
