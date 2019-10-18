#ifndef VIDEO_LAYOUT_COMPONENT_ATTR_H
#define VIDEO_LAYOUT_COMPONENT_ATTR_H

typedef struct c_attr_screen
{
   int index;
} c_attr_screen_t;

typedef struct c_attr_image
{
   char *file;
   char *alpha_file;
   int   image_idx;
   int   alpha_idx;
   bool  loaded;
} c_attr_image_t;

typedef struct c_attr_text
{
   char               *string;
   video_layout_text_align_t align;
} c_attr_text_t;

typedef struct c_attr_counter
{
   int                 digits;
   int                 max_state;
   video_layout_text_align_t align;
} c_attr_counter_t;

#endif
