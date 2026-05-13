#ifndef VULKAN_OHOS_H_
#define VULKAN_OHOS_H_ 1

/*
** Copyright 2015-2025 The Khronos Group Inc.
**
** SPDX-License-Identifier: Apache-2.0
*/

/**
 * @addtogroup Vulkan
 * @{
 *
 * @brief Provides Vulkan capabilities extended by OpenHarmony.
 * This module provides extended APIs for creating a Vulkan surface and obtaining <b>OH_NativeBuffer</b> and
 * <b>OH_NativeBuffer</b> properties through <b>OHNativeWindow</b>.
 * This header is generated from the Khronos Vulkan XML API Registry.
 *
 * @since 10
 * @version 1.0
 */

/**
 * @file vulkan_ohos.h
 *
 * @brief Declares the Vulkan APIs extended by OpenHarmony. File to include: <vulkan/vulkan.h>
 *
 * @kit ArkGraphics2D
 * @library libvulkan.so
 * @syscap SystemCapability.Graphic.Vulkan
 * @since 10
 * @version 1.0
 */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Defines the surface extension of OpenHarmony.
 * @since 10
 * @version 1.0
 */
#define VK_OHOS_surface 1

/**
 * @brief Defines the <b>OHNativeWindow</b> struct.
 * @since 10
 * @version 1.0
 */
typedef struct NativeWindow OHNativeWindow;

/**
 * @brief Defines the surface extension version of OpenHarmony.
 * @since 10
 * @version 1.0
 */
#define VK_OHOS_SURFACE_SPEC_VERSION      1

/**
 * @brief Defines the surface extension name of OpenHarmony.
 * @since 10
 * @version 1.0
 */
#define VK_OHOS_SURFACE_EXTENSION_NAME    "VK_OHOS_surface"

/**
 * @brief Defines the bit mask of the VkFlags type used for the creation of a Vulkan surface.
 * It is a reserved flag type.
 * @since 10
 * @version 1.0
 */
typedef VkFlags VkSurfaceCreateFlagsOHOS;

/**
 * @brief Defines the parameters required for creating a Vulkan surface.
 * @since 10
 * @version 1.0
 */
typedef struct VkSurfaceCreateInfoOHOS {
    /**
     * Struct sType is a VkStructureType value identifying this structure.
     * it must be VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS.
     */
    VkStructureType             sType;
    /**
     * pNext is NULL or a pointer to a structure extending this structure. pNext must be NULL.
     */
    const void*                 pNext;
    /**
     * flags is reserved for future use. flags must be 0.
     */
    VkSurfaceCreateFlagsOHOS    flags;
    /**
     * window: is a pointer to a OHNativeWindow to associate the surface with.
     */
    OHNativeWindow*             window;
} VkSurfaceCreateInfoOHOS;

/**
 * @brief Defines the function pointer for creating a Vulkan surface.
 *
 * @param instance <b>Vulkan</b> instance.
 * @param pCreateInfo Pointer to the <b>VkSurfaceCreateInfoOHOS</b> struct,
 * including the parameters required for creating a Vulkan surface.
 * @param pAllocator Pointer to a callback function for custom memory allocation.
 * If custom memory allocation is not required, pass in <b>NULL</b>, and the default memory allocation function is used.
 * @param pSurface Pointer to the Vulkan surface created. The type is <b>VkSurfaceKHR</b>.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 */
typedef VkResult (VKAPI_PTR *PFN_vkCreateSurfaceOHOS)(
    VkInstance                     instance,
    const VkSurfaceCreateInfoOHOS* pCreateInfo,
    const VkAllocationCallbacks*   pAllocator,
    VkSurfaceKHR*                  pSurface
);

#ifndef VK_NO_PROTOTYPES

