#ifndef VIDEO_LAYOUT_INTERNAL_H
#define VIDEO_LAYOUT_INTERNAL_H
#include <stddef.h>
#include <boolean.h>
#include <retro_miscellaneous.h>

#include "types.h"

char  *init_string (const char *src);
void   set_string  (char **string, const char *src);
bool   vec_size    (void **target, size_t elem_size, int count);

bool   is_decimal  (const char *str);
int    get_int     (const char *str);
float  get_dec     (const char *str);

video_layout_color_t  make_color        (void);
video_layout_color_t  make_color_white  (void);
video_layout_color_t  make_color_v      (float v);
video_layout_color_t  make_color_rgb    (float r, float g, float b);
video_layout_color_t  make_color_rgba   (float r, float g, float b, float a);
void                  color_mod         (video_layout_color_t *dst, const video_layout_color_t *src);

video_layout_bounds_t make_bounds       (void);
video_layout_bounds_t make_bounds_unit  (void);
video_layout_bounds_t bounds_union      (const video_layout_bounds_t *a, const video_layout_bounds_t *b);
void                  bounds_scale      (video_layout_bounds_t *dst, const video_layout_bounds_t *dim);
bool                  bounds_valid      (const video_layout_bounds_t *bounds);

#endif
