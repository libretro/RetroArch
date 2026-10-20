/* The generated function below contains the production HDR slot/wait path. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
typedef unsigned VkFence;
#define VK_NULL_HANDLE 0
#define VK_TRUE 1
#define UINT64_MAX_VALUE (~(uint64_t)0)
#undef UINT64_MAX
#define UINT64_MAX UINT64_MAX_VALUE
struct vk_texture { unsigned marker; };
struct context { unsigned current_frame_index; VkFence swapchain_fences[3]; bool swapchain_fences_signalled[3]; unsigned device; };
typedef struct { struct context *context; struct { struct vk_texture staging[3]; } readback; } vk_t;
static vk_t *active;
static unsigned waited, drains, calls;
static void video_driver_cached_frame(void)
{
 unsigned slot=active->context->current_frame_index;
 active->context->swapchain_fences_signalled[slot]=true;
 active->context->current_frame_index=(slot+1)%3;
}
static int vkWaitForFences(unsigned device,unsigned count,const VkFence *f,bool all,uint64_t timeout)
{ (void)device;(void)all;(void)timeout;assert(count==1);waited=*f;calls++;return 0; }
static void vulkan_wait_own_submissions(vk_t *v) { (void)v;drains++; }
#include "production_wait.h"
int main(void)
{
 struct context c={0,{11,22,33},{false,false,false},0};
 vk_t v;unsigned i;
 v.context=&c;active=&v;
 for(i=0;i<3;i++)
 {
  unsigned j;for(j=0;j<3;j++)c.swapchain_fences_signalled[j]=true;
  c.current_frame_index=i;waited=drains=calls=0;
  assert(probe(&v,false)==i);
  assert(c.current_frame_index==(i+1)%3);
  assert(calls==1 && waited==c.swapchain_fences[i] && drains==0);
  c.current_frame_index=i;waited=drains=calls=0;
  assert(probe(&v,true)==i);
  assert(calls==1 && waited==c.swapchain_fences[i] && drains==0);
  c.swapchain_fences_signalled[i]=false;waited=drains=calls=0;
  assert(probe(&v,true)==i);assert(calls==0 && drains==1);
 }
 puts("[pass] Vulkan HDR readback waits on the copied slot after present advances");return 0;
}