/**
 * @brief Creates a Vulkan surface.
 *
 * @param instance <b>Vulkan</b> instance.
 * @param pCreateInfo Pointer to the <b>VkSurfaceCreateInfoOHOS</b> struct,
 * including the parameters required for creating a Vulkan surface.
 * @param pAllocator Pointer to a callback function for custom memory allocation.
 * If custom memory allocation is not required, pass in <b>NULL</b>, and the default memory allocation function is used.
 * @param pSurface Pointer to the Vulkan surface created. The type is <b>VkSurfaceKHR</b>.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 */
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSurfaceOHOS(
    VkInstance                                  instance,
    const VkSurfaceCreateInfoOHOS*              pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface)
    __attribute__((__availability__(ohos, introduced=10.0.0)));
#endif


#define VK_OHOS_native_buffer 1
struct OHBufferHandle;
#define VK_OHOS_NATIVE_BUFFER_SPEC_VERSION 1
#define VK_OHOS_NATIVE_BUFFER_EXTENSION_NAME "VK_OHOS_native_buffer"

/**
 * @brief move to vk_ohos_native_buffer.h
 * @since 10
 * @deprecated since 23
 */
typedef enum VkSwapchainImageUsageFlagBitsOHOS {
    VK_SWAPCHAIN_IMAGE_USAGE_SHARED_BIT_OHOS = 0x00000001,
    VK_SWAPCHAIN_IMAGE_USAGE_FLAG_BITS_MAX_ENUM_OHOS = 0x7FFFFFFF
} VkSwapchainImageUsageFlagBitsOHOS;
typedef VkFlags VkSwapchainImageUsageFlagsOHOS;
/**
 * @brief move to vk_ohos_native_buffer.h
 * @since 10
 * @deprecated since 23
 */
typedef struct VkNativeBufferOHOS {
    VkStructureType    sType;
    const void*        pNext;
    struct OHBufferHandle*      handle;
} VkNativeBufferOHOS;

/**
 * @brief move to vk_ohos_native_buffer.h
 * @since 10
 * @deprecated since 23
 */
typedef struct VkSwapchainImageCreateInfoOHOS {
    VkStructureType                   sType;
    const void*                       pNext;
    VkSwapchainImageUsageFlagsOHOS    usage;
} VkSwapchainImageCreateInfoOHOS;

/**
 * @brief move to vk_ohos_native_buffer.h
 * @since 10
 * @deprecated since 23
 */
typedef struct VkPhysicalDevicePresentationPropertiesOHOS {
    VkStructureType    sType;
    const void*        pNext;
    VkBool32           sharedImage;
} VkPhysicalDevicePresentationPropertiesOHOS;

/**
 * @brief this type is deprecated, please use PFN_vkAcquireImageOHOS instead
 * @since 10
 * @deprecated since 10
 */
typedef VkResult (VKAPI_PTR *PFN_vkSetNativeFenceFdOpenHarmony)(VkDevice device, int32_t nativeFenceFd, VkSemaphore semaphore, VkFence fence);

/**
 * @brief this type is deprecated, please use PFN_vkQueueSignalReleaseImageOHOS instead
 * @since 10
 * @deprecated since 10
 */
typedef VkResult (VKAPI_PTR *PFN_vkGetNativeFenceFdOpenHarmony)(VkQueue queue, uint32_t waitSemaphoreCount, const VkSemaphore* pWaitSemaphores, VkImage image, int32_t* pNativeFenceFd);

/**
 * @brief move to vk_ohos_native_buffer.h
 * @since 10
 * @deprecated since 23
 */
typedef VkResult (VKAPI_PTR *PFN_vkGetSwapchainGrallocUsageOHOS)(VkDevice device, VkFormat format, VkImageUsageFlags imageUsage, uint64_t* grallocUsage);

/**
 * @brief move to vk_ohos_native_buffer.h
 * @since 10
 * @deprecated since 23
 */
typedef VkResult (VKAPI_PTR *PFN_vkAcquireImageOHOS)(VkDevice device, VkImage image, int32_t nativeFenceFd, VkSemaphore semaphore, VkFence fence);

