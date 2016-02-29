#ifndef VKICD_H
#define VKICD_H

#include "vk_platform.h"

/*
 * The ICD must reserve space for a pointer for the loader's dispatch
 * table, at the start of <each object>.
 * The ICD must initialize this variable using the SET_LOADER_MAGIC_VALUE macro.
 */

#define ICD_LOADER_MAGIC   0x01CDC0DE

typedef union _VK_LOADER_DATA {
  uintptr_t loaderMagic;
  void *loaderData;
} VK_LOADER_DATA;

static inline void set_loader_magic_value(void* pNewObject) {
    VK_LOADER_DATA *loader_info = (VK_LOADER_DATA *) pNewObject;
    loader_info->loaderMagic = ICD_LOADER_MAGIC;
}

static inline bool valid_loader_magic_value(void* pNewObject) {
    const VK_LOADER_DATA *loader_info = (VK_LOADER_DATA *) pNewObject;
    return (loader_info->loaderMagic & 0xffffffff) == ICD_LOADER_MAGIC;
}

/*
 * Windows and Linux ICDs will treat VkSurfaceKHR as a pointer to a struct that
 * contains the platform-specific connection and surface information.
 */
typedef enum _VkIcdWsiPlatform {
    VK_ICD_WSI_PLATFORM_MIR,
    VK_ICD_WSI_PLATFORM_WAYLAND,
    VK_ICD_WSI_PLATFORM_WIN32,
    VK_ICD_WSI_PLATFORM_XCB,
    VK_ICD_WSI_PLATFORM_XLIB,
} VkIcdWsiPlatform;

typedef struct _VkIcdSurfaceBase {
    VkIcdWsiPlatform   platform;
} VkIcdSurfaceBase;

#ifdef VK_USE_PLATFORM_MIR_KHR
typedef struct _VkIcdSurfaceMir {
    VkIcdSurfaceBase   base;
    MirConnection*     connection;
    MirSurface*        mirSurface;
} VkIcdSurfaceMir;
#endif // VK_USE_PLATFORM_MIR_KHR

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
typedef struct _VkIcdSurfaceWayland {
    VkIcdSurfaceBase   base;
    struct wl_display* display;
    struct wl_surface* surface;
} VkIcdSurfaceWayland;
#endif // VK_USE_PLATFORM_WAYLAND_KHR

#ifdef VK_USE_PLATFORM_WIN32_KHR
typedef struct _VkIcdSurfaceWin32 {
    VkIcdSurfaceBase   base;
    HINSTANCE          hinstance;
    HWND               hwnd;
} VkIcdSurfaceWin32;
#endif // VK_USE_PLATFORM_WIN32_KHR

#ifdef VK_USE_PLATFORM_XCB_KHR
typedef struct _VkIcdSurfaceXcb {
    VkIcdSurfaceBase   base;
    xcb_connection_t*  connection;
    xcb_window_t       window;
} VkIcdSurfaceXcb;
#endif // VK_USE_PLATFORM_XCB_KHR

#ifdef VK_USE_PLATFORM_XLIB_KHR
typedef struct _VkIcdSurfaceXlib {
    VkIcdSurfaceBase   base;
    Display*           dpy;
    Window             window;
} VkIcdSurfaceXlib;
#endif // VK_USE_PLATFORM_XLIB_KHR

#endif // VKICD_H
