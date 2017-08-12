/*
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __VULKAN_INTEL_H__
#define __VULKAN_INTEL_H__

#include "vulkan.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL 1024
typedef struct VkDmaBufImageCreateInfo_
{
    VkStructureType                             sType;                      /* Must be VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL */
    const void*                                 pNext;                      /* Pointer to next structure. */
    int                                         fd;
    VkFormat                                    format;
    VkExtent3D                                  extent;         /* Depth must be 1 */
    uint32_t                                    strideInBytes;
} VkDmaBufImageCreateInfo;

typedef VkResult (VKAPI_PTR *PFN_vkCreateDmaBufImageINTEL)(VkDevice device, const VkDmaBufImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMem, VkImage* pImage);

#ifdef VK_PROTOTYPES

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDmaBufImageINTEL(
    VkDevice                                    _device,
    const VkDmaBufImageCreateInfo*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkDeviceMemory*                             pMem,
    VkImage*                                    pImage);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __VULKAN_INTEL_H__ */
