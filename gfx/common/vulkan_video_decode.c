/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* rh264/rh265/rvp9/rav1 hardware backends on Vulkan Video.
 *
 * Each decoder owns a video session, a DPB, bitstream/command rings and
 * a decode/transfer queue lane. Submissions share an ordered timeline.
 * With unified video layouts, readers sample the DPB in GENERAL and
 * physical slots are renamed while exported frames remain held.
 * Otherwise the visible window is copied to an independent output image.
 * Ready/done timelines cover either export. Closed decoders stay in an
 * atomic graveyard until their work and every reader have finished. */

#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <formats/rh264.h>
#include <formats/rh265_hw.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
#include <formats/video_hw_frame.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "vulkan_video_decode.h"
#include "vulkan_hevc_params.h"
#include "vulkan_hevc_picture.h"
#include "vulkan_vp9_picture.h"
#ifdef HAVE_RAV1
#include "vulkan_av1_params.h"
#endif
#include "../include/vulkan/vulkan.h"
#include "vksym.h"
#include "vulkan_video_planes.h"
#include "../video_defines.h"
#include "../../verbosity.h"

#if defined(_WIN32) && !defined(__WINRT__) \
   && (defined(HAVE_D3D11) || defined(HAVE_D3D12))
#include <dxgi1_2.h>
#include "d3d11_video_decode.h"
#endif

#define VVD_MAX_LANES  2
#define VVD_MAX_SLOTS  32
#define VVD_OUTS       4    /* output images per decoder          */
#define VVD_PHYSICAL (VVD_MAX_SLOTS + VVD_OUTS)
#define VVD_RING       4    /* command and bitstream buffers      */
#define VVD_WAIT_NS    1000000000ull

typedef struct vvd_dec
{
   struct vvd_dec *grave_next;
   unsigned lane;
   VkQueue dq, xq;

   /* the profile, chained as every create that names it wants */
   VkVideoDecodeH264ProfileInfoKHR h264_profile;
   VkVideoDecodeH265ProfileInfoKHR h265_profile;
   VkVideoDecodeVP9ProfileInfoKHR vp9_profile;
#ifdef HAVE_RAV1
   VkVideoDecodeAV1ProfileInfoKHR av1_profile;
   vulkan_av1_picture_t *av1;
   rav1_sequence_t av1_sequence;
   vulkan_av1_sequence_t av1_params;
   StdVideoDecodeAV1ReferenceInfo av1_saved[VVD_MAX_SLOTS], av1_ref[8];
   VkVideoDecodeAV1DpbSlotInfoKHR av1_slot[8];
#endif
   const rh265_video *hevc_source;
   vulkan_hevc_params_t *hevc;
   StdVideoH265LevelIdc hevc_max_level;
   VkVideoProfileInfoKHR profile;
   VkVideoProfileListInfoKHR profiles;

   VkVideoSessionKHR session;
   VkDeviceMemory session_mem[16];
   uint32_t num_session_mem;
   VkVideoSessionParametersKHR params;
   unsigned params_gen;
   bool have_params;
   bool reset;                    /* the session has been reset       */

   VkExtent2D coded;              /* in samples, whole macroblocks    */
   VkExtent2D image;              /* coded, to the access granularity */
   VkFormat format;
   unsigned pixfmt;
   unsigned visible_w, visible_h;
   unsigned crop_x, crop_y;
   unsigned slots;
   bool dpb_array;                /* the slots are layers of dpb[0]   */
   unsigned yuv[VVD_MAX_SLOTS];
   bool direct_dpb;
   unsigned physical_slots, slot_map[VVD_MAX_SLOTS], out_slot[VVD_OUTS];
   VkImageView dpb_planes[VVD_PHYSICAL][2];
   VkImage dpb[VVD_PHYSICAL];
   VkDeviceMemory dpb_mem[VVD_PHYSICAL];
   VkImageView view[VVD_PHYSICAL];

   VkImage out[VVD_OUTS];
   VkImageView out_planes[VVD_OUTS][2];
   VkDeviceMemory out_mem[VVD_OUTS];
   VkSemaphore out_done[VVD_OUTS];
   uint64_t out_value[VVD_OUTS];  /* the read last handed out       */
   unsigned out_next;
   VkBuffer readback;
   VkDeviceMemory readback_mem;
   uint8_t *readback_map;
   bool readback_coherent;

   VkSemaphore tl;                /* the order of every submission   */
   uint64_t tl_value;

   VkCommandPool dpool, xpool;
   VkCommandBuffer dcb[VVD_RING], xcb[VVD_RING];
   uint64_t dcb_value[VVD_RING], xcb_value[VVD_RING];
   unsigned dring, xring;

   VkBuffer bs[VVD_RING];
   VkDeviceMemory bs_mem[VVD_RING];
   void *bs_map[VVD_RING];
   VkDeviceSize bs_cap[VVD_RING];
   bool bs_coherent[VVD_RING];
   VkDeviceSize bs_offset_align, bs_size_align;

   /* a decode's reference list - its picture's slot and up to 16
    * references - kept here rather than on the stack */
   VkVideoPictureResourceInfoKHR res[17];
   StdVideoDecodeH264ReferenceInfo std_ref[17];
   VkVideoDecodeH264DpbSlotInfoKHR h264_slot[17];
   StdVideoDecodeH265ReferenceInfo hevc_ref[17];
   VkVideoDecodeH265DpbSlotInfoKHR hevc_slot[17];
   uint32_t hevc_offsets[64];
   VkVideoReferenceSlotInfoKHR slots_info[17], begin_slots[17];
} vvd_dec_t;

static struct
{
   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkPhysicalDeviceMemoryProperties mem;
   uint32_t gfx_family, dec_family, xfer_family, xfer_first;
   unsigned lanes;
   bool up;
   bool general;
   retro_atomic_int_t lane_busy[VVD_MAX_LANES];
   retro_atomic_int_t live;
   retro_atomic_ptr_t grave;
   rh264_hw_ops_t ops;
   rh265_hw_backend_t hevc;
#ifdef HAVE_RVP9
   rvp9_hw_backend_t vp9;
#endif
#ifdef HAVE_RAV1
   rav1_hw_backend_t av1;
#ifdef _WIN32
   bool av1_intrabc_broken;
#endif
#endif
#ifdef _WIN32
   bool h264_broken;
#endif
#if defined(_WIN32) && !defined(__WINRT__) \
   && (defined(HAVE_D3D11) || defined(HAVE_D3D12))
   bool win32_fallback;
#endif
   VkVideoCodecOperationFlagsKHR codecs;

   PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR       get_caps;
   PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR   get_formats;
   PFN_vkCreateVideoSessionKHR                       create_session;
   PFN_vkDestroyVideoSessionKHR                      destroy_session;
   PFN_vkGetVideoSessionMemoryRequirementsKHR        session_mem_reqs;
   PFN_vkBindVideoSessionMemoryKHR                   bind_session_mem;
   PFN_vkCreateVideoSessionParametersKHR             create_params;
   PFN_vkDestroyVideoSessionParametersKHR            destroy_params;
   PFN_vkCmdBeginVideoCodingKHR                      cmd_begin;
   PFN_vkCmdEndVideoCodingKHR                        cmd_end;
   PFN_vkCmdControlVideoCodingKHR                    cmd_control;
   PFN_vkCmdDecodeVideoKHR                           cmd_decode;
   PFN_vkCmdPipelineBarrier2KHR                      cmd_barrier2;
   PFN_vkQueueSubmit2KHR                             submit2;
   PFN_vkWaitSemaphoresKHR                           wait_sems;
   PFN_vkSignalSemaphoreKHR                          signal_sem;
   PFN_vkGetSemaphoreCounterValueKHR                 counter;
} vvd;

/* ---- helpers ---- */

static uint32_t vvd_memory_type(uint32_t bits, VkMemoryPropertyFlags want,
      VkMemoryPropertyFlags avoid)
{
   uint32_t i;
   for (i = 0; i < vvd.mem.memoryTypeCount; i++)
   {
      VkMemoryPropertyFlags f = vvd.mem.memoryTypes[i].propertyFlags;
      if ((bits & (1u << i)) && (f & want) == want && !(f & avoid))
         return i;
   }
   return UINT32_MAX;
}

static bool vvd_alloc(const VkMemoryRequirements *req,
      VkMemoryPropertyFlags want, VkDeviceMemory *mem)
{
   VkMemoryAllocateInfo info;
   uint32_t type = vvd_memory_type(req->memoryTypeBits, want,
         VK_MEMORY_PROPERTY_PROTECTED_BIT);
   if (type == UINT32_MAX)
      type = vvd_memory_type(req->memoryTypeBits, 0,
            VK_MEMORY_PROPERTY_PROTECTED_BIT);
   if (type == UINT32_MAX)
      return false;
   info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   info.pNext           = NULL;
   info.allocationSize  = req->size;
   info.memoryTypeIndex = type;
   return vkAllocateMemory(vvd.device, &info, NULL, mem) == VK_SUCCESS;
}

static VkSemaphore vvd_timeline(void)
{
   VkSemaphoreTypeCreateInfoKHR type;
   VkSemaphoreCreateInfo info;
   VkSemaphore sem = VK_NULL_HANDLE;
   type.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR;
   type.pNext         = NULL;
   type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
   type.initialValue  = 0;
   info.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   info.pNext         = &type;
   info.flags         = 0;
   if (vkCreateSemaphore(vvd.device, &info, NULL, &sem) != VK_SUCCESS)
      return VK_NULL_HANDLE;
   return sem;
}

/* Waits for @sem to reach @value; false on a timeout or a lost device. */
static bool vvd_wait(VkSemaphore sem, uint64_t value, uint64_t timeout_ns)
{
   VkSemaphoreWaitInfoKHR info;
   if (!value)
      return true;
   info.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR;
   info.pNext          = NULL;
   info.flags          = 0;
   info.semaphoreCount = 1;
   info.pSemaphores    = &sem;
   info.pValues        = &value;
   return vvd.wait_sems(vvd.device, &info, timeout_ns) == VK_SUCCESS;
}

static int vvd_try_wait(VkSemaphore sem, uint64_t value)
{
   uint64_t now;
   VkResult r;
   if (!value)
      return 0;
   r = vvd.counter(vvd.device, sem, &now);
   return r == VK_SUCCESS ? (now < value ? 1 : 0) : -1;
}

