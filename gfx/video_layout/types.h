#ifndef VIDEO_LAYOUT_TYPES_H
#define VIDEO_LAYOUT_TYPES_H

#include <stdint.h>

typedef uint8_t video_layout_orientation_t;

#define VIDEO_LAYOUT_FLIP_X  1
#define VIDEO_LAYOUT_FLIP_Y  2
#define VIDEO_LAYOUT_SWAP_XY 4

#define VIDEO_LAYOUT_ROT0    0
#define VIDEO_LAYOUT_ROT90   VIDEO_LAYOUT_SWAP_XY | VIDEO_LAYOUT_FLIP_X
#define VIDEO_LAYOUT_ROT180  VIDEO_LAYOUT_FLIP_X  | VIDEO_LAYOUT_FLIP_Y
#define VIDEO_LAYOUT_ROT270  VIDEO_LAYOUT_SWAP_XY | VIDEO_LAYOUT_FLIP_Y

typedef enum video_layout_blend
{
   VIDEO_LAYOUT_BLEND_ALPHA = 0,
   VIDEO_LAYOUT_BLEND_ADD,
   VIDEO_LAYOUT_BLEND_MOD
} video_layout_blend_t;

typedef enum video_layout_text_align
{
   VIDEO_LAYOUT_TEXT_ALIGN_CENTER = 0,
   VIDEO_LAYOUT_TEXT_ALIGN_LEFT,
   VIDEO_LAYOUT_TEXT_ALIGN_RIGHT
} video_layout_text_align_t;

typedef struct video_layout_color
{
   float r;
   float g;
   float b;
   float a;
} video_layout_color_t;

typedef struct video_layout_bounds
{
   float x;
   float y;
   float w;
   float h;
} video_layout_bounds_t;

#endif
