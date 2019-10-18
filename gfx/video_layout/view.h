#ifndef VIDEO_LAYOUT_VIEW_H
#define VIDEO_LAYOUT_VIEW_H

#include "internal.h"
#include "element.h"

typedef struct layer
{
   char                 *name;
   video_layout_blend_t  blend;

   element_t            *elements;
   int                   elements_count;
} layer_t;

typedef struct view
{
   char                  *name;
   video_layout_bounds_t  bounds;
   video_layout_bounds_t  render_bounds;

   layer_t               *layers;
   int                    layers_count;

   video_layout_bounds_t *screens;
   int                    screens_count;
} view_t;

typedef struct view_array
{
   view_t *views;
   int     views_count;
} view_array_t;

void       layer_init         (layer_t *layer, const char *name);
void       layer_deinit       (layer_t *layer);
element_t *layer_add_element  (layer_t *layer);

void       view_init          (view_t *view, const char *name);
void       view_deinit        (view_t *view);
layer_t   *view_find_layer    (view_t *view, const char *name);
layer_t   *view_emplace_layer (view_t *view, const char *name);
void       view_sort_layers   (view_t *view);
void       view_normalize     (view_t *view);
void       view_count_screens (view_t *view);

void       view_array_init    (view_array_t *view_array, int views_count);
void       view_array_deinit  (view_array_t *view_array);
view_t    *view_array_find    (view_array_t *view_array, const char *name);

#endif