static bool vvd_reached(VkSemaphore sem, uint64_t value)
{
   uint64_t now = 0;
   if (!value)
      return true;
   return vvd.counter(vvd.device, sem, &now) == VK_SUCCESS && now >= value;
}

static void vvd_barrier(VkCommandBuffer cb, VkImage image, uint32_t layer,
      VkImageLayout from, VkImageLayout to,
      VkPipelineStageFlags2KHR src_stage, VkAccessFlags2KHR src_access,
      VkPipelineStageFlags2KHR dst_stage, VkAccessFlags2KHR dst_access)
{
   VkImageMemoryBarrier2KHR b;
   VkDependencyInfoKHR dep;
   b.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
   b.pNext                           = NULL;
   b.srcStageMask                    = src_stage;
   b.srcAccessMask                   = src_access;
   b.dstStageMask                    = dst_stage;
   b.dstAccessMask                   = dst_access;
   b.oldLayout                       = from;
   b.newLayout                       = to;
   b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   b.image                           = image;
   b.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   b.subresourceRange.baseMipLevel   = 0;
   b.subresourceRange.levelCount     = 1;
   b.subresourceRange.baseArrayLayer = layer;
   b.subresourceRange.layerCount     = 1;
   dep.sType                         = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
   dep.pNext                         = NULL;
   dep.dependencyFlags               = 0;
   dep.memoryBarrierCount            = 0;
   dep.pMemoryBarriers               = NULL;
   dep.bufferMemoryBarrierCount      = 0;
   dep.pBufferMemoryBarriers         = NULL;
   dep.imageMemoryBarrierCount       = 1;
   dep.pImageMemoryBarriers          = &b;
   vvd.cmd_barrier2(cb, &dep);
}

/* @cb to @queue after every submission before it, raising the
 * timeline past it. */
static bool vvd_submit(vvd_dec_t *d, VkQueue queue, VkCommandBuffer cb)
{
   VkSemaphoreSubmitInfoKHR wait, signal;
   VkCommandBufferSubmitInfoKHR cmd;
   VkSubmitInfo2KHR info;

   wait.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
   wait.pNext         = NULL;
   wait.semaphore     = d->tl;
   wait.value         = d->tl_value;
   wait.stageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
   wait.deviceIndex   = 0;
   signal             = wait;
   signal.value       = d->tl_value + 1;
   cmd.sType          = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
   cmd.pNext          = NULL;
   cmd.commandBuffer  = cb;
   cmd.deviceMask     = 0;
   info.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
   info.pNext                    = NULL;
   info.flags                    = 0;
   info.waitSemaphoreInfoCount   = d->tl_value ? 1 : 0;
   info.pWaitSemaphoreInfos      = &wait;
   info.commandBufferInfoCount   = 1;
   info.pCommandBufferInfos      = &cmd;
   info.signalSemaphoreInfoCount = 1;
   info.pSignalSemaphoreInfos    = &signal;
   if (vvd.submit2(queue, 1, &info, VK_NULL_HANDLE) != VK_SUCCESS)
      return false;
   d->tl_value++;
   return true;
}

static bool vvd_begin(VkCommandBuffer cb)
{
   VkCommandBufferBeginInfo info;
   info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   info.pNext            = NULL;
   info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   info.pInheritanceInfo = NULL;
   return     vkResetCommandBuffer(cb, 0) == VK_SUCCESS
           && vkBeginCommandBuffer(cb, &info) == VK_SUCCESS;
}

static StdVideoH264LevelIdc vvd_level(int level_idc)
{
   switch (level_idc)
   {
      case 10: return STD_VIDEO_H264_LEVEL_IDC_1_0;
      case 9:                         /* 1b, which 1.1 contains */
      case 11: return STD_VIDEO_H264_LEVEL_IDC_1_1;
      case 12: return STD_VIDEO_H264_LEVEL_IDC_1_2;
      case 13: return STD_VIDEO_H264_LEVEL_IDC_1_3;
      case 20: return STD_VIDEO_H264_LEVEL_IDC_2_0;
      case 21: return STD_VIDEO_H264_LEVEL_IDC_2_1;
      case 22: return STD_VIDEO_H264_LEVEL_IDC_2_2;
      case 30: return STD_VIDEO_H264_LEVEL_IDC_3_0;
      case 31: return STD_VIDEO_H264_LEVEL_IDC_3_1;
      case 32: return STD_VIDEO_H264_LEVEL_IDC_3_2;
      case 40: return STD_VIDEO_H264_LEVEL_IDC_4_0;
      case 41: return STD_VIDEO_H264_LEVEL_IDC_4_1;
      case 42: return STD_VIDEO_H264_LEVEL_IDC_4_2;
      case 50: return STD_VIDEO_H264_LEVEL_IDC_5_0;
      case 51: return STD_VIDEO_H264_LEVEL_IDC_5_1;
      case 52: return STD_VIDEO_H264_LEVEL_IDC_5_2;
      case 60: return STD_VIDEO_H264_LEVEL_IDC_6_0;
      case 61: return STD_VIDEO_H264_LEVEL_IDC_6_1;
      default: break;
   }
   return STD_VIDEO_H264_LEVEL_IDC_6_2;
}

static void vvd_scaling(StdVideoH264ScalingLists *o,
      const rh264_hw_scaling_t *s)
{
   o->scaling_list_present_mask       = s->present_mask;
   o->use_default_scaling_matrix_mask = s->default_mask;
   memcpy(o->ScalingList4x4, s->list4x4, sizeof(o->ScalingList4x4));
   memcpy(o->ScalingList8x8, s->list8x8, sizeof(o->ScalingList8x8));
}

/* ---- teardown ---- */

static void vvd_destroy(vvd_dec_t *d)
{
   VkDevice dev = vvd.device;
   unsigned i;
   if (d->readback_map)
      vkUnmapMemory(dev, d->readback_mem);
   if (d->readback)
      vkDestroyBuffer(dev, d->readback, NULL);
   if (d->readback_mem)
      vkFreeMemory(dev, d->readback_mem, NULL);
   for (i = 0; i < VVD_RING; i++)
   {
      if (d->bs_map[i])
         vkUnmapMemory(dev, d->bs_mem[i]);
      if (d->bs[i])
         vkDestroyBuffer(dev, d->bs[i], NULL);
      if (d->bs_mem[i])
         vkFreeMemory(dev, d->bs_mem[i], NULL);
   }
   if (d->dpool)
      vkDestroyCommandPool(dev, d->dpool, NULL);
   if (d->xpool)
      vkDestroyCommandPool(dev, d->xpool, NULL);
   for (i = 0; i < VVD_OUTS; i++)
   {
      if (d->out_planes[i][0]) vkDestroyImageView(dev, d->out_planes[i][0], NULL);
      if (d->out_planes[i][1]) vkDestroyImageView(dev, d->out_planes[i][1], NULL);
      if (d->out[i])
         vkDestroyImage(dev, d->out[i], NULL);
      if (d->out_mem[i])
         vkFreeMemory(dev, d->out_mem[i], NULL);
      if (d->out_done[i])
         vkDestroySemaphore(dev, d->out_done[i], NULL);
   }
   for (i = 0; i < VVD_PHYSICAL; i++)
   {
      if (d->dpb_planes[i][0]) vkDestroyImageView(dev, d->dpb_planes[i][0], NULL);
      if (d->dpb_planes[i][1]) vkDestroyImageView(dev, d->dpb_planes[i][1], NULL);
      if (d->view[i])
         vkDestroyImageView(dev, d->view[i], NULL);
      if (d->dpb[i])
         vkDestroyImage(dev, d->dpb[i], NULL);
      if (d->dpb_mem[i])
         vkFreeMemory(dev, d->dpb_mem[i], NULL);
   }
   if (d->params)
      vvd.destroy_params(dev, d->params, NULL);
   if (d->session)
      vvd.destroy_session(dev, d->session, NULL);
   for (i = 0; i < d->num_session_mem; i++)
      vkFreeMemory(dev, d->session_mem[i], NULL);
   if (d->tl)
      vkDestroySemaphore(dev, d->tl, NULL);
#ifdef HAVE_RAV1
   free(d->av1);
#endif
   free(d->hevc);
   free(d);
}

/* Whether nothing of @d's is in use any more: its own work done, and
 * every output image read or given back. */
static bool vvd_settled(const vvd_dec_t *d)
{
   unsigned i;
   if (d->tl && !vvd_reached(d->tl, d->tl_value))
      return false;
   for (i = 0; i < VVD_OUTS; i++)
      if (d->out_done[i] && !vvd_reached(d->out_done[i], d->out_value[i]))
         return false;
   return true;
}

static void vvd_grave_push(vvd_dec_t *d)
{
   void *head;
   do
   {
      head          = retro_atomic_load_acquire_ptr(&vvd.grave);
      d->grave_next = (vvd_dec_t*)head;
   } while (!retro_atomic_cas_ptr(&vvd.grave, head, d));
}

/* Frees the closed decoders that have settled, or all of them with
 * @all. Whoever takes the list owns what is on it. */
static void vvd_grave_reap(bool all)
{
   vvd_dec_t *d = (vvd_dec_t*)retro_atomic_exchange_ptr(&vvd.grave, NULL);
   while (d)
   {
      vvd_dec_t *next = d->grave_next;
      if (all)
      {
         /* its own queues may still run its last work */
         vvd_wait(d->tl, d->tl_value, VVD_WAIT_NS);
         vvd_destroy(d);
      }
      else if (vvd_settled(d))
         vvd_destroy(d);
      else
         vvd_grave_push(d);
      d = next;
   }
}

/* ---- the frame handed out ---- */

/* The reader will never come: its signal, given here. */
static void vvd_abandon(void *owner, uint64_t done, uint64_t value)
{
   VkSemaphoreSignalInfoKHR info;
   VkSemaphore sem;
   (void)owner;
   if (!vvd.up || !done)
      return;
   memcpy(&sem, &done, sizeof(sem));
   info.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR;
   info.pNext     = NULL;
   info.semaphore = sem;
   info.value     = value;
   vvd.signal_sem(vvd.device, &info);
}

/* ---- open ---- */

