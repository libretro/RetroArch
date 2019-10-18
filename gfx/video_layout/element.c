#include <stdlib.h>

#include "element.h"

void element_init(element_t *elem, const char *name, int components_count)
{
   elem->name   = init_string(name);
   elem->state  = -1;
   elem->o_bind = -1;
   elem->i_bind = -1;
   elem->i_mask = -1;
   elem->i_raw  = false;

   elem->bounds = make_bounds();
   elem->render_bounds = make_bounds_unit();

   elem->components = (component_t*)(components_count > 0 ?
         calloc(components_count, sizeof(component_t)) : NULL);
   elem->components_count = components_count;
}

void element_copy(element_t *elem, const element_t *src)
{
   int i;

   elem->name = init_string(src->name);
   elem->state = src->state;

   elem->bounds = src->bounds;
   elem->render_bounds = src->render_bounds;

   elem->components = (component_t*)(src->components_count > 0 ?
         calloc(src->components_count, sizeof(component_t)) : NULL);

   for (i = 0; i < src->components_count; ++i)
      component_copy(&elem->components[i], &src->components[i]);

   elem->components_count = src->components_count;
}

void element_deinit(element_t *elem)
{
   unsigned i;

   for (i = 0; i < elem->components_count; ++i)
      component_deinit(&elem->components[i]);
   free(elem->components);

   free(elem->name);
}

void element_apply_orientation(element_t *elem, video_layout_orientation_t orientation)
{
   unsigned i;

   for (i = 0; i < elem->components_count; ++i)
   {
      component_t *comp = &elem->components[i];
      comp->orientation ^= orientation;

      if (orientation & VIDEO_LAYOUT_SWAP_XY)
      {
         video_layout_bounds_t b;
         b = comp->bounds;

         comp->bounds.x = b.y;
         comp->bounds.y = b.x;
         comp->bounds.w = b.h;
         comp->bounds.h = b.w;
      }

      if (orientation & VIDEO_LAYOUT_FLIP_X)
         comp->bounds.x = 1.0f - comp->bounds.x - comp->bounds.w;

      if (orientation & VIDEO_LAYOUT_FLIP_Y)
         comp->bounds.y = 1.0f - comp->bounds.y - comp->bounds.h;
   }
}
