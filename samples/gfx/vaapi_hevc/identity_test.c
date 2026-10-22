/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static unsigned allocation_failure;
static void *identity_alloc(size_t size){return allocation_failure?NULL:malloc(size);}
#define VULKAN_DRM_ALLOC identity_alloc
#include "../../../gfx/common/vulkan_drm_identity.h"
static unsigned fault,queries,enumerations;
static VKAPI_ATTR VkResult VKAPI_CALL identity_enumerate(VkPhysicalDevice gpu,
 const char *layer,uint32_t *count,VkExtensionProperties *extensions)
{
 assert(gpu==(VkPhysicalDevice)(uintptr_t)1 && !layer);enumerations++;
 if(!extensions) {
  *count=fault==1?0:fault==2?4097:2;
  return fault==3?VK_ERROR_DEVICE_LOST:VK_SUCCESS;
 }
 assert(*count==2);memset(extensions,0,2*sizeof(*extensions));
 strcpy(extensions[0].extensionName,"VK_KHR_swapchain");
 strcpy(extensions[1].extensionName,fault==4?"VK_EXT_wrong":VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME);
 if(fault==5)*count=3;
 if(fault==6)return VK_INCOMPLETE;
 if(fault==7)return VK_ERROR_DEVICE_LOST;
 return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL identity_properties(VkPhysicalDevice gpu,VkPhysicalDeviceProperties2 *properties)
{
 VkPhysicalDeviceDrmPropertiesEXT *drm=(VkPhysicalDeviceDrmPropertiesEXT*)properties->pNext;
 assert(gpu==(VkPhysicalDevice)(uintptr_t)1 && properties->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
 assert(drm && drm->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT && !drm->pNext);queries++;
 drm->hasPrimary=VK_TRUE;drm->primaryMajor=226;drm->primaryMinor=0;
 drm->hasRender=fault==8?VK_FALSE:fault==9?2:VK_TRUE;drm->renderMajor=226;drm->renderMinor=128;
 if(fault==10)drm->renderMajor=-1;
 if(fault==11)drm->renderMinor=-1;
 if(fault==12)drm->renderMajor=(int64_t)UINT_MAX+1;
 if(fault==13)drm->renderMinor=(int64_t)UINT_MAX+1;
 if(fault==14){drm->renderMajor=UINT_MAX;drm->renderMinor=UINT_MAX;}
}
int main(void)
{
 vulkan_drm_identity_t result;
 const char *command;
 for(fault=0;fault<=14;fault++) {
  queries=enumerations=0;memset(&result,0xa5,sizeof(result));
  if(!fault || fault==14) {
   assert(vulkan_drm_identity_query(&result,(VkPhysicalDevice)(uintptr_t)1,identity_enumerate,identity_properties));
   assert(result.valid && result.major_id==(fault?UINT_MAX:226) && result.minor_id==(fault?UINT_MAX:128));
  } else {
   assert(!vulkan_drm_identity_query(&result,(VkPhysicalDevice)(uintptr_t)1,identity_enumerate,identity_properties));
   assert(!result.valid && !result.major_id && !result.minor_id);
  }
  assert(queries==((!fault || fault>=8)?1u:0u));
 }
 fault=0;allocation_failure=1;queries=0;
 assert(!vulkan_drm_identity_query(&result,(VkPhysicalDevice)(uintptr_t)1,identity_enumerate,identity_properties) && !queries);
 assert(!vulkan_drm_identity_query(&result,VK_NULL_HANDLE,identity_enumerate,identity_properties));
 assert(!vulkan_drm_identity_query(&result,(VkPhysicalDevice)(uintptr_t)1,NULL,identity_properties));
 assert(!vulkan_drm_identity_query(&result,(VkPhysicalDevice)(uintptr_t)1,identity_enumerate,NULL));
 assert(!vulkan_drm_identity_query(NULL,(VkPhysicalDevice)(uintptr_t)1,identity_enumerate,identity_properties));
 assert(!vulkan_drm_properties_command(VK_API_VERSION_1_0,false));
 command=vulkan_drm_properties_command(VK_API_VERSION_1_0,true);assert(!strcmp(command,"vkGetPhysicalDeviceProperties2KHR"));
 command=vulkan_drm_properties_command(VK_API_VERSION_1_1,false);assert(!strcmp(command,"vkGetPhysicalDeviceProperties2"));
 command=vulkan_drm_properties_command(VK_API_VERSION_1_2,true);assert(!strcmp(command,"vkGetPhysicalDeviceProperties2"));
 puts("PASS: Vulkan DRM render-node query, extension/instance gates, bounds and refusal; no device execution");
 return 0;
}