/* The profile @d decodes @sps with: its own, or a later one that
 * decodes it (Main takes constrained Baseline, High takes Main) - the
 * first the implementation offers with what the stream needs. */
static bool vvd_pick_profile(vvd_dec_t *d, const rh264_hw_sps_t *sps,
      unsigned slots, VkVideoCapabilitiesKHR *caps,
      VkVideoDecodeCapabilitiesKHR *dcaps)
{
   static const StdVideoH264ProfileIdc order[3] = {
      STD_VIDEO_H264_PROFILE_IDC_BASELINE,
      STD_VIDEO_H264_PROFILE_IDC_MAIN,
      STD_VIDEO_H264_PROFILE_IDC_HIGH
   };
   unsigned i = sps->profile_idc == 100 ? 2 : sps->profile_idc == 77 ? 1 : 0;
   StdVideoH264LevelIdc level = vvd_level(sps->level_idc);

   for (; i < 3; i++)
   {
      VkVideoDecodeH264CapabilitiesKHR hcaps;

      d->h264_profile.sType         =
         VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR;
      d->h264_profile.pNext         = NULL;
      d->h264_profile.stdProfileIdc = order[i];
      d->h264_profile.pictureLayout =
         VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR;
      d->profile.sType               = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
      d->profile.pNext               = &d->h264_profile;
      d->profile.videoCodecOperation =
         VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
      d->profile.chromaSubsampling   = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
      d->profile.lumaBitDepth        = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
      d->profile.chromaBitDepth      = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;

      hcaps.sType                      = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;
      hcaps.pNext                      = NULL;
      hcaps.maxLevelIdc                = STD_VIDEO_H264_LEVEL_IDC_1_0;
      dcaps->sType                     = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
      dcaps->pNext                     = &hcaps;
      dcaps->flags                     = 0;
      caps->sType                      = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
      caps->pNext                      = dcaps;
      caps->flags                      = 0;
      caps->minCodedExtent.width       = 0;
      caps->minCodedExtent.height      = 0;
      caps->maxCodedExtent.width       = 0;
      caps->maxCodedExtent.height      = 0;
      caps->maxDpbSlots                = 0;
      caps->maxActiveReferencePictures = 0;
      if (vvd.get_caps(vvd.gpu, &d->profile, caps) != VK_SUCCESS)
         continue;
      if (     d->coded.width  < caps->minCodedExtent.width
            || d->coded.height < caps->minCodedExtent.height
            || d->coded.width  > caps->maxCodedExtent.width
            || d->coded.height > caps->maxCodedExtent.height
            || caps->maxDpbSlots < slots
            || caps->maxActiveReferencePictures
               < (uint32_t)(sps->max_num_ref_frames ? sps->max_num_ref_frames : 1)
            || (int)hcaps.maxLevelIdc < (int)level
            || !(dcaps->flags
               & VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR))
         continue;
      d->profiles.sType        = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
      d->profiles.pNext        = NULL;
      d->profiles.profileCount = 1;
      d->profiles.pProfiles    = &d->profile;
      /* the capability chain points into this frame: cut it */
      dcaps->pNext             = NULL;
      return true;
   }
   return false;
}

/* The decoder format, optimal tiling, for every slot use. */
static bool vvd_format_ok(vvd_dec_t *d, VkImageUsageFlags usage)
{
   VkPhysicalDeviceVideoFormatInfoKHR info;
   VkVideoFormatPropertiesKHR props[16];
   uint32_t n = 16, i;

   info.sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR;
   info.pNext      = &d->profiles;
   info.imageUsage = usage;
   for (i = 0; i < 16; i++)
   {
      props[i].sType           = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
      props[i].pNext           = NULL;
      props[i].format          = VK_FORMAT_UNDEFINED;
      props[i].imageType       = VK_IMAGE_TYPE_1D;
      props[i].imageTiling     = VK_IMAGE_TILING_OPTIMAL;
      props[i].imageUsageFlags = 0;
   }
   {
      VkResult r = vvd.get_formats(vvd.gpu, &info, &n, props);
      if (r != VK_SUCCESS && r != VK_INCOMPLETE)
         return false;
   }
   for (i = 0; i < n && i < 16; i++)
      if (     props[i].format     == d->format
            && props[i].imageType  == VK_IMAGE_TYPE_2D
            && props[i].imageTiling == VK_IMAGE_TILING_OPTIMAL
            && (props[i].imageUsageFlags & usage) == usage)
         return true;
   return false;
}

static bool vvd_make_image(VkImage *image, VkDeviceMemory *mem,
      VkFormat format, VkExtent2D extent, uint32_t layers, VkImageUsageFlags usage,
      const void *pnext, uint32_t family_a, uint32_t family_b)
{
   VkImageCreateInfo info;
   VkMemoryRequirements req;
   uint32_t families[3], count = 1;

   families[0]                = family_a;
   if (family_b != family_a) families[count++] = family_b;
   if ((usage & (VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT))
         == (VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT)
         && vvd.gfx_family != family_a && vvd.gfx_family != family_b)
      families[count++] = vvd.gfx_family;
   info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.pNext                 = pnext;
   info.flags                 = (usage & VK_IMAGE_USAGE_SAMPLED_BIT)
      ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0;
   info.imageType             = VK_IMAGE_TYPE_2D;
   info.format                = format;
   info.extent.width          = extent.width;
   info.extent.height         = extent.height;
   info.extent.depth          = 1;
   info.mipLevels             = 1;
   info.arrayLayers           = layers;
   info.samples               = VK_SAMPLE_COUNT_1_BIT;
   info.tiling                = VK_IMAGE_TILING_OPTIMAL;
   info.usage                 = usage;
   info.sharingMode           = count == 1
      ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
   info.queueFamilyIndexCount = count == 1 ? 0 : count;
   info.pQueueFamilyIndices   = families;
   info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
   if (vkCreateImage(vvd.device, &info, NULL, image) != VK_SUCCESS)
   {
      *image = VK_NULL_HANDLE;
      return false;
   }
   vkGetImageMemoryRequirements(vvd.device, *image, &req);
   if (     !vvd_alloc(&req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mem)
         || vkBindImageMemory(vvd.device, *image, *mem, 0) != VK_SUCCESS)
      return false;
   return true;
}

static bool vvd_make_session(vvd_dec_t *d,
      const VkVideoCapabilitiesKHR *caps)
{
   static const VkExtensionProperties h264_std = {
      VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_EXTENSION_NAME,
      VK_STD_VULKAN_VIDEO_CODEC_H264_DECODE_SPEC_VERSION
   };
   static const VkExtensionProperties h265_std = {
      VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_EXTENSION_NAME,
      VK_STD_VULKAN_VIDEO_CODEC_H265_DECODE_SPEC_VERSION
   };
   static const VkExtensionProperties vp9_std = {
      VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_EXTENSION_NAME,
      VK_STD_VULKAN_VIDEO_CODEC_VP9_DECODE_SPEC_VERSION
   };
#ifdef HAVE_RAV1
   static const VkExtensionProperties av1_std = {
      VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_EXTENSION_NAME,
      VK_STD_VULKAN_VIDEO_CODEC_AV1_DECODE_SPEC_VERSION
   };
#endif
   VkVideoSessionCreateInfoKHR info;
   VkVideoSessionMemoryRequirementsKHR reqs[16];
   VkBindVideoSessionMemoryInfoKHR binds[16];
   uint32_t n = 0, i;

   info.sType                      = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
   info.pNext                      = NULL;
   info.queueFamilyIndex           = vvd.dec_family;
   info.flags                      = 0;
   info.pVideoProfile              = &d->profile;
   info.pictureFormat              = d->format;
   info.maxCodedExtent             = d->coded;
   info.referencePictureFormat     = d->format;
   info.maxDpbSlots                = d->slots;
   info.maxActiveReferencePictures = d->vp9_profile.sType ? RVP9_REFS_PER_FRAME : d->hevc
      ? (unsigned)(d->hevc->params.max_dec_pic_buffering - 1)
      : (caps->maxActiveReferencePictures < 16 ? caps->maxActiveReferencePictures : 16);
   info.pStdHeaderVersion          = d->vp9_profile.sType ? &vp9_std : d->hevc ? &h265_std : &h264_std;
#ifdef HAVE_RAV1
   if (d->av1)
   {
      info.maxActiveReferencePictures = STD_VIDEO_AV1_REFS_PER_FRAME;
      info.pStdHeaderVersion = &av1_std;
   }
#endif
   if (vvd.create_session(vvd.device, &info, NULL, &d->session) != VK_SUCCESS)
   {
      d->session = VK_NULL_HANDLE;
      return false;
   }

   if (     vvd.session_mem_reqs(vvd.device, d->session, &n, NULL) != VK_SUCCESS
         || n > 16)
      return false;
   for (i = 0; i < n; i++)
   {
      reqs[i].sType                             = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
      reqs[i].pNext                             = NULL;
      reqs[i].memoryBindIndex                   = 0;
      reqs[i].memoryRequirements.size           = 0;
      reqs[i].memoryRequirements.alignment      = 0;
      reqs[i].memoryRequirements.memoryTypeBits = 0;
   }
   if (vvd.session_mem_reqs(vvd.device, d->session, &n, reqs) != VK_SUCCESS)
      return false;
   for (i = 0; i < n; i++)
   {
      if (!vvd_alloc(&reqs[i].memoryRequirements,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               &d->session_mem[d->num_session_mem]))
         return false;
      binds[i].sType           = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
      binds[i].pNext           = NULL;
      binds[i].memoryBindIndex = reqs[i].memoryBindIndex;
      binds[i].memory          = d->session_mem[d->num_session_mem++];
      binds[i].memoryOffset    = 0;
      binds[i].memorySize      = reqs[i].memoryRequirements.size;
   }
   return !n || vvd.bind_session_mem(vvd.device, d->session, n, binds)
      == VK_SUCCESS;
}