/**
 * @brief move to vk_ohos_native_buffer.h
 * @since 10
 * @deprecated since 23
 */
typedef VkResult (VKAPI_PTR *PFN_vkQueueSignalReleaseImageOHOS)(VkQueue queue, uint32_t waitSemaphoreCount, const VkSemaphore* pWaitSemaphores, VkImage image, int32_t* pNativeFenceFd);

#ifndef VK_NO_PROTOTYPES
/**
 * @brief this interface is deprecated, please use vkAcquireImageOHOS instead
 * @since 10
 * @deprecated since 10
 */
VKAPI_ATTR VkResult VKAPI_CALL vkSetNativeFenceFdOpenHarmony(
    VkDevice                                    device,
    int32_t                                     nativeFenceFd,
    VkSemaphore                                 semaphore,
    VkFence                                     fence)
    __attribute__((__availability__(ohos, introduced=10.0.0)));

/**
 * @brief this interface is deprecated, please use vkQueueSignalReleaseImageOHOS instead
 * @since 10
 * @deprecated since 10
 */
VKAPI_ATTR VkResult VKAPI_CALL vkGetNativeFenceFdOpenHarmony(
    VkQueue                                     queue,
    uint32_t                                    waitSemaphoreCount,
    const VkSemaphore*                          pWaitSemaphores,
    VkImage                                     image,
    int32_t*                                    pNativeFenceFd)
    __attribute__((__availability__(ohos, introduced=10.0.0)));

/**
 * @brief Returns the appropriate gralloc usage flag based on
 * the given Vulkan device, image format, and image usage flag.
 * move to vk_ohos_native_buffer.h
 *
 * @param device <b>VkDevice</b> instance.
 * @param format Image format.
 * @param imageUsage Image usage flag.
 * @param grallocUsage Pointer to the gralloc usage flag.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 * @deprecated since 23
 */
VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainGrallocUsageOHOS(
    VkDevice                                    device,
    VkFormat                                    format,
    VkImageUsageFlags                           imageUsage,
    uint64_t*                                   grallocUsage)
    __attribute__((__availability__(ohos, introduced=10.0.0)));

/**
 * @brief Obtains the ownership of the swap chain image and imports the fence of the external signal
 * to the VkSemaphore and VkFence objects.
 * move to vk_ohos_native_buffer.h
 *
 * @param device <b>VkDevice</b> instance.
 * @param image Vulkan image to obtain.
 * @param nativeFenceFd File descriptor of the native fence.
 * @param semaphore Vulkan semaphore indicating that the image is available.
 * @param fence Vulkan fence used for synchronization when the image acquisition is complete.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 * @deprecated since 23
 */
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireImageOHOS(
    VkDevice                                    device,
    VkImage                                     image,
    int32_t                                     nativeFenceFd,
    VkSemaphore                                 semaphore,
    VkFence                                     fence)
    __attribute__((__availability__(ohos, introduced=10.0.0)));

/**
 * @brief Sends a signal to the system hardware buffer to release an image once it is no longer needed
 * so that other components can access it.
 * move to vk_ohos_native_buffer.h
 *
 * @param queue Handle to the Vulkan queue.
 * @param waitSemaphoreCount Number of semaphores to wait on.
 * @param pWaitSemaphores Pointer to the array of semaphores to wait on.
 * @param images Handle to the Vulkan image to be released.
 * @param pNativeFenceFd Pointer to the file descriptor of the fence.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 * @deprecated since 23
 */
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSignalReleaseImageOHOS(
    VkQueue                                     queue,
    uint32_t                                    waitSemaphoreCount,
    const VkSemaphore*                          pWaitSemaphores,
    VkImage                                     image,
    int32_t*                                    pNativeFenceFd)
    __attribute__((__availability__(ohos, introduced=10.0.0)));
#endif


/**
 * @brief Defines the external memory extension of OpenHarmony.
 * @since 10
 * @version 1.0
 */
#define VK_OHOS_external_memory 1

