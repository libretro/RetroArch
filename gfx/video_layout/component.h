#ifndef VIDEO_LAYOUT_COMPONENT_H
#define VIDEO_LAYOUT_COMPONENT_H

#include "internal.h"
#include "component_attr.h"

typedef enum comp_type
{
   VIDEO_LAYOUT_C_UNKNOWN,
   VIDEO_LAYOUT_C_SCREEN,
   VIDEO_LAYOUT_C_RECT,
   VIDEO_LAYOUT_C_DISK,
   VIDEO_LAYOUT_C_IMAGE,
   VIDEO_LAYOUT_C_TEXT,
   VIDEO_LAYOUT_C_COUNTER,
   VIDEO_LAYOUT_C_DOTMATRIX_X1,
   VIDEO_LAYOUT_C_DOTMATRIX_H5,
   VIDEO_LAYOUT_C_DOTMATRIX_H8,
   VIDEO_LAYOUT_C_LED_7,
   VIDEO_LAYOUT_C_LED_8_GTS1,
   VIDEO_LAYOUT_C_LED_14,
   VIDEO_LAYOUT_C_LED_14_SC,
   VIDEO_LAYOUT_C_LED_16,
   VIDEO_LAYOUT_C_LED_16_SC,
   VIDEO_LAYOUT_C_REEL
} comp_type_t;

union comp_attr
{
   c_attr_screen_t   screen;
   c_attr_image_t    image;
   c_attr_text_t     text;
   c_attr_counter_t  counter;
};

typedef struct component
{
   comp_type_t                type;
   video_layout_bounds_t      bounds;
   video_layout_bounds_t      render_bounds;
   video_layout_orientation_t orientation;
   video_layout_color_t       color;
   int                        enabled_state;
   union comp_attr            attr;
} component_t;

void component_init   (component_t *comp, comp_type_t type);
void component_copy   (component_t *comp, const component_t *src);
void component_deinit (component_t *comp);

#endif
