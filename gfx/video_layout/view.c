#include <stdlib.h>
#include <string.h>

#include "view.h"

void layer_init(layer_t *layer, const char *name)
{
   layer->name = init_string(name);
   layer->blend = VIDEO_LAYOUT_BLEND_ALPHA;
   layer->elements = NULL;
   layer->elements_count = 0;
}

void layer_deinit(layer_t *layer)
{
   int i;

   for (i = 0; i < layer->elements_count; ++i)
      element_deinit(&layer->elements[i]);

   free(layer->elements);
   free(layer->name);
}

element_t *layer_add_element(layer_t *layer)
{
   element_t *elem;

   vec_size((void**)&layer->elements,
         sizeof(element_t), ++layer->elements_count);

   elem = &layer->elements[layer->elements_count - 1];
   element_init(elem, NULL, 0);

   return elem;
}

void view_init(view_t *view, const char *name)
{
   view->name          = init_string(name);
   view->bounds        = make_bounds();
   view->render_bounds = make_bounds_unit();
   view->layers        = NULL;
   view->layers_count  = 0;
   view->screens       = NULL;
   view->screens_count = 0;
}

void view_deinit(view_t *view)
{
   int i;

   free(view->screens);

   for (i = 0; i < view->layers_count; ++i)
      layer_deinit(&view->layers[i]);

   free(view->layers);
   free(view->name);
}

layer_t *view_find_layer(view_t *view, const char *name)
{
   int i;

   for (i = 0; i < view->layers_count; ++i)
   {
      if (strcmp(name, view->layers[i].name) == 0)
         return &view->layers[i];
   }

   return NULL;
}

layer_t *view_emplace_layer(view_t *view, const char *name)
{
   layer_t *layer = view_find_layer(view, name);

   if (!layer)
   {
      vec_size((void**)&view->layers, sizeof(layer_t), ++view->layers_count);

      layer = &view->layers[view->layers_count - 1];
      layer_init(layer, name);
   }

   return layer;
}

void view_sort_layers(view_t *view)
{
   layer_t sorted[6];
   layer_t *layer;
   int i = 0;

   /* retroarch frame *= screen's color */
   if ((layer = view_find_layer(view, "screen")))
   {
      layer->blend = VIDEO_LAYOUT_BLEND_MOD;
      sorted[i] = *layer;
      ++i;
   }

   if ((layer = view_find_layer(view, "overlay")))
   {
      layer->blend = VIDEO_LAYOUT_BLEND_MOD;
      sorted[i] = *layer;
      ++i;
   }

   if ((layer = view_find_layer(view, "backdrop")))
   {
      layer->blend = VIDEO_LAYOUT_BLEND_ADD;
      sorted[i] = *layer;
      ++i;
   }

   if ((layer = view_find_layer(view, "bezel")))
   {
      layer->blend = VIDEO_LAYOUT_BLEND_ALPHA;
      sorted[i] = *layer;
      ++i;
   }

   if ((layer = view_find_layer(view, "cpanel")))
   {
      layer->blend = VIDEO_LAYOUT_BLEND_ALPHA;
      sorted[i] = *layer;
      ++i;
   }

   if ((layer = view_find_layer(view, "marquee")))
   {
      layer->blend = VIDEO_LAYOUT_BLEND_ALPHA;
      sorted[i] = *layer;
      ++i;
   }

   for (i = 0; i < view->layers_count; ++i)
      view->layers[i] = sorted[i];
}

void view_normalize(view_t *view)
{
   video_layout_bounds_t dim;
   int i, j;

   if (bounds_valid(&view->bounds))
   {
      dim.x = view->bounds.x / view->bounds.w;
      dim.y = view->bounds.y / view->bounds.h;
      dim.w = 1.0f / view->bounds.w;
      dim.h = 1.0f / view->bounds.h;

      if (view->bounds.w < view->bounds.h)
      {
         view->bounds.w = view->bounds.w / view->bounds.h;
         view->bounds.h = 1.f;
      }
      else
      {
         view->bounds.h = view->bounds.h / view->bounds.w;
         view->bounds.w = 1.f;
      }

      view->bounds.x = 0;
      view->bounds.y = 0;
   }
   else
   {
      dim = view->bounds = make_bounds_unit();
   }

   for (i = 0; i < view->layers_count; ++i)
   {
      layer_t *layer;
      layer = &view->layers[i];

      for (j = 0; j < layer->elements_count; ++j)
      {
         element_t *elem;
         elem = &layer->elements[j];

         if (bounds_valid(&elem->bounds))
         {
            bounds_scale(&elem->bounds, &dim);
         }
         else
         {
            elem->bounds = dim;
         }

         elem->bounds.x -= dim.x;
         elem->bounds.y -= dim.y;
      }
   }
}

void view_count_screens(view_t *view)
{
   int i, j, k;
   int idx = -1;

   for (i = 0; i < view->layers_count; ++i)
   {
      layer_t *layer = &view->layers[i];
      for (j = 0; j < layer->elements_count; ++j)
      {
         element_t *elem = &layer->elements[j];
         for (k = 0; k < elem->components_count; ++k)
         {
            component_t *comp = &elem->components[k];
            if (comp->type == VIDEO_LAYOUT_C_SCREEN)
               idx = MAX(idx, comp->attr.screen.index);
         }
      }
   }

   if (view->screens_count)
   {
      free(view->screens);
      view->screens_count = 0;
   }

   if ((++idx))
   {
      view->screens = (video_layout_bounds_t*)calloc(idx, sizeof(video_layout_bounds_t));
      view->screens_count = idx;
   }
}

void view_array_init(view_array_t *view_array, int views_count)
{
   view_array->views = (view_t*)(views_count > 0 ?
      calloc(views_count, sizeof(view_t)) : NULL);
   view_array->views_count = views_count;
}

void view_array_deinit(view_array_t *view_array)
{
   int i;

   for (i = 0; i < view_array->views_count; ++i)
      view_deinit(&view_array->views[i]);
   free(view_array->views);
   view_array->views = NULL;
   view_array->views_count = 0;
}

view_t *view_array_find(view_array_t *view_array, const char *name)
{
   int i;

   for (i = 0; i < view_array->views_count; ++i)
   {
      if (strcmp(name, view_array->views[i].name) == 0)
         return &view_array->views[i];
   }
   return NULL;
}
