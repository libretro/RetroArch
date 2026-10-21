/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_android_cache.h"
#include <stdlib.h>
#include <string.h>
struct vulkan_android_cache {
 vulkan_android_entry_config_t config;
 VkPhysicalDeviceMemoryProperties memory;
 unsigned count,next;
 bool fp16,closed;
 bool occupied[32];
 vulkan_android_cache_entry_t entry[32];
};
vulkan_android_cache_t *vulkan_android_cache_new(const vulkan_android_entry_config_t *c,unsigned count,bool fp16)
{
 vulkan_android_cache_t *cache;
 if(!c || !c->memory || !count || count>32 || !c->device || !c->queue || !c->command_pool)return NULL;
 cache=(vulkan_android_cache_t*)calloc(1,sizeof(*cache));if(!cache)return NULL;
 cache->config=*c;cache->memory=*c->memory;cache->config.memory=&cache->memory;
 cache->count=count;cache->fp16=fp16;return cache;
}
static bool vulkan_android_cache_matches(const vulkan_android_submission_t *e,const android_frame_view_t *v)
{
 const android_image_view_t *a=&e->source.image,*b=&v->image;
 return a->buffer==b->buffer && a->buffer_width==b->buffer_width && a->buffer_height==b->buffer_height
       && a->left==b->left && a->top==b->top && a->right==b->right && a->bottom==b->bottom
       && e->source.pixfmt==v->pixfmt && !memcmp(&e->source.color,&v->color,sizeof(v->color));
}
static bool vulkan_android_cache_claim(const video_hw_frame_t *frame,android_frame_view_t *view)
{
 return frame && android_frame_callback(frame->abandon)
       && android_frame_claim(frame->owner,frame->done,frame->done_value,frame->image,view);
}
int vulkan_android_cache_prepare(vulkan_android_cache_t *cache,const video_hw_frame_t *frame)
{
 android_frame_view_t view;
 unsigned i,index;
 int result=0;
 if(!cache || cache->closed || !vulkan_android_cache_claim(frame,&view))return -1;
 for(i=0;i<cache->count;i++)if(cache->occupied[i] && vulkan_android_cache_matches(&cache->entry[i].submission,&view)) {
  result=vulkan_android_submission_poll(&cache->entry[i].submission);
  if(result<0 && vulkan_android_cache_entry_destroy(&cache->entry[i])) {
   cache->occupied[i]=false;
   if(vulkan_android_cache_entry_create(&cache->config,&view,cache->fp16,&cache->entry[i])) {
    cache->occupied[i]=true;result=1;
   }
  }
  goto done;
 }
 /* Round-robin eviction only on this cold path, and only after completion. */
 for(i=0;i<cache->count;i++) {
  index=(cache->next+i)%cache->count;
  if(cache->occupied[index] && !vulkan_android_cache_entry_destroy(&cache->entry[index]))continue;
  cache->occupied[index]=false;
  if(!vulkan_android_cache_entry_create(&cache->config,&view,cache->fp16,&cache->entry[index])) {
   result=-1;goto done;
  }
  cache->occupied[index]=true;cache->next=(index+1)%cache->count;result=1;break;
 }
done:
 if(!android_frame_unclaim(frame->owner,frame->done,frame->done_value))return -2;
 return result;
}
int vulkan_android_cache_present(vulkan_android_cache_t *cache,const video_hw_frame_t *frame,
      VkImage destination,VkImageView image_view,VkImageLayout layout,bool *prepare_needed)
{
 android_frame_view_t view;
 unsigned i;
 if(prepare_needed)*prepare_needed=false;
 if(!cache || cache->closed || !vulkan_android_cache_claim(frame,&view))return -1;
 for(i=0;i<cache->count;i++)if(cache->occupied[i] && vulkan_android_cache_matches(&cache->entry[i].submission,&view))break;
 if(!android_frame_unclaim(frame->owner,frame->done,frame->done_value))return -2;
 if(i==cache->count){if(prepare_needed)*prepare_needed=true;return 0;}
 return vulkan_android_submission_present(&cache->entry[i].submission,frame,destination,image_view,layout);
}
void vulkan_android_cache_close(vulkan_android_cache_t *cache){if(cache)cache->closed=true;}
bool vulkan_android_cache_free(vulkan_android_cache_t *cache)
{
 unsigned i;bool pending=false;
 if(!cache)return true;
 if(!cache->closed)return false;
 for(i=0;i<cache->count;i++)if(cache->occupied[i]) {
  if(vulkan_android_cache_entry_destroy(&cache->entry[i]))cache->occupied[i]=false;
  else pending=true;
 }
 if(pending)return false;
 free(cache);return true;
}
bool vulkan_android_cache_after_device_destroy(vulkan_android_cache_t *cache)
{
 unsigned i;bool complete=true;
 if(!cache)return true;
 cache->closed=true;
 for(i=0;i<cache->count;i++)if(cache->occupied[i]
       && !vulkan_android_submission_after_device_destroy(&cache->entry[i].submission))complete=false;
 return complete;
}
