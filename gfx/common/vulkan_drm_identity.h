/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_DRM_IDENTITY_H
#define RARCH_VULKAN_DRM_IDENTITY_H
#include "../include/vulkan/vulkan_core.h"
#include <boolean.h>
#include <retro_inline.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
typedef struct vulkan_drm_identity {
 uint32_t major_id,minor_id;
 bool valid;
} vulkan_drm_identity_t;
#ifndef VULKAN_DRM_ALLOC
#define VULKAN_DRM_ALLOC malloc
#endif
static INLINE const char *vulkan_drm_properties_command(uint32_t version,bool properties2)
{
 return version>=VK_API_VERSION_1_1?"vkGetPhysicalDeviceProperties2":
    properties2?"vkGetPhysicalDeviceProperties2KHR":NULL;
}
/* Cold query. The caller supplies a properties2 command legal for its actual
 * instance API/extensions. No node/path inference from GPU names or PCI IDs. */
static INLINE bool vulkan_drm_identity_query(vulkan_drm_identity_t *out,
 VkPhysicalDevice gpu,PFN_vkEnumerateDeviceExtensionProperties enumerate,
 PFN_vkGetPhysicalDeviceProperties2 properties)
{
#ifdef VK_EXT_physical_device_drm
 VkExtensionProperties *extensions;
 VkPhysicalDeviceProperties2 info;
 VkPhysicalDeviceDrmPropertiesEXT drm;
 uint32_t count=0,capacity,i;
 bool found=false;
 if(!out)return false;
 memset(out,0,sizeof(*out));
 if(!gpu || !enumerate || !properties || enumerate(gpu,NULL,&count,NULL)!=VK_SUCCESS
       || !count || count>4096)return false;
 capacity=count;
 extensions=(VkExtensionProperties*)VULKAN_DRM_ALLOC(count*sizeof(*extensions));
 if(!extensions)return false;
 if(enumerate(gpu,NULL,&count,extensions)==VK_SUCCESS && count<=capacity)
  for(i=0;i<count;i++)
   if(!strncmp(extensions[i].extensionName,VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME,
            sizeof(extensions[i].extensionName))){found=true;break;}
 free(extensions);if(!found)return false;
 memset(&info,0,sizeof(info));memset(&drm,0,sizeof(drm));
 info.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;info.pNext=&drm;
 drm.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
 properties(gpu,&info);
 if(drm.hasRender!=VK_TRUE || drm.renderMajor<0 || drm.renderMinor<0
       || (uint64_t)drm.renderMajor>UINT_MAX || (uint64_t)drm.renderMinor>UINT_MAX)return false;
 out->major_id=(uint32_t)drm.renderMajor;out->minor_id=(uint32_t)drm.renderMinor;out->valid=true;
 return true;
#else
 (void)gpu;(void)enumerate;(void)properties;
 if(out)memset(out,0,sizeof(*out));return false;
#endif
}
#endif