static bool vvd_make_dpb_storage(vvd_dec_t *d, const VkVideoCapabilitiesKHR *caps)
{
   VkImageUsageFlags usage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR
                           | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                           | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   unsigned i;

   if (d->direct_dpb) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
   d->physical_slots = d->slots + (d->direct_dpb ? VVD_OUTS : 0);
   for (i = 0; i < d->slots; i++) d->slot_map[i] = i;
   if (!vvd_format_ok(d, usage)) return false;
   d->dpb_array = !(caps->flags
         & VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR);
   if (d->dpb_array)
   {
      /* every slot a layer of one image: as many layers as slots */
      VkPhysicalDeviceImageFormatInfo2 fi;
      VkImageFormatProperties2 fp;
      PFN_vkGetPhysicalDeviceImageFormatProperties2 get =
         (PFN_vkGetPhysicalDeviceImageFormatProperties2)
         vulkan_symbol_wrapper_instance_proc_addr()(vvd.instance,
               "vkGetPhysicalDeviceImageFormatProperties2");
      fi.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
      fi.pNext  = &d->profiles;
      fi.format = d->format;
      fi.type   = VK_IMAGE_TYPE_2D;
      fi.tiling = VK_IMAGE_TILING_OPTIMAL;
      fi.usage  = usage;
      fi.flags  = d->direct_dpb ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0;
      fp.sType                                = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
      fp.pNext                                = NULL;
      fp.imageFormatProperties.maxArrayLayers = 0;
      if (     !get || get(vvd.gpu, &fi, &fp) != VK_SUCCESS
            || fp.imageFormatProperties.maxArrayLayers < d->physical_slots)
         return false;
   }
   for (i = 0; i < (d->dpb_array ? 1u : d->physical_slots); i++)
      if (!vvd_make_image(&d->dpb[i], &d->dpb_mem[i], d->format, d->image,
               d->dpb_array ? d->physical_slots : 1, usage, &d->profiles,
               vvd.dec_family, vvd.xfer_family))
         return false;

   for (i = 0; i < d->physical_slots; i++)
   {
      VkImageViewUsageCreateInfo view_usage;
      VkImageViewCreateInfo info;
      view_usage.sType  = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
      view_usage.pNext  = NULL;
      view_usage.usage  = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR
                        | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
      info.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      info.pNext        = &view_usage;
      info.flags        = 0;
      info.image        = d->dpb[d->dpb_array ? 0 : i];
      info.viewType     = VK_IMAGE_VIEW_TYPE_2D;
      info.format       = d->format;
      info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      info.subresourceRange.baseMipLevel   = 0;
      info.subresourceRange.levelCount     = 1;
      info.subresourceRange.baseArrayLayer = d->dpb_array ? i : 0;
      info.subresourceRange.layerCount     = 1;
      if (vkCreateImageView(vvd.device, &info, NULL, &d->view[i])
            != VK_SUCCESS)
      {
         d->view[i] = VK_NULL_HANDLE;
         return false;
      }
   }
   if (d->direct_dpb)
      for (i = 0; i < d->physical_slots; i++)
         if (!vulkan_video_plane_views_layer(vvd.device,
                  d->dpb[d->dpb_array ? 0 : i], d->pixfmt,
                  d->dpb_array ? i : 0, d->dpb_planes[i]))
            return false;
   return true;
}

static bool vvd_make_dpb(vvd_dec_t *d, const VkVideoCapabilitiesKHR *caps)
{
   unsigned i;
   d->direct_dpb = vvd.general;
   if (vvd_make_dpb_storage(d, caps)) return true;
   if (!d->direct_dpb) return false;
   for (i = 0; i < VVD_PHYSICAL; i++)
   {
      if (d->dpb_planes[i][0]) vkDestroyImageView(vvd.device, d->dpb_planes[i][0], NULL);
      if (d->dpb_planes[i][1]) vkDestroyImageView(vvd.device, d->dpb_planes[i][1], NULL);
      if (d->view[i]) vkDestroyImageView(vvd.device, d->view[i], NULL);
      if (d->dpb[i]) vkDestroyImage(vvd.device, d->dpb[i], NULL);
      if (d->dpb_mem[i]) vkFreeMemory(vvd.device, d->dpb_mem[i], NULL);
   }
   memset(d->dpb_planes, 0, sizeof(d->dpb_planes));
   memset(d->view, 0, sizeof(d->view));
   memset(d->dpb, 0, sizeof(d->dpb));
   memset(d->dpb_mem, 0, sizeof(d->dpb_mem));
   d->direct_dpb = false;
   return vvd_make_dpb_storage(d, caps);
}

static bool vvd_make_output(vvd_dec_t *d, unsigned i, VkExtent2D extent)
{
   VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   VkImageFormatProperties props;
   if (vkGetPhysicalDeviceImageFormatProperties(vvd.gpu, d->format,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT, &props) == VK_SUCCESS
         && extent.width <= props.maxExtent.width && extent.height <= props.maxExtent.height
         && vvd_make_image(&d->out[i], &d->out_mem[i], d->format, extent, 1,
            usage | VK_IMAGE_USAGE_SAMPLED_BIT, NULL, vvd.xfer_family, vvd.gfx_family)
         && vulkan_video_plane_views(vvd.device, d->out[i], d->pixfmt, d->out_planes[i]))
      return true;
   if (d->out[i]) vkDestroyImage(vvd.device, d->out[i], NULL);
   if (d->out_mem[i]) vkFreeMemory(vvd.device, d->out_mem[i], NULL);
   d->out[i] = VK_NULL_HANDLE; d->out_mem[i] = VK_NULL_HANDLE;
   return vvd_make_image(&d->out[i], &d->out_mem[i], d->format, extent, 1,
         usage, NULL, vvd.xfer_family, vvd.gfx_family);
}