/**
 * @brief Defines the <b>OH_NativeBuffer</b> struct.
 * @since 10
 * @version 1.0
 */
struct OH_NativeBuffer;

/**
 * @brief Defines the external memory extension version of OpenHarmony.
 * @since 10
 * @version 1.0
 */
#define VK_OHOS_EXTERNAL_MEMORY_SPEC_VERSION 1

/**
 * @brief Defines the external memory extension name of OpenHarmony.
 * @since 10
 * @version 1.0
 */
#define VK_OHOS_EXTERNAL_MEMORY_EXTENSION_NAME "VK_OHOS_external_memory"

/**
 * @brief Defines the usage of a <b>OH_NativeBuffer</b>.
 * @since 10
 * @version 1.0
 */
typedef struct VkNativeBufferUsageOHOS {
    /**
     * Struct type is a VkStructureType value identifying this structure.
     * sType must be VK_STRUCTURE_TYPE_NATIVE_BUFFER_USAGE_OHOS.
     */
    VkStructureType    sType;
    /**
     * pNext is NULL or a pointer to a structure extending this structure.
     */
    void*              pNext;
    /**
     * @brief OHOSNativeBufferUsage returns the Open Harmony OS buffer usage flags.
     */
    uint64_t           OHOSNativeBufferUsage;
} VkNativeBufferUsageOHOS;

/**
 * @brief Defines the properties of a <b>OH_NativeBuffer</b>.
 * @since 10
 * @version 1.0
 */
typedef struct VkNativeBufferPropertiesOHOS {
    /**
     * Struct type.
     */
    VkStructureType    sType;
    /**
     * Pointer to the next-level struct.
     */
    void*              pNext;
    /**
     * @brief Defines the size of the occupied memory.
     */
    VkDeviceSize       allocationSize;
    /**
     * @brief Defines the memory type.
     */
    uint32_t           memoryTypeBits;
} VkNativeBufferPropertiesOHOS;

/**
 * @brief Defines the format properties of a <b>OH_NativeBuffer</b>.
 * @since 10
 * @version 1.0
 */
typedef struct VkNativeBufferFormatPropertiesOHOS {
    /**
     * Struct type.
     */
    VkStructureType                  sType;
    /**
     * Pointer to the next-level struct.
     */
    void*                            pNext;
    /**
     * Format properties.
     */
    VkFormat                         format;
    /**
     * Externally defined format.
     */
    uint64_t                         externalFormat;
    /**
     * Features of the externally defined format.
     */
    VkFormatFeatureFlags             formatFeatures;
    /**
     * A group of VkComponentSwizzles.
     */
    VkComponentMapping               samplerYcbcrConversionComponents;
    /**
     * Color model.
     */
    VkSamplerYcbcrModelConversion    suggestedYcbcrModel;
    /**
     * Color value range.
     */
    VkSamplerYcbcrRange              suggestedYcbcrRange;
    /**
     * X chrominance offset.
     */
    VkChromaLocation                 suggestedXChromaOffset;
    /**
     * Y chrominance offset.
     */
    VkChromaLocation                 suggestedYChromaOffset;
} VkNativeBufferFormatPropertiesOHOS;

/**
 * @brief Defines the pointer to an <b>OH_NativeBuffer</b> struct.
 * @since 10
 * @version 1.0
 */
typedef struct VkImportNativeBufferInfoOHOS {
    /**
     * Struct type.
     */
    VkStructureType            sType;
    /**
     * Pointer to the next-level struct.
     */
    const void*                pNext;
    /**
     * Pointer to an <b>OH_NativeBuffer</b> struct.
     */
    struct OH_NativeBuffer*    buffer;
} VkImportNativeBufferInfoOHOS;

/**
 * @brief Defines a struct used to obtain an <b>OH_NativeBuffer</b> from the Vulkan memory.
 * @since 10
 * @version 1.0
 */
