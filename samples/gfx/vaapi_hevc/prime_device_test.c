/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned allocation_failure;
static void *prime_alloc(size_t size){return allocation_failure?NULL:malloc(size);}
#define VULKAN_PRIME_ALLOC prime_alloc
#include "../../../gfx/common/vulkan_prime_device.h"
static const char *const full[]={"VK_KHR_swapchain","VK_KHR_external_memory_fd",
 "VK_EXT_external_memory_dma_buf","VK_EXT_image_drm_format_modifier",
 "VK_EXT_queue_family_foreign","VK_KHR_image_format_list","VK_KHR_timeline_semaphore"};
static void prepare(const char **list,unsigned *count,int missing)
{
 unsigned i;*count=0;
 for(i=0;i<7;i++)if((int)i!=missing)list[(*count)++]=full[i];
}
static unsigned calls,refuse_prime,refuse_video;
static VkDeviceQueueCreateInfo queue_info;
static VkPhysicalDeviceFeatures2 features;
static VkResult create(void *user,VkPhysicalDevice gpu,const VkDeviceCreateInfo *info,VkDevice *device)
{
 unsigned i,prime=0,video=0;
 assert(user==&calls && gpu==(VkPhysicalDevice)(uintptr_t)1 && *device==VK_NULL_HANDLE);
 assert(info->queueCreateInfoCount==1 && info->pQueueCreateInfos==&queue_info && info->pNext==&features);calls++;
 for(i=0;i<info->enabledExtensionCount;i++) {
  prime|=vulkan_prime_device_extension(info->ppEnabledExtensionNames[i]);
  if(!strcmp(info->ppEnabledExtensionNames[i],"VK_KHR_video_queue"))video=1;
 }
 *device=(VkDevice)(uintptr_t)1;
 if(prime && refuse_prime)return VK_ERROR_EXTENSION_NOT_PRESENT;
 if(video && refuse_video)return VK_ERROR_FEATURE_NOT_PRESENT;
 return VK_SUCCESS;
}
static void creation(void)
{
 const char *list[8],*saved[8];
 VkDeviceCreateInfo info;
 VkDevice device;
 bool prime;
 unsigned count;
 prepare(list,&count,-1);list[count++]="VK_KHR_video_queue";memcpy(saved,list,sizeof(saved));
 memset(&info,0,sizeof(info));info.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
 info.enabledExtensionCount=count;info.ppEnabledExtensionNames=list;
 info.queueCreateInfoCount=1;info.pQueueCreateInfos=&queue_info;info.pNext=&features;
 prime=true;assert(vulkan_prime_device_create(&calls,(VkPhysicalDevice)(uintptr_t)1,&info,&device,&prime,create)==VK_SUCCESS);
 assert(calls==1 && prime && device && !memcmp(saved,list,sizeof(saved)));
 calls=0;refuse_prime=1;prime=true;
 assert(vulkan_prime_device_create(&calls,(VkPhysicalDevice)(uintptr_t)1,&info,&device,&prime,create)==VK_SUCCESS);
 assert(calls==2 && !prime && device && !memcmp(saved,list,sizeof(saved)));
 calls=0;refuse_video=1;prime=true;
 assert(vulkan_prime_device_create(&calls,(VkPhysicalDevice)(uintptr_t)1,&info,&device,&prime,create)==VK_ERROR_FEATURE_NOT_PRESENT);
 assert(calls==2 && !prime && !device && !memcmp(saved,list,sizeof(saved)));
 info.enabledExtensionCount=7;
 assert(vulkan_prime_device_create(&calls,(VkPhysicalDevice)(uintptr_t)1,&info,&device,&prime,create)==VK_SUCCESS);
 assert(calls==3 && !prime && device && !memcmp(saved,list,sizeof(saved)));
 calls=0;allocation_failure=1;prime=true;
 assert(vulkan_prime_device_create(&calls,(VkPhysicalDevice)(uintptr_t)1,&info,&device,&prime,create)==VK_ERROR_OUT_OF_HOST_MEMORY);
 assert(!calls && !device && !prime && !memcmp(saved,list,sizeof(saved)));allocation_failure=0;
 info.enabledExtensionCount=1;prime=true;
 assert(vulkan_prime_device_create(&calls,(VkPhysicalDevice)(uintptr_t)1,&info,&device,&prime,create)==VK_SUCCESS && !prime && calls==1);
}
int main(void)
{
 const char *list[8],*saved[8];
 unsigned count,i,before;
 for(i=0;i<7;i++)assert(!!vulkan_prime_device_extension(full[i])==(i>0 && i<6));
 assert(!vulkan_prime_device_extension(NULL));
 assert(!vulkan_prime_device_extension("VK_EXT_external_memory_dma_buf_extra"));
 prepare(list,&count,-1);assert(vulkan_prime_device_filter(VK_API_VERSION_1_1,VK_API_VERSION_1_1,true,true,list,&count) && count==7);
 prepare(list,&count,5);assert(vulkan_prime_device_filter(VK_API_VERSION_1_2,VK_API_VERSION_1_2,true,true,list,&count) && count==6);
 prepare(list,&count,5);assert(!vulkan_prime_device_filter(VK_API_VERSION_1_2,VK_API_VERSION_1_1,true,true,list,&count) && count==2);
 for(i=1;i<=5;i++) {
  prepare(list,&count,(int)i);
  assert(!vulkan_prime_device_filter(VK_API_VERSION_1_1,VK_API_VERSION_1_1,true,true,list,&count));
  assert(count==2 && !strcmp(list[0],full[0]) && !strcmp(list[1],full[6]));
 }
 prepare(list,&count,-1);before=count;memcpy(saved,list,count*sizeof(*list));
 assert(!vulkan_prime_device_filter(VK_API_VERSION_1_2,VK_API_VERSION_1_2,false,true,list,&count));
 assert(count==before && !memcmp(saved,list,count*sizeof(*list)));
 prepare(list,&count,-1);assert(!vulkan_prime_device_filter(VK_API_VERSION_1_2,VK_API_VERSION_1_2,true,false,list,&count) && count==2);
 prepare(list,&count,-1);assert(!vulkan_prime_device_filter(VK_API_VERSION_1_0,VK_API_VERSION_1_1,true,true,list,&count) && count==2);
 prepare(list,&count,-1);assert(!vulkan_prime_device_filter(VK_API_VERSION_1_1,VK_API_VERSION_1_0,true,true,list,&count) && count==2);
 prepare(list,&count,-1);list[count++]=full[1];
 assert(vulkan_prime_device_filter(VK_API_VERSION_1_2,VK_API_VERSION_1_2,true,true,list,&count) && count==8);
 assert(!vulkan_prime_device_filter(VK_API_VERSION_1_2,VK_API_VERSION_1_2,true,true,NULL,&count));
 assert(!vulkan_prime_device_filter(VK_API_VERSION_1_2,VK_API_VERSION_1_2,true,true,list,NULL));
 creation();
 puts("PASS: PRIME own-device extension bundle, version dependencies, preserved core requests and refusals; no device execution");
 return 0;
}