static bool vvd_make_outputs(vvd_dec_t *d)
{
   VkFormatProperties fp;
   VkExtent2D extent;
   unsigned i;

   vkGetPhysicalDeviceFormatProperties(vvd.gpu,
         d->format, &fp);
   if ((fp.optimalTilingFeatures
            & (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
             | VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
         != (VK_FORMAT_FEATURE_TRANSFER_SRC_BIT
           | VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
      return false;
   /* 4:2:0 images are of even size; the reader takes the odd edge's
    * chroma from the sample past it */
   extent.width  = (d->visible_w + 1) & ~1u;
   extent.height = (d->visible_h + 1) & ~1u;
   for (i = 0; i < VVD_OUTS; i++)
   {
      if (     (!d->direct_dpb && !vvd_make_output(d, i, extent))
            || !(d->out_done[i] = vvd_timeline()))
         return false;
   }
   return true;
}

static bool vvd_make_commands(vvd_dec_t *d)
{
   VkCommandPoolCreateInfo pool;
   VkCommandBufferAllocateInfo alloc;

   pool.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.pNext            = NULL;
   pool.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool.queueFamilyIndex = vvd.dec_family;
   if (vkCreateCommandPool(vvd.device, &pool, NULL, &d->dpool) != VK_SUCCESS)
   {
      d->dpool = VK_NULL_HANDLE;
      return false;
   }
   pool.queueFamilyIndex = vvd.xfer_family;
   if (vkCreateCommandPool(vvd.device, &pool, NULL, &d->xpool) != VK_SUCCESS)
   {
      d->xpool = VK_NULL_HANDLE;
      return false;
   }
   alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   alloc.pNext              = NULL;
   alloc.commandPool        = d->dpool;
   alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   alloc.commandBufferCount = VVD_RING;
   if (vkAllocateCommandBuffers(vvd.device, &alloc, d->dcb) != VK_SUCCESS)
      return false;
   alloc.commandPool        = d->xpool;
   return vkAllocateCommandBuffers(vvd.device, &alloc, d->xcb) == VK_SUCCESS;
}

static void vvd_lane_release(unsigned lane)
{
   retro_atomic_store_release_int(&vvd.lane_busy[lane], 0);
}

/* The player captures application policy before requesting a backend.
 * Native opens use the renderer capabilities published for this device. */
static void *vvd_open(void *user, const rh264_hw_sps_t *sps, unsigned slots)
{
   VkVideoCapabilitiesKHR caps;
   VkVideoDecodeCapabilitiesKHR dcaps;
   vvd_dec_t *d;
   unsigned lane;
   (void)user;

   if (!vvd.up || slots > VVD_MAX_SLOTS)
      return NULL;
   vvd_grave_reap(false);
   for (lane = 0; lane < vvd.lanes; lane++)
      if (retro_atomic_cas_int(&vvd.lane_busy[lane], 0, 1))
         break;
   if (lane >= vvd.lanes)
      return NULL;
   if (!(d = (vvd_dec_t*)calloc(1, sizeof(*d))))
   {
      vvd_lane_release(lane);
      return NULL;
   }
   d->lane          = lane;
   d->format        = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   d->pixfmt        = IMAGE_PIXFMT_NV12;
   d->slots         = slots;
   d->coded.width   = (uint32_t)sps->width_in_mbs  * 16;
   d->coded.height  = (uint32_t)sps->height_in_mbs * 16;
   d->visible_w     = (unsigned)sps->width;
   d->visible_h     = (unsigned)sps->height;
   d->crop_x        = (unsigned)sps->crop_x;
   d->crop_y        = (unsigned)sps->crop_y;
   d->params_gen    = 0;
   vkGetDeviceQueue(vvd.device, vvd.dec_family, lane, &d->dq);
   vkGetDeviceQueue(vvd.device, vvd.xfer_family, vvd.xfer_first + lane,
         &d->xq);

   if (!vvd_pick_profile(d, sps, slots, &caps, &dcaps))
   {
      RARCH_LOG("[Vulkan] Video decode: no profile for H.264 %d level %d %ux%u with %u slots; decoding on the CPU.\n",
            sps->profile_idc, sps->level_idc, d->coded.width,
            d->coded.height, slots);
      goto fail;
   }
   d->image.width     = (d->coded.width + caps.pictureAccessGranularity.width - 1)
      / (caps.pictureAccessGranularity.width ? caps.pictureAccessGranularity.width : 1)
      * (caps.pictureAccessGranularity.width ? caps.pictureAccessGranularity.width : 1);
   d->image.height    = (d->coded.height + caps.pictureAccessGranularity.height - 1)
      / (caps.pictureAccessGranularity.height ? caps.pictureAccessGranularity.height : 1)
      * (caps.pictureAccessGranularity.height ? caps.pictureAccessGranularity.height : 1);
   d->bs_offset_align = caps.minBitstreamBufferOffsetAlignment
      ? caps.minBitstreamBufferOffsetAlignment : 1;
   d->bs_size_align   = caps.minBitstreamBufferSizeAlignment
      ? caps.minBitstreamBufferSizeAlignment : 1;

   if (!vvd_format_ok(d, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR
                       | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
                       | VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
   {
      RARCH_LOG("[Vulkan] Video decode: no NV12 picture the GPU decodes into and copies from; decoding on the CPU.\n");
      goto fail;
   }
   if (     !vvd_make_session(d, &caps)
         || !vvd_make_dpb(d, &caps)
         || !vvd_make_outputs(d)
         || !vvd_make_commands(d)
         || !(d->tl = vvd_timeline()))
   {
      RARCH_WARN("[Vulkan] Video decode: could not set up a %ux%u session; decoding on the CPU.\n",
            d->coded.width, d->coded.height);
      goto fail;
   }
   retro_atomic_fetch_add_int(&vvd.live, 1);
   RARCH_LOG("[Vulkan] Video decode: H.264 %ux%u on the GPU, %u slots%s.\n",
         d->visible_w, d->visible_h, slots,
         d->dpb_array ? " in one image" : "");
   return d;

fail:
   vvd_destroy(d);
   vvd_lane_release(lane);
   return NULL;
}

/* ---- decode ---- */

static bool vvd_make_params(vvd_dec_t *d, const rh264_hw_pic_t *pic)
{
   const rh264_hw_sps_t *s = pic->sps;
   const rh264_hw_pps_t *p = pic->pps;
   StdVideoH264SequenceParameterSet sps;
   StdVideoH264PictureParameterSet pps;
   StdVideoH264ScalingLists sps_sl, pps_sl;
   VkVideoDecodeH264SessionParametersAddInfoKHR add;
   VkVideoDecodeH264SessionParametersCreateInfoKHR h264;
   VkVideoSessionParametersCreateInfoKHR info;

   /* the parameters the work in flight was recorded with stay until
    * it is done */
   if (d->params)
   {
      if (!vvd_reached(d->tl, d->tl_value))
         return false;
      vvd.destroy_params(vvd.device, d->params, NULL);
      d->params = VK_NULL_HANDLE;
   }

   memset(&sps, 0, sizeof(sps));
   sps.flags.constraint_set0_flag   = (s->constraint_flags >> 7) & 1;
   sps.flags.constraint_set1_flag   = (s->constraint_flags >> 6) & 1;
   sps.flags.constraint_set2_flag   = (s->constraint_flags >> 5) & 1;
   sps.flags.constraint_set3_flag   = (s->constraint_flags >> 4) & 1;
   sps.flags.constraint_set4_flag   = (s->constraint_flags >> 3) & 1;
   sps.flags.constraint_set5_flag   = (s->constraint_flags >> 2) & 1;
   sps.flags.direct_8x8_inference_flag = s->direct_8x8_inference ? 1 : 0;
   sps.flags.frame_mbs_only_flag    = 1;
   sps.flags.delta_pic_order_always_zero_flag =
      s->delta_pic_order_always_zero ? 1 : 0;
   sps.flags.gaps_in_frame_num_value_allowed_flag = s->gaps_allowed ? 1 : 0;
   sps.flags.qpprime_y_zero_transform_bypass_flag =
      s->transform_bypass ? 1 : 0;
   sps.flags.frame_cropping_flag    = s->frame_cropping ? 1 : 0;
   sps.flags.seq_scaling_matrix_present_flag = s->scaling_present ? 1 : 0;
   sps.profile_idc                  = (StdVideoH264ProfileIdc)s->profile_idc;
   sps.level_idc                    = vvd_level(s->level_idc);
   sps.chroma_format_idc            = STD_VIDEO_H264_CHROMA_FORMAT_IDC_420;
   sps.seq_parameter_set_id         = (uint8_t)s->sps_id;
   sps.bit_depth_luma_minus8        = (uint8_t)(s->bit_depth - 8);
   sps.bit_depth_chroma_minus8      = (uint8_t)(s->bit_depth - 8);
   sps.log2_max_frame_num_minus4    = (uint8_t)(s->log2_max_frame_num - 4);
   sps.pic_order_cnt_type           = (StdVideoH264PocType)s->poc_type;
   sps.offset_for_non_ref_pic       = s->offset_for_non_ref_pic;
   sps.offset_for_top_to_bottom_field = s->offset_for_top_to_bottom_field;
   sps.log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)(s->poc_type == 0
         ? s->log2_max_poc_lsb - 4 : 0);
   sps.num_ref_frames_in_pic_order_cnt_cycle =
      (uint8_t)s->num_ref_frames_in_poc_cycle;
   sps.max_num_ref_frames           = (uint8_t)s->max_num_ref_frames;
   sps.pic_width_in_mbs_minus1      = (uint32_t)s->width_in_mbs  - 1;
   sps.pic_height_in_map_units_minus1 = (uint32_t)s->height_in_mbs - 1;
   sps.frame_crop_left_offset       = (uint32_t)s->crop_left;
   sps.frame_crop_right_offset      = (uint32_t)s->crop_right;
   sps.frame_crop_top_offset        = (uint32_t)s->crop_top;
   sps.frame_crop_bottom_offset     = (uint32_t)s->crop_bottom;
   sps.pOffsetForRefFrame           = s->num_ref_frames_in_poc_cycle
      ? s->offset_for_ref_frame : NULL;
   if (s->scaling_present)
   {
      vvd_scaling(&sps_sl, &s->scaling);
      sps.pScalingLists             = &sps_sl;
   }

   memset(&pps, 0, sizeof(pps));
   pps.flags.transform_8x8_mode_flag = p->transform_8x8_mode ? 1 : 0;
   pps.flags.constrained_intra_pred_flag = p->constrained_intra_pred ? 1 : 0;
   pps.flags.deblocking_filter_control_present_flag =
      p->deblocking_filter_control_present ? 1 : 0;
   pps.flags.weighted_pred_flag     = p->weighted_pred ? 1 : 0;
   pps.flags.bottom_field_pic_order_in_frame_present_flag =
      p->bottom_field_pic_order_in_frame_present ? 1 : 0;
   pps.flags.entropy_coding_mode_flag = p->entropy_coding_mode ? 1 : 0;
   pps.flags.pic_scaling_matrix_present_flag = p->scaling_present ? 1 : 0;
   pps.seq_parameter_set_id         = (uint8_t)p->sps_id;
   pps.pic_parameter_set_id         = (uint8_t)p->pps_id;
   pps.num_ref_idx_l0_default_active_minus1 =
      (uint8_t)(p->num_ref_idx_l0_default - 1);
   pps.num_ref_idx_l1_default_active_minus1 =
      (uint8_t)(p->num_ref_idx_l1_default - 1);
   pps.weighted_bipred_idc          =
      (StdVideoH264WeightedBipredIdc)p->weighted_bipred_idc;
   pps.pic_init_qp_minus26          = (int8_t)(p->pic_init_qp - 26);
   pps.pic_init_qs_minus26          = (int8_t)(p->pic_init_qs - 26);
   pps.chroma_qp_index_offset       = (int8_t)p->chroma_qp_index_offset;
   pps.second_chroma_qp_index_offset =
      (int8_t)p->second_chroma_qp_index_offset;
   if (p->scaling_present)
   {
      vvd_scaling(&pps_sl, &p->scaling);
      pps.pScalingLists             = &pps_sl;
   }

   add.sType       =
      VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR;
   add.pNext       = NULL;
   add.stdSPSCount = 1;
   add.pStdSPSs    = &sps;
   add.stdPPSCount = 1;
   add.pStdPPSs    = &pps;
   h264.sType      =
      VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR;
   h264.pNext              = NULL;
   h264.maxStdSPSCount     = 1;
   h264.maxStdPPSCount     = 1;
   h264.pParametersAddInfo = &add;
   info.sType     = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
   info.pNext     = &h264;
   info.flags     = 0;
   info.videoSessionParametersTemplate = VK_NULL_HANDLE;
   info.videoSession                   = d->session;
   if (vvd.create_params(vvd.device, &info, NULL, &d->params) != VK_SUCCESS)
   {
      d->params = VK_NULL_HANDLE;
      return false;
   }
   d->params_gen  = pic->params_gen;
   d->have_params = true;
   return true;
}

/* Ring entry @k's bitstream buffer, at least @size bytes. */
static bool vvd_bitstream(vvd_dec_t *d, unsigned k, VkDeviceSize size)
{
   VkBufferCreateInfo info;
   VkMemoryRequirements req;
   VkMemoryAllocateInfo alloc;
   uint32_t type;
   VkDeviceSize cap;

   if (d->bs_cap[k] >= size)
      return true;
   if (d->bs_map[k])
      vkUnmapMemory(vvd.device, d->bs_mem[k]);
   if (d->bs[k])
      vkDestroyBuffer(vvd.device, d->bs[k], NULL);
   if (d->bs_mem[k])
      vkFreeMemory(vvd.device, d->bs_mem[k], NULL);
   d->bs_map[k] = NULL;
   d->bs[k]     = VK_NULL_HANDLE;
   d->bs_mem[k] = VK_NULL_HANDLE;
   d->bs_cap[k] = 0;

   for (cap = 256 * 1024; cap < size; cap *= 2)
      ;
   info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   info.pNext                 = &d->profiles;
   info.flags                 = 0;
   info.size                  = cap;
   info.usage                 = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices   = NULL;
   if (vkCreateBuffer(vvd.device, &info, NULL, &d->bs[k]) != VK_SUCCESS)
   {
      d->bs[k] = VK_NULL_HANDLE;
      return false;
   }
   vkGetBufferMemoryRequirements(vvd.device, d->bs[k], &req);
   type = vvd_memory_type(req.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
       | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
         VK_MEMORY_PROPERTY_PROTECTED_BIT);
   d->bs_coherent[k] = type != UINT32_MAX;
   if (type == UINT32_MAX)
      type = vvd_memory_type(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            VK_MEMORY_PROPERTY_PROTECTED_BIT);
   if (type == UINT32_MAX)
      return false;
   alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext           = NULL;
   alloc.allocationSize  = req.size;
   alloc.memoryTypeIndex = type;
   if (vkAllocateMemory(vvd.device, &alloc, NULL, &d->bs_mem[k]) != VK_SUCCESS)
   {
      d->bs_mem[k] = VK_NULL_HANDLE;
      return false;
   }
   if (     vkBindBufferMemory(vvd.device, d->bs[k], d->bs_mem[k], 0)
            != VK_SUCCESS
         || vkMapMemory(vvd.device, d->bs_mem[k], 0, VK_WHOLE_SIZE, 0,
            &d->bs_map[k]) != VK_SUCCESS)
   {
      d->bs_map[k] = NULL;
      return false;
   }
   d->bs_cap[k] = cap;
   return true;
}

static VkVideoPictureResourceInfoKHR vvd_resource(const vvd_dec_t *d,
      unsigned slot)
{
   VkVideoPictureResourceInfoKHR r;
   r.sType            = VK_STRUCTURE_TYPE_VIDEO_PICTURE_RESOURCE_INFO_KHR;
   r.pNext            = NULL;
   r.codedOffset.x    = 0;
   r.codedOffset.y    = 0;
   r.codedExtent      = d->coded;
   r.baseArrayLayer   = 0;
   r.imageViewBinding = d->view[d->slot_map[slot]];
   return r;
}

/* Keep every mapped reference and every outstanding reader's physical slot. */
static int vvd_target(vvd_dec_t *d, unsigned setup)
{
   bool held[VVD_PHYSICAL];
   unsigned i, p;
   int ready;
   if (!d->direct_dpb) return (int)setup;
   memset(held, 0, sizeof(held));
   for (i = 0; i < d->slots; i++)
      if (i != setup) held[d->slot_map[i]] = true;
   for (i = 0; i < VVD_OUTS; i++)
   {
      ready = vvd_try_wait(d->out_done[i], d->out_value[i]);
      if (ready < 0) return -1;
      if (ready) held[d->out_slot[i]] = true;
   }
   for (p = 0; p < d->physical_slots; p++)
      if (!held[p]) return (int)p;
   return (int)d->physical_slots;
}

static int vvd_record(vvd_dec_t *d, unsigned k, VkDeviceSize size,
      unsigned n, unsigned setup, const void *codec_picture)
{
   VkVideoBeginCodingInfoKHR begin;
   VkVideoEndCodingInfoKHR end;
   VkVideoDecodeInfoKHR info;
   VkCommandBuffer cb;
   VkVideoReferenceSlotInfoKHR *slots = d->slots_info;
   VkVideoReferenceSlotInfoKHR *begin_slots = d->begin_slots;
   VkVideoPictureResourceInfoKHR *res = d->res;
   int target = vvd_target(d, setup);
   if (target < 0) return -1;
   if ((unsigned)target == d->physical_slots) return 1;
   res[n].imageViewBinding = d->view[target];
   cb = d->dcb[k];
   if (!vvd_begin(cb))
      return -1;
   /* the slot's last picture is gone: it is written whole - after the
    * submission wait, so not under a copy out of it still running */
   vvd_barrier(cb, d->dpb[d->dpb_array ? 0 : target],
         d->dpb_array ? (unsigned)target : 0,
         VK_IMAGE_LAYOUT_UNDEFINED, d->direct_dpb ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
         VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0,
         VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
         VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR);

   begin.sType                  = VK_STRUCTURE_TYPE_VIDEO_BEGIN_CODING_INFO_KHR;
   begin.pNext                  = NULL;
   begin.flags                  = 0;
   begin.videoSession           = d->session;
   begin.videoSessionParameters = d->params;
   begin.referenceSlotCount     = n + 1;
   begin.pReferenceSlots        = begin_slots;
   vvd.cmd_begin(cb, &begin);
   if (!d->reset)
   {
      VkVideoCodingControlInfoKHR ctl;
      ctl.sType = VK_STRUCTURE_TYPE_VIDEO_CODING_CONTROL_INFO_KHR;
      ctl.pNext = NULL;
      ctl.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR;
      vvd.cmd_control(cb, &ctl);
   }

   info.sType               = VK_STRUCTURE_TYPE_VIDEO_DECODE_INFO_KHR;
   info.pNext               = codec_picture;
   info.flags               = 0;
   info.srcBuffer           = d->bs[k];
   info.srcBufferOffset     = 0;
   info.srcBufferRange      = size;
   info.dstPictureResource  = res[n];
   info.pSetupReferenceSlot = &slots[n];
   info.referenceSlotCount  = n;
   info.pReferenceSlots     = n ? slots : NULL;
   vvd.cmd_decode(cb, &info);

   end.sType = VK_STRUCTURE_TYPE_VIDEO_END_CODING_INFO_KHR;
   end.pNext = NULL;
   end.flags = 0;
   vvd.cmd_end(cb, &end);
   if (     vkEndCommandBuffer(cb) != VK_SUCCESS
         || !vvd_submit(d, d->dq, cb))
      return -1;
   d->slot_map[setup] = (unsigned)target;
   d->reset        = true;
   d->dcb_value[k] = d->tl_value;
   d->dring++;
   return 0;
}

static int vvd_decode(void *dec, const rh264_hw_pic_t *pic)
{
   vvd_dec_t *d = (vvd_dec_t*)dec;
   VkVideoPictureResourceInfoKHR *res         = d->res;
   StdVideoDecodeH264ReferenceInfo *std_ref   = d->std_ref;
   VkVideoDecodeH264DpbSlotInfoKHR *h264_slot = d->h264_slot;
   VkVideoReferenceSlotInfoKHR *slots         = d->slots_info;
   VkVideoReferenceSlotInfoKHR *begin_slots   = d->begin_slots;
   StdVideoDecodeH264PictureInfo std_pic;
   VkVideoDecodeH264PictureInfoKHR h264_pic;
   VkDeviceSize size;
   unsigned i, n, k;
   int ready;
   unsigned setup = (unsigned)pic->slot;

   if (setup >= d->slots || pic->num_refs > 16 || !pic->num_slices)
      return -1;
   if (d->have_params && pic->params_gen != d->params_gen)
   {
      ready = vvd_try_wait(d->tl, d->tl_value);
      if (ready != 0)
         return ready;
   }
   if (     (!d->have_params || pic->params_gen != d->params_gen)
         && !vvd_make_params(d, pic))
      return -1;

   /* the ring entry, once the work it last held is done */
   k = d->dring % VVD_RING;
   ready = vvd_try_wait(d->tl, d->dcb_value[k]);
   if (ready != 0)
      return ready;
   size = (pic->size + d->bs_size_align - 1) / d->bs_size_align
        * d->bs_size_align;
   if (!vvd_bitstream(d, k, size))
      return -1;
   memcpy(d->bs_map[k], pic->data, pic->size);
   memset((uint8_t*)d->bs_map[k] + pic->size, 0,
         (size_t)(size - pic->size));
   if (!d->bs_coherent[k])
   {
      VkMappedMemoryRange range;
      range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      range.pNext  = NULL;
      range.memory = d->bs_mem[k];
      range.offset = 0;
      range.size   = VK_WHOLE_SIZE;
      vkFlushMappedMemoryRanges(vvd.device, 1, &range);
   }

   /* the references, then the picture's own slot */
   n = 0;
   for (i = 0; i < pic->num_refs; i++)
   {
      const rh264_hw_ref_t *r = &pic->refs[i];
      if (r->slot < 0 || (unsigned)r->slot >= d->slots
            || (unsigned)r->slot == setup)
         return -1;
      memset(&std_ref[n], 0, sizeof(std_ref[n]));
      std_ref[n].flags.used_for_long_term_reference = r->long_term ? 1 : 0;
      std_ref[n].FrameNum       = (uint16_t)r->frame_num;
      std_ref[n].PicOrderCnt[0] = r->poc[0];
      std_ref[n].PicOrderCnt[1] = r->poc[1];
      res[n]                    = vvd_resource(d, (unsigned)r->slot);
      h264_slot[n].sType        = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
      h264_slot[n].pNext        = NULL;
      h264_slot[n].pStdReferenceInfo = &std_ref[n];
      slots[n].sType            = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
      slots[n].pNext            = &h264_slot[n];
      slots[n].slotIndex        = r->slot;
      slots[n].pPictureResource = &res[n];
      begin_slots[n]            = slots[n];
      begin_slots[n].pNext      = NULL;
      n++;
   }
   memset(&std_ref[n], 0, sizeof(std_ref[n]));
   std_ref[n].flags.used_for_long_term_reference = pic->long_term ? 1 : 0;
   std_ref[n].FrameNum       = (uint16_t)(pic->long_term
         ? pic->long_term_idx : pic->frame_num);
   std_ref[n].PicOrderCnt[0] = pic->poc[0];
   std_ref[n].PicOrderCnt[1] = pic->poc[1];
   res[n]                    = vvd_resource(d, setup);
   h264_slot[n].sType        = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_DPB_SLOT_INFO_KHR;
   h264_slot[n].pNext        = NULL;
   h264_slot[n].pStdReferenceInfo = &std_ref[n];
   slots[n].sType            = VK_STRUCTURE_TYPE_VIDEO_REFERENCE_SLOT_INFO_KHR;
   slots[n].pNext            = &h264_slot[n];
   slots[n].slotIndex        = (int32_t)setup;
   slots[n].pPictureResource = &res[n];
   /* bound for the decode, not yet a reference */
   begin_slots[n]            = slots[n];
   begin_slots[n].pNext      = NULL;
   begin_slots[n].slotIndex  = -1;

   memset(&std_pic, 0, sizeof(std_pic));
   std_pic.flags.is_intra      = pic->intra ? 1 : 0;
   std_pic.flags.IdrPicFlag    = pic->idr ? 1 : 0;
   std_pic.flags.is_reference  = pic->reference ? 1 : 0;
   std_pic.seq_parameter_set_id = (uint8_t)pic->sps->sps_id;
   std_pic.pic_parameter_set_id = (uint8_t)pic->pps->pps_id;
   std_pic.frame_num           = (uint16_t)pic->frame_num;
   std_pic.idr_pic_id          = (uint16_t)pic->idr_pic_id;
   std_pic.PicOrderCnt[0]      = pic->poc[0];
   std_pic.PicOrderCnt[1]      = pic->poc[1];
   h264_pic.sType              = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PICTURE_INFO_KHR;
   h264_pic.pNext              = NULL;
   h264_pic.pStdPictureInfo    = &std_pic;
   h264_pic.sliceCount         = pic->num_slices;
   h264_pic.pSliceOffsets      = pic->slice_offsets;

   return vvd_record(d, k, size, n, setup, &h264_pic);
}

/* The visible window of @slot into an output image of its own. */
static int vvd_output(void *dec, unsigned slot, video_hw_frame_t *out)
{
   vvd_dec_t *d = (vvd_dec_t*)dec;
   VkImageCopy regions[2];
   VkCommandBuffer cb;
   VkImage src;
   uint32_t layer;
   unsigned o, k, n;
   int ready;

   if (slot >= d->slots)
      return -1;
   for (n = 0; n < VVD_OUTS; n++)
   {
      o = (d->out_next + n) % VVD_OUTS;
      ready = vvd_try_wait(d->out_done[o], d->out_value[o]);
      if (ready < 0)
         return -1;
      if (!ready)
         break;
   }
   if (n == VVD_OUTS)
      return 1;
   if (d->direct_dpb)
   {
      unsigned p = d->slot_map[slot];
      memset(out, 0, sizeof(*out));
      memcpy(&out->image, &d->dpb[d->dpb_array ? 0 : p], sizeof(VkImage));
      memcpy(&out->plane_views[0], &d->dpb_planes[p][0], sizeof(VkImageView));
      memcpy(&out->plane_views[1], &d->dpb_planes[p][1], sizeof(VkImageView));
      memcpy(&out->wait, &d->tl, sizeof(VkSemaphore));
      memcpy(&out->done, &d->out_done[o], sizeof(VkSemaphore));
      out->wait_value = d->tl_value;
      out->done_value = ++d->out_value[o];
      out->abandon = vvd_abandon;
      out->width = d->visible_w;
      out->height = d->visible_h;
      out->pixfmt = d->pixfmt;
      out->yuv = d->yuv[slot];
      out->layout = VK_IMAGE_LAYOUT_GENERAL;
      out->array_slice = d->dpb_array ? p : 0;
      out->crop_x = d->crop_x;
      out->crop_y = d->crop_y;
      d->out_slot[o] = p;
      d->out_next = (o + 1) % VVD_OUTS;
      return 0;
   }
   k = d->xring % VVD_RING;
   ready = vvd_try_wait(d->tl, d->xcb_value[k]);
   if (ready != 0)
      return ready;

   src   = d->dpb[d->dpb_array ? 0 : d->slot_map[slot]];
   layer = d->dpb_array ? d->slot_map[slot] : 0;
   regions[0].srcSubresource.aspectMask     = VK_IMAGE_ASPECT_PLANE_0_BIT;
   regions[0].srcSubresource.mipLevel       = 0;
   regions[0].srcSubresource.baseArrayLayer = layer;
   regions[0].srcSubresource.layerCount     = 1;
   regions[0].srcOffset.x                   = (int32_t)d->crop_x;
   regions[0].srcOffset.y                   = (int32_t)d->crop_y;
   regions[0].srcOffset.z                   = 0;
   regions[0].dstSubresource.aspectMask     = VK_IMAGE_ASPECT_PLANE_0_BIT;
   regions[0].dstSubresource.mipLevel       = 0;
   regions[0].dstSubresource.baseArrayLayer = 0;
   regions[0].dstSubresource.layerCount     = 1;
   regions[0].dstOffset.x                   = 0;
   regions[0].dstOffset.y                   = 0;
   regions[0].dstOffset.z                   = 0;
   regions[0].extent.width                  = d->visible_w;
   regions[0].extent.height                 = d->visible_h;
   regions[0].extent.depth                  = 1;
   /* the chroma plane, in its own half-size samples */
   regions[1]                           = regions[0];
   regions[1].srcSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
   regions[1].dstSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
   regions[1].srcOffset.x               = (int32_t)(d->crop_x / 2);
   regions[1].srcOffset.y               = (int32_t)(d->crop_y / 2);
   regions[1].extent.width              = (d->visible_w + 1) / 2;
   regions[1].extent.height             = (d->visible_h + 1) / 2;

   cb = d->xcb[k];
   if (!vvd_begin(cb))
      return -1;
   vvd_barrier(cb, src, layer,
         VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0,
         VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT_KHR);
   vvd_barrier(cb, d->out[o], 0,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0,
         VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
   vkCmdCopyImage(cb, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         d->out[o], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, regions);
   /* the slot back for the decodes, the copy left for its reader */
   vvd_barrier(cb, src, layer,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
         VK_PIPELINE_STAGE_2_COPY_BIT_KHR, 0,
         VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0);
   vvd_barrier(cb, d->out[o], 0,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
         VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0);
   if (     vkEndCommandBuffer(cb) != VK_SUCCESS
         || !vvd_submit(d, d->xq, cb))
      return -1;
   d->xcb_value[k] = d->tl_value;
   d->out_next = (o + 1) % VVD_OUTS;
   d->xring++;

   memset(out, 0, sizeof(*out));
   memcpy(&out->image, &d->out[o], sizeof(d->out[o]));
   memcpy(&out->plane_views[0], &d->out_planes[o][0], sizeof(VkImageView));
   memcpy(&out->plane_views[1], &d->out_planes[o][1], sizeof(VkImageView));
   memcpy(&out->wait, &d->tl, sizeof(d->tl));
   memcpy(&out->done, &d->out_done[o], sizeof(d->out_done[o]));
   out->wait_value  = d->tl_value;
   out->done_value  = ++d->out_value[o];
   out->abandon     = vvd_abandon;
   out->owner       = NULL;
   out->width       = d->visible_w;
   out->height      = d->visible_h;
   out->pixfmt      = d->pixfmt;
   out->yuv         = d->yuv[slot];
   out->layout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   return 0;
}

/* Read the visible NV12 window back for cores that expose YUV in RAM. */
static int vvd_readback(void *dec, unsigned slot, uint8_t *planes[3])
{
   vvd_dec_t *d = (vvd_dec_t*)dec;
   VkBufferImageCopy regions[2];
   VkCommandBuffer cb;
   VkImage src;
   uint32_t layer, type;
   unsigned k, x, y, cw = (d->visible_w + 1) / 2;
   unsigned ch = (d->visible_h + 1) / 2;
   VkDeviceSize ysize = (VkDeviceSize)d->visible_w * d->visible_h;
   VkDeviceSize uv_offset = (ysize + 3) & ~(VkDeviceSize)3;
   const uint8_t *uv;
   if (slot >= d->slots)
      return -1;
   if (!d->readback_map)
   {
      VkBufferCreateInfo info;
      VkMemoryRequirements req;
      VkMemoryAllocateInfo alloc;
      info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      info.pNext                 = NULL;
      info.flags                 = 0;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices   = NULL;
      info.size                  = uv_offset + (VkDeviceSize)cw * ch * 2;
      info.usage                 = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
      if (vkCreateBuffer(vvd.device, &info, NULL, &d->readback) != VK_SUCCESS)
         return -1;
      vkGetBufferMemoryRequirements(vvd.device, d->readback, &req);
      type = vvd_memory_type(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_PROTECTED_BIT);
      d->readback_coherent = type != UINT32_MAX;
      if (type == UINT32_MAX)
         type = vvd_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
               VK_MEMORY_PROPERTY_PROTECTED_BIT);
      if (type == UINT32_MAX)
         return -1;
      alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      alloc.pNext           = NULL;
      alloc.allocationSize  = req.size;
      alloc.memoryTypeIndex = type;
      if (vkAllocateMemory(vvd.device, &alloc, NULL, &d->readback_mem) != VK_SUCCESS
            || vkBindBufferMemory(vvd.device, d->readback, d->readback_mem, 0) != VK_SUCCESS
            || vkMapMemory(vvd.device, d->readback_mem, 0, VK_WHOLE_SIZE, 0,
                  (void**)&d->readback_map) != VK_SUCCESS)
         return -1;
   }
   k = d->xring++ % VVD_RING;
   if (!vvd_wait(d->tl, d->xcb_value[k], VVD_WAIT_NS))
      return -1;
   src = d->dpb[d->dpb_array ? 0 : d->slot_map[slot]];
   layer = d->dpb_array ? d->slot_map[slot] : 0;
   regions[0].bufferOffset                    = 0;
   regions[0].bufferRowLength                 = 0;
   regions[0].bufferImageHeight               = 0;
   regions[0].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_PLANE_0_BIT;
   regions[0].imageSubresource.mipLevel       = 0;
   regions[0].imageSubresource.baseArrayLayer = layer;
   regions[0].imageSubresource.layerCount     = 1;
   regions[0].imageOffset.x                   = d->crop_x;
   regions[0].imageOffset.y                   = d->crop_y;
   regions[0].imageOffset.z                   = 0;
   regions[0].imageExtent.width               = d->visible_w;
   regions[0].imageExtent.height              = d->visible_h;
   regions[0].imageExtent.depth               = 1;
   regions[1] = regions[0];
   regions[1].bufferOffset = uv_offset;
   regions[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
   regions[1].imageOffset.x = d->crop_x / 2;
   regions[1].imageOffset.y = d->crop_y / 2;
   regions[1].imageExtent.width = cw;
   regions[1].imageExtent.height = ch;
   cb = d->xcb[k];
   if (!vvd_begin(cb))
      return -1;
   vvd_barrier(cb, src, layer, (d->direct_dpb ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR),
         (d->direct_dpb ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
         0, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT_KHR);
   vkCmdCopyImageToBuffer(cb, src, (d->direct_dpb ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
         d->readback, 2, regions);
   {
      VkBufferMemoryBarrier2KHR barrier;
      VkDependencyInfoKHR dep;
      barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
      barrier.pNext               = NULL;
      barrier.offset              = 0;
      barrier.srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
      barrier.srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
      barrier.dstStageMask        = VK_PIPELINE_STAGE_2_HOST_BIT_KHR;
      barrier.dstAccessMask       = VK_ACCESS_2_HOST_READ_BIT_KHR;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer              = d->readback;
      barrier.size                = VK_WHOLE_SIZE;
      dep.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
      dep.pNext                    = NULL;
      dep.dependencyFlags          = 0;
      dep.memoryBarrierCount       = 0;
      dep.pMemoryBarriers          = NULL;
      dep.imageMemoryBarrierCount  = 0;
      dep.pImageMemoryBarriers     = NULL;
      dep.bufferMemoryBarrierCount = 1;
      dep.pBufferMemoryBarriers    = &barrier;
      vvd.cmd_barrier2(cb, &dep);
   }
   vvd_barrier(cb, src, layer, (d->direct_dpb ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
         (d->direct_dpb ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR), VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
         0, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, 0);
   if (vkEndCommandBuffer(cb) != VK_SUCCESS || !vvd_submit(d, d->xq, cb))
      return -1;
   d->xcb_value[k] = d->tl_value;
   if (!vvd_wait(d->tl, d->tl_value, VVD_WAIT_NS))
      return -1;
   if (!d->readback_coherent)
   {
      VkMappedMemoryRange range;
      range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
      range.pNext  = NULL;
      range.offset = 0;
      range.memory = d->readback_mem;
      range.size   = VK_WHOLE_SIZE;
      if (vkInvalidateMappedMemoryRanges(vvd.device, 1, &range) != VK_SUCCESS)
         return -1;
   }
   memcpy(planes[0], d->readback_map, (size_t)ysize);
   uv = d->readback_map + uv_offset;
   for (y = 0; y < ch; y++)
      for (x = 0; x < cw; x++)
      {
         planes[1][y * cw + x] = uv[(y * cw + x) * 2];
         planes[2][y * cw + x] = uv[(y * cw + x) * 2 + 1];
      }
   return 0;
}

static void vvd_close(void *dec)
{
   vvd_dec_t *d = (vvd_dec_t*)dec;
   unsigned lane;
   if (!d)
      return;
   lane = d->lane;
   /* nothing more goes on its queues: the lane is the next one's,
    * the rest waits until its frames are read */
   if (vvd_settled(d))
      vvd_destroy(d);
   else
      vvd_grave_push(d);
   vvd_lane_release(lane);
   retro_atomic_fetch_add_int(&vvd.live, -1);
   vvd_grave_reap(false);
}

/* ---- the backend ---- */

#include "vulkan_video_decode_hevc.h"
#include "vulkan_video_decode_vp9.h"
#include "vulkan_video_decode_av1.h"

#define VVD_DEV_PROC(var, type, name) \
   ((vvd.var = (type)vkGetDeviceProcAddr(vvd.device, name)) != NULL)
#define VVD_INST_PROC(var, type, name) \
   ((vvd.var = (type)vulkan_symbol_wrapper_instance_proc_addr()( \
         vvd.instance, name)) != NULL)

bool vulkan_video_decode_init(const vulkan_context_t *ctx)
{
   unsigned i;
   if (!ctx || !ctx->video_lanes || !ctx->device
         || !(ctx->video_codec_operations & (VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR
               | VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR
#ifdef HAVE_RAV1
               | VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR
#endif
#ifdef HAVE_RVP9
               | VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR
#endif
               )))
      return false;
   if (vvd.up)
      return vvd.device == ctx->device;
   vvd.instance    = ctx->instance;
   vvd.gpu         = ctx->gpu;
   vvd.device      = ctx->device;
   vvd.codecs      = ctx->video_codec_operations;
   vvd.general     = (ctx->flags & VK_CTX_FLAG_VIDEO_GENERAL) != 0;
   vvd.mem         = ctx->memory_properties;
#ifdef _WIN32
   /* NVIDIA H.264 corruption; 610.47 also faults on AV1 intra block copy. */
   vvd.h264_broken = ctx->gpu_properties.vendorID == 0x10de
      && (ctx->gpu_properties.driverVersion == ((610u << 22) | (47u << 14))
         || (ctx->gpu_properties.deviceID == 0x2b85
            && ctx->gpu_properties.driverVersion == ((621u << 22) | (1u << 14))));
#ifdef HAVE_RAV1
   vvd.av1_intrabc_broken = ctx->gpu_properties.vendorID == 0x10de
      && ctx->gpu_properties.driverVersion == ((610u << 22) | (47u << 14));
#endif
#endif
   vvd.gfx_family  = ctx->graphics_queue_index;
   vvd.dec_family  = ctx->video_decode_family;
   vvd.xfer_family = ctx->video_xfer_family;
   vvd.xfer_first  = ctx->video_xfer_first;
   vvd.lanes       = ctx->video_lanes < VVD_MAX_LANES
      ? ctx->video_lanes : VVD_MAX_LANES;
   if (     !VVD_INST_PROC(get_caps, PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR,
               "vkGetPhysicalDeviceVideoCapabilitiesKHR")
         || !VVD_INST_PROC(get_formats, PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR,
               "vkGetPhysicalDeviceVideoFormatPropertiesKHR")
         || !VVD_DEV_PROC(create_session, PFN_vkCreateVideoSessionKHR,
               "vkCreateVideoSessionKHR")
         || !VVD_DEV_PROC(destroy_session, PFN_vkDestroyVideoSessionKHR,
               "vkDestroyVideoSessionKHR")
         || !VVD_DEV_PROC(session_mem_reqs, PFN_vkGetVideoSessionMemoryRequirementsKHR,
               "vkGetVideoSessionMemoryRequirementsKHR")
         || !VVD_DEV_PROC(bind_session_mem, PFN_vkBindVideoSessionMemoryKHR,
               "vkBindVideoSessionMemoryKHR")
         || !VVD_DEV_PROC(create_params, PFN_vkCreateVideoSessionParametersKHR,
               "vkCreateVideoSessionParametersKHR")
         || !VVD_DEV_PROC(destroy_params, PFN_vkDestroyVideoSessionParametersKHR,
               "vkDestroyVideoSessionParametersKHR")
         || !VVD_DEV_PROC(cmd_begin, PFN_vkCmdBeginVideoCodingKHR,
               "vkCmdBeginVideoCodingKHR")
         || !VVD_DEV_PROC(cmd_end, PFN_vkCmdEndVideoCodingKHR,
               "vkCmdEndVideoCodingKHR")
         || !VVD_DEV_PROC(cmd_control, PFN_vkCmdControlVideoCodingKHR,
               "vkCmdControlVideoCodingKHR")
         || !VVD_DEV_PROC(cmd_decode, PFN_vkCmdDecodeVideoKHR,
               "vkCmdDecodeVideoKHR")
         || !VVD_DEV_PROC(cmd_barrier2, PFN_vkCmdPipelineBarrier2KHR,
               "vkCmdPipelineBarrier2KHR")
         || !VVD_DEV_PROC(submit2, PFN_vkQueueSubmit2KHR,
               "vkQueueSubmit2KHR")
         || !VVD_DEV_PROC(wait_sems, PFN_vkWaitSemaphoresKHR,
               "vkWaitSemaphoresKHR")
         || !VVD_DEV_PROC(signal_sem, PFN_vkSignalSemaphoreKHR,
               "vkSignalSemaphoreKHR")
         || !VVD_DEV_PROC(counter, PFN_vkGetSemaphoreCounterValueKHR,
               "vkGetSemaphoreCounterValueKHR"))
   {
      RARCH_WARN("[Vulkan] Video decode: entry points missing; previews decode on the CPU.\n");
      return false;
   }
   for (i = 0; i < VVD_MAX_LANES; i++)
      retro_atomic_store_release_int(&vvd.lane_busy[i], 0);
   retro_atomic_store_release_int(&vvd.live, 0);
   retro_atomic_store_release_ptr(&vvd.grave, NULL);
   vvd.ops.user   = NULL;
   vvd.ops.open   = vvd_open;
   vvd.ops.decode = vvd_decode;
   vvd.ops.output = vvd_output;
   vvd.ops.close  = vvd_close;
   vvd.ops.readback = vvd_readback;
   vvd.ops.api = VIDEO_HW_API_VULKAN;
   vvd.up         = true;
   vvd.hevc.user = NULL;
   vvd.hevc.open = vvd_hevc_open;
   vvd.hevc.close = vvd_hevc_close;
   if (vvd.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR)
   {
#ifdef _WIN32
      if (!vvd.h264_broken)
#endif
         rh264_set_hw_backend(&vvd.ops);
   }
   if (vvd.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
      rh265_set_hw_backend(&vvd.hevc);
#ifdef HAVE_RVP9
   vvd.vp9.user = NULL; vvd.vp9.open = vvd_vp9_open; vvd.vp9.close = vvd_vp9_close;
   vvd.vp9.packet_decoder = false;
   vvd.vp9.open_color = NULL;
   vvd.vp9.open_config = NULL;
   vvd.vp9.restart = NULL;
   vvd.vp9.drain = NULL;
   if (vvd.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR)
      rvp9_set_hw_backend(&vvd.vp9);
#endif
#ifdef HAVE_RAV1
   vvd.av1.packet_decoder = false;
   vvd.av1.open_config = NULL;
   vvd.av1.drain = NULL;
   vvd.av1.user = NULL;
   vvd.av1.open = vvd_av1_open;
   vvd.av1.close = vvd_av1_close;
   if (vvd.codecs & VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR)
      rav1_set_hw_backend(&vvd.av1);
#endif
   return true;
}

#if defined(_WIN32) && !defined(__WINRT__) \
   && (defined(HAVE_D3D11) || defined(HAVE_D3D12))
#include "vulkan_video_win32.h"
#endif

void vulkan_video_decode_deinit(void)
{
   if (!vvd.up)
      return;
#if defined(_WIN32) && !defined(__WINRT__) \
   && (defined(HAVE_D3D11) || defined(HAVE_D3D12))
   if (vvd.win32_fallback)
   {
      d3d11_video_decode_deinit();
      vvd.win32_fallback = false;
   }
#endif
#ifdef HAVE_RAV1
   rav1_unset_hw_backend(&vvd.av1);
#endif
   rh264_set_hw_backend(NULL);
   rh265_unset_hw_backend(&vvd.hevc);
#ifdef HAVE_RVP9
   rvp9_unset_hw_backend(&vvd.vp9);
#endif
   if (retro_atomic_load_acquire_int(&vvd.live))
      RARCH_ERR("[Vulkan] Video decode: %d decoder(s) still open at teardown.\n",
            retro_atomic_load_acquire_int(&vvd.live));
   /* the device is idle: what waited on reads goes now */
   vvd_grave_reap(true);
   vvd.up = false;
}