typedef struct VkMemoryGetNativeBufferInfoOHOS {
    /**
     * sType is a VkStructureType value identifying this structure.
     * sType must be VK_STRUCTURE_TYPE_MEMORY_GET_NATIVE_BUFFER_INFO_OHOS.
     */
    VkStructureType    sType;
    /**
     * pNext is NULL or a pointer to a structure extending this structure. pNext must be NULL
     */
    const void*        pNext;
    /**
     * memory is a valid VkDeviceMemory object from which the Open Harmony OS native buffer will be exported.
     * memory must be a valid VkDeviceMemory handle
     */
    VkDeviceMemory     memory;
} VkMemoryGetNativeBufferInfoOHOS;

/**
 * @brief Defines an externally defined format.
 * @since 10
 * @version 1.0
 */
typedef struct VkExternalFormatOHOS {
    /**
     * sType is a VkStructureType value identifying this structure.
     * sType must be VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_OHOS.
     */
    VkStructureType    sType;
    /**
     * pNext is NULL or a pointer to a structure extending this structure.
     */
    void*              pNext;
    /**
     * externalFormat is an implementation-defined identifier for the external format.
     */
    uint64_t           externalFormat;
} VkExternalFormatOHOS;

/**
 * @brief Defines a function pointer used to obtain <b>OH_NativeBuffer</b> properties.
 *
 * @param device <b>VkDevice</b> instance.
 * @param buffer Pointer to the <b>OH_NativeBuffer</b> struct.
 * @param pProperties Pointer to the struct holding the properties of <b>OH_NativeBuffer</b>.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 */
typedef VkResult (VKAPI_PTR *PFN_vkGetNativeBufferPropertiesOHOS)(
    VkDevice                      device,
    const struct OH_NativeBuffer* buffer,
    VkNativeBufferPropertiesOHOS* pProperties
);

/**
 * @brief Defines a function pointer used to obtain an <b>OH_NativeBuffer</b> instance.
 *
 * @param device <b>VkDevice</b> instance.
 * @param pInfo Pointer to the <b>VkMemoryGetNativeBufferInfoOHOS</b> struct.
 * @param pBuffer Double pointer to the <b>OH_NativeBuffer</b> obtained.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 */
typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryNativeBufferOHOS)(
    VkDevice                               device,
    const VkMemoryGetNativeBufferInfoOHOS* pInfo,
    struct OH_NativeBuffer**               pBuffer
);

#ifndef VK_NO_PROTOTYPES
/**
 * @brief Obtains the properties of an <b>OH_NativeBuffer</b> instance.
 *
 * @param device <b>VkDevice</b> instance.
 * @param buffer Pointer to the <b>OH_NativeBuffer</b> struct.
 * @param pProperties Pointer to the struct holding the properties of <b>OH_NativeBuffer</b>.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 */
VKAPI_ATTR VkResult VKAPI_CALL vkGetNativeBufferPropertiesOHOS(
    VkDevice                                    device,
    const struct OH_NativeBuffer*               buffer,
    VkNativeBufferPropertiesOHOS*               pProperties)
    __attribute__((__availability__(ohos, introduced=10.0.0)));

/**
 * @brief Obtains an <b>OH_NativeBuffer</b> instance.
 *
 * @param device <b>VkDevice</b> instance.
 * @param pInfo Pointer to the <b>VkMemoryGetNativeBufferInfoOHOS</b> struct.
 * @param pBuffer Double pointer to the <b>OH_NativeBuffer</b> obtained.
 * @return Returns <b>VK_SUCCESS</b> if the execution is successful;
 * returns an error code of the VkResult type otherwise.
 * @since 10
 * @version 1.0
 */
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryNativeBufferOHOS(
    VkDevice                                    device,
    const VkMemoryGetNativeBufferInfoOHOS*      pInfo,
    struct OH_NativeBuffer**                    pBuffer)
    __attribute__((__availability__(ohos, introduced=10.0.0)));
#endif

#ifdef __cplusplus
}
#endif

#endif
