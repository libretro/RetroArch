/*  RetroArch - A frontend for libretro.
 *  Copyright (C)      2016 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef _VULKAN_VKSYM_H
#define _VULKAN_VKSYM_H

#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <vulkan/vulkan.h>

#define VKFUNC(sym) (vkcfp->sym)

#define VK_PROTOTYPES

#ifdef HAVE_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#ifdef HAVE_MIR
#define VK_USE_PLATFORM_MIR_KHR
#endif

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef HAVE_XCB
#define VK_USE_PLATFORM_XCB_KHR
#endif

#ifdef HAVE_XLIB
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <vulkan/vulkan.h>

typedef struct vulkan_context_fp
{
   /* Instance */
   PFN_vkCreateInstance                          vkCreateInstance;
   PFN_vkDestroyInstance                         vkDestroyInstance;

   /* Device */
   PFN_vkCreateDevice                            vkCreateDevice;
   PFN_vkDestroyDevice                           vkDestroyDevice;
   PFN_vkDeviceWaitIdle                          vkDeviceWaitIdle;

   /* Device Memory */
   PFN_vkAllocateMemory                          vkAllocateMemory;
   PFN_vkFreeMemory                              vkFreeMemory;

   /* Command Function Pointers */
   PFN_vkGetInstanceProcAddr                     vkGetInstanceProcAddr;
   PFN_vkGetDeviceProcAddr                       vkGetDeviceProcAddr;

   /* Buffers */
   PFN_vkCreateBuffer                            vkCreateBuffer;
   PFN_vkDestroyBuffer                           vkDestroyBuffer;

   /* Fences */
   PFN_vkCreateFence                             vkCreateFence;
   PFN_vkDestroyFence                            vkDestroyFence;
   PFN_vkResetFences                             vkResetFences;
   PFN_vkWaitForFences                           vkWaitForFences;

   /* Semaphores */
   PFN_vkCreateSemaphore                         vkCreateSemaphore;
   PFN_vkDestroySemaphore                        vkDestroySemaphore;

   /* Images */
   PFN_vkCreateImage                             vkCreateImage;
   PFN_vkDestroyImage                            vkDestroyImage;
   PFN_vkGetImageSubresourceLayout               vkGetImageSubresourceLayout;

   /* Images (Resource Memory Association) */
   PFN_vkGetBufferMemoryRequirements             vkGetBufferMemoryRequirements;
   PFN_vkBindBufferMemory                        vkBindBufferMemory;
   PFN_vkBindImageMemory                         vkBindImageMemory;

   /* Image Views */
   PFN_vkCreateImageView                         vkCreateImageView;
   PFN_vkDestroyImageView                        vkDestroyImageView;

   /* Image Views (Resource Memory Association) */
   PFN_vkGetImageMemoryRequirements              vkGetImageMemoryRequirements;

   /* Queues */
   PFN_vkGetDeviceQueue                          vkGetDeviceQueue;
   PFN_vkQueueWaitIdle                           vkQueueWaitIdle;

   /* Pipelines */
   PFN_vkDestroyPipeline                         vkDestroyPipeline;
   PFN_vkCreateGraphicsPipelines                 vkCreateGraphicsPipelines;

   /* Pipeline Layouts */
   PFN_vkCreatePipelineLayout                    vkCreatePipelineLayout;
   PFN_vkDestroyPipelineLayout                   vkDestroyPipelineLayout;

   /* Pipeline Cache */
   PFN_vkCreatePipelineCache                     vkCreatePipelineCache;
   PFN_vkDestroyPipelineCache                    vkDestroyPipelineCache;

   /* Pipeline Barriers */
   PFN_vkCmdPipelineBarrier                      vkCmdPipelineBarrier;

   /* Descriptor pools */
   PFN_vkCreateDescriptorPool                    vkCreateDescriptorPool;
   PFN_vkDestroyDescriptorPool                   vkDestroyDescriptorPool;

   /* Descriptor sets */
   PFN_vkAllocateDescriptorSets                  vkAllocateDescriptorSets;
   PFN_vkFreeDescriptorSets                      vkFreeDescriptorSets;
   PFN_vkCmdBindDescriptorSets                   vkCmdBindDescriptorSets;
   PFN_vkUpdateDescriptorSets                    vkUpdateDescriptorSets;

   /* Descriptor Set Layout */
   PFN_vkCreateDescriptorSetLayout               vkCreateDescriptorSetLayout;
   PFN_vkDestroyDescriptorSetLayout              vkDestroyDescriptorSetLayout;

   /* Command Buffers */
   PFN_vkCreateCommandPool                       vkCreateCommandPool;
   PFN_vkDestroyCommandPool                      vkDestroyCommandPool;
   PFN_vkBeginCommandBuffer                      vkBeginCommandBuffer;
   PFN_vkEndCommandBuffer                        vkEndCommandBuffer;
   PFN_vkResetCommandBuffer                      vkResetCommandBuffer;
   PFN_vkFreeCommandBuffers                      vkFreeCommandBuffers;
   PFN_vkAllocateCommandBuffers                  vkAllocateCommandBuffers;

   /* Command Buffer Submission */
   PFN_vkQueueSubmit                             vkQueueSubmit;

   /* Framebuffers */
   PFN_vkCreateFramebuffer                       vkCreateFramebuffer;
   PFN_vkDestroyFramebuffer                      vkDestroyFramebuffer;

   /* Memory allocation */
   PFN_vkMapMemory                               vkMapMemory;
   PFN_vkUnmapMemory                             vkUnmapMemory;

   /* Samplers */
   PFN_vkCreateSampler                           vkCreateSampler;
   PFN_vkDestroySampler                          vkDestroySampler;

   /* Render Passes */
   PFN_vkCreateRenderPass                        vkCreateRenderPass;
   PFN_vkDestroyRenderPass                       vkDestroyRenderPass;

   /* Image commands */
   PFN_vkCmdCopyImage                            vkCmdCopyImage;

   /* Pipeline commands */
   PFN_vkCmdBindPipeline                         vkCmdBindPipeline;

   /* Vertex input descriptions */
   PFN_vkCmdBindVertexBuffers                    vkCmdBindVertexBuffers;

   /* Render Pass commands */
   PFN_vkCmdBeginRenderPass                      vkCmdBeginRenderPass;
   PFN_vkCmdEndRenderPass                        vkCmdEndRenderPass;

   /* Clear commands */
   PFN_vkCmdClearAttachments                     vkCmdClearAttachments;

   /* Drawing commands */
   PFN_vkCmdDraw                                 vkCmdDraw;

   /* Fragment operations */
   PFN_vkCmdSetScissor                           vkCmdSetScissor;

   /* Fixed-function vertex postprocessing */
   PFN_vkCmdSetViewport                          vkCmdSetViewport;

   /* Shaders */
   PFN_vkCreateShaderModule                      vkCreateShaderModule;
   PFN_vkDestroyShaderModule                     vkDestroyShaderModule;

   PFN_vkGetPhysicalDeviceFormatProperties       vkGetPhysicalDeviceFormatProperties;
   PFN_vkEnumeratePhysicalDevices                vkEnumeratePhysicalDevices;
   PFN_vkGetPhysicalDeviceProperties             vkGetPhysicalDeviceProperties;
   PFN_vkGetPhysicalDeviceMemoryProperties       vkGetPhysicalDeviceMemoryProperties;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties  vkGetPhysicalDeviceQueueFamilyProperties;

   PFN_vkGetPhysicalDeviceSurfaceSupportKHR      vkGetPhysicalDeviceSurfaceSupportKHR;
   PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
   PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      vkGetPhysicalDeviceSurfaceFormatsKHR;
   PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;

   /* Swapchains */
   PFN_vkCreateSwapchainKHR                      vkCreateSwapchainKHR;
   PFN_vkDestroySwapchainKHR                     vkDestroySwapchainKHR;
   PFN_vkGetSwapchainImagesKHR                   vkGetSwapchainImagesKHR;

   PFN_vkAcquireNextImageKHR                     vkAcquireNextImageKHR;
   PFN_vkQueuePresentKHR                         vkQueuePresentKHR;
   PFN_vkDestroySurfaceKHR                       vkDestroySurfaceKHR;

   /* Platform-specific surface functions */
#ifdef _WIN32
   PFN_vkCreateWin32SurfaceKHR                   vkCreateWin32SurfaceKHR;
#endif
#ifdef HAVE_XCB
   PFN_vkCreateXcbSurfaceKHR                     vkCreateXcbSurfaceKHR;
#endif
#ifdef HAVE_XLIB
   PFN_vkCreateXlibSurfaceKHR                    vkCreateXlibSurfaceKHR;
#endif
#ifdef ANDROID
   PFN_vkCreateAndroidSurfaceKHR                 vkCreateAndroidSurfaceKHR;
#endif
#ifdef HAVE_WAYLAND
   PFN_vkCreateWaylandSurfaceKHR                 vkCreateWaylandSurfaceKHR;
#endif
#ifdef HAVE_MIR
   PFN_vkCreateMirSurfaceKHR                     vkCreateMirSurfaceKHR;
#endif
} vulkan_context_fp_t;

#endif
