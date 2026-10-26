#ifndef BCM_HOST_STUB_H
#define BCM_HOST_STUB_H
/* Stand-in for the Raspberry Pi userland's bcm_host.h: the types,
 * enumerants and prototypes gfx/drivers/dispmanx_gfx.c uses, with the
 * values and signatures of the userland headers, so the driver is
 * compiled - and its calls are checked - where /opt/vc is not
 * installed. */
#include <stddef.h>
#include <stdint.h>

/* vcinclude/common.h, which bcm_host.h pulls in */
#ifndef ALIGN_UP
#define ALIGN_UP(p, n) (((size_t)(p) + ((n) - 1)) & ~((n) - 1))
#endif

typedef uint32_t DISPMANX_DISPLAY_HANDLE_T;
typedef uint32_t DISPMANX_UPDATE_HANDLE_T;
typedef uint32_t DISPMANX_ELEMENT_HANDLE_T;
typedef uint32_t DISPMANX_RESOURCE_HANDLE_T;
typedef uint32_t DISPMANX_PROTECTION_T;

#define DISPMANX_PROTECTION_NONE 0

#define DISPMANX_ID_MAIN_LCD  0
#define DISPMANX_ID_HDMI0     2
#define DISPMANX_ID_SDTV      3
#define DISPMANX_ID_FORCE_LCD 4
#define DISPMANX_ID_HDMI1     7

typedef struct tag_VC_RECT
{
   int32_t x;
   int32_t y;
   int32_t width;
   int32_t height;
} VC_RECT_T;

typedef enum
{
   VC_IMAGE_MIN      = 0,
   VC_IMAGE_RGB565   = 1,
   VC_IMAGE_RGB888   = 5,
   VC_IMAGE_RGBA32   = 15,
   VC_IMAGE_RGBA16   = 18,
   VC_IMAGE_ARGB8888 = 43,
   VC_IMAGE_XRGB8888 = 44
} VC_IMAGE_TYPE_T;

typedef enum
{
   DISPMANX_FLAGS_ALPHA_FROM_SOURCE       = 0,
   DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS  = 1,
   DISPMANX_FLAGS_ALPHA_FIXED_NON_ZERO    = 2,
   DISPMANX_FLAGS_ALPHA_FIXED_EXCEED_0X07 = 3,
   DISPMANX_FLAGS_ALPHA_PREMULT           = 1 << 16,
   DISPMANX_FLAGS_ALPHA_MIX               = 1 << 17
} DISPMANX_FLAGS_ALPHA_T;

typedef struct
{
   DISPMANX_FLAGS_ALPHA_T flags;
   uint32_t opacity;
   DISPMANX_RESOURCE_HANDLE_T mask;
} VC_DISPMANX_ALPHA_T;

typedef struct DISPMANX_CLAMP_T DISPMANX_CLAMP_T;

typedef enum
{
   DISPMANX_NO_ROTATE  = 0,
   DISPMANX_ROTATE_90  = 1,
   DISPMANX_ROTATE_180 = 2,
   DISPMANX_ROTATE_270 = 3,
   DISPMANX_FLIP_HRIZ  = 1 << 16,
   DISPMANX_FLIP_VERT  = 1 << 17
} DISPMANX_TRANSFORM_T;

typedef void (*DISPMANX_CALLBACK_FUNC_T)(DISPMANX_UPDATE_HANDLE_T u,
      void *arg);

void bcm_host_init(void);
void bcm_host_deinit(void);
int32_t graphics_get_display_size(const uint16_t display_number,
      uint32_t *width, uint32_t *height);
int vc_gencmd_send(const char *format, ...);

int vc_dispmanx_rect_set(VC_RECT_T *rect, uint32_t x_offset,
      uint32_t y_offset, uint32_t width, uint32_t height);
DISPMANX_DISPLAY_HANDLE_T vc_dispmanx_display_open(uint32_t device);
int vc_dispmanx_display_close(DISPMANX_DISPLAY_HANDLE_T display);
DISPMANX_RESOURCE_HANDLE_T vc_dispmanx_resource_create(
      VC_IMAGE_TYPE_T type, uint32_t width, uint32_t height,
      uint32_t *native_image_handle);
int vc_dispmanx_resource_write_data(DISPMANX_RESOURCE_HANDLE_T res,
      VC_IMAGE_TYPE_T src_type, int src_pitch, void *src_address,
      const VC_RECT_T *rect);
int vc_dispmanx_resource_read_data(DISPMANX_RESOURCE_HANDLE_T handle,
      const VC_RECT_T *p_rect, void *dst_address, uint32_t dst_pitch);
int vc_dispmanx_snapshot(DISPMANX_DISPLAY_HANDLE_T display,
      DISPMANX_RESOURCE_HANDLE_T snapshot_resource,
      DISPMANX_TRANSFORM_T transform);
int vc_dispmanx_resource_delete(DISPMANX_RESOURCE_HANDLE_T res);
DISPMANX_UPDATE_HANDLE_T vc_dispmanx_update_start(int32_t priority);
DISPMANX_ELEMENT_HANDLE_T vc_dispmanx_element_add(
      DISPMANX_UPDATE_HANDLE_T update, DISPMANX_DISPLAY_HANDLE_T display,
      int32_t layer, const VC_RECT_T *dest_rect,
      DISPMANX_RESOURCE_HANDLE_T src, const VC_RECT_T *src_rect,
      DISPMANX_PROTECTION_T protection, VC_DISPMANX_ALPHA_T *alpha,
      DISPMANX_CLAMP_T *clamp, DISPMANX_TRANSFORM_T transform);
int vc_dispmanx_element_change_source(DISPMANX_UPDATE_HANDLE_T update,
      DISPMANX_ELEMENT_HANDLE_T element, DISPMANX_RESOURCE_HANDLE_T src);
int vc_dispmanx_element_change_attributes(
      DISPMANX_UPDATE_HANDLE_T update, DISPMANX_ELEMENT_HANDLE_T element,
      uint32_t change_flags, int32_t layer, uint8_t opacity,
      const VC_RECT_T *dest_rect, const VC_RECT_T *src_rect,
      DISPMANX_RESOURCE_HANDLE_T mask, DISPMANX_TRANSFORM_T transform);
int vc_dispmanx_element_remove(DISPMANX_UPDATE_HANDLE_T update,
      DISPMANX_ELEMENT_HANDLE_T element);
int vc_dispmanx_update_submit(DISPMANX_UPDATE_HANDLE_T update,
      DISPMANX_CALLBACK_FUNC_T cb_func, void *cb_arg);
int vc_dispmanx_update_submit_sync(DISPMANX_UPDATE_HANDLE_T update);

#endif
