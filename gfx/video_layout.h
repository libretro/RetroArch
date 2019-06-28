#ifndef VIDEO_LAYOUT_H
#define VIDEO_LAYOUT_H
#include "video_layout/types.h"
#include <boolean.h>
#include <formats/image.h>

typedef struct video_layout_render_info
{
   void                      *video_driver_data;
   void                      *video_driver_frame_data;

   video_layout_bounds_t      bounds;
   video_layout_orientation_t orientation;
   video_layout_color_t       color;
}
video_layout_render_info_t;

typedef enum video_layout_led
{
   VIDEO_LAYOUT_LED_7,        /* digit with . */
   VIDEO_LAYOUT_LED_8_GTS1,   /* digit with vertical split */
   VIDEO_LAYOUT_LED_14,       /* alphanumeric */
   VIDEO_LAYOUT_LED_14_SC,    /* alphanumeric with ., */
   VIDEO_LAYOUT_LED_16,       /* full alphanumeric */
   VIDEO_LAYOUT_LED_16_SC     /* full alphanumeric with ., */
}
video_layout_led_t;

typedef struct video_layout_render_interface
{
   void *(*take_image)  (void *video_driver_data, struct texture_image image);
   void  (*free_image)  (void *video_driver_data, void *image);

   void  (*layer_begin) (const video_layout_render_info_t *info);

   void  (*screen)      (const video_layout_render_info_t *info, int screen_index);
   void  (*image)       (const video_layout_render_info_t *info, void *image_handle, void *alpha_handle);
   void  (*text)        (const video_layout_render_info_t *info, const char *str);
   void  (*counter)     (const video_layout_render_info_t *info, int value);
   void  (*rect)        (const video_layout_render_info_t *info);
   void  (*ellipse)     (const video_layout_render_info_t *info);
   void  (*led_dot)     (const video_layout_render_info_t *info, int dot_count, int dot_mask);
   void  (*led_seg)     (const video_layout_render_info_t *info, video_layout_led_t seg_layout, int seg_mask);

   void  (*layer_end)   (const video_layout_render_info_t *info, video_layout_blend_t blend_type);
}
video_layout_render_interface_t;

void        video_layout_init       (void *video_driver_data, const video_layout_render_interface_t *render);
void        video_layout_deinit     (void);

int         video_layout_io_assign         (const char *name, int base_value);
int         video_layout_io_get            (int index);
void        video_layout_io_set            (int index, int value);

bool        video_layout_load              (const char *path);
bool        video_layout_valid             (void);

int         video_layout_view_count        (void);
const char *video_layout_view_name         (int index);

int         video_layout_view_select       (int index);
int         video_layout_view_cycle        (void);
int         video_layout_view_index        (void);

void        video_layout_view_change       (void);
bool        video_layout_view_on_change    (void);
void        video_layout_view_fit_bounds   (video_layout_bounds_t bounds);

int         video_layout_layer_count       (void);
void        video_layout_layer_render      (void *video_driver_frame_data, int index);

const video_layout_bounds_t
           *video_layout_screen            (int index);
int         video_layout_screen_count      (void);

#endif
