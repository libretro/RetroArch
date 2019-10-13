#include <stdlib.h>
#include <string.h>
#include "component.h"

void component_init(component_t *comp, comp_type_t type)
{
   comp->type = type;
   comp->bounds = make_bounds();
   comp->render_bounds = make_bounds_unit();
   comp->orientation = VIDEO_LAYOUT_ROT0;
   comp->color = make_color_white();
   comp->enabled_state = -1;

   switch (comp->type)
   {
      case VIDEO_LAYOUT_C_UNKNOWN:
         break;
      case VIDEO_LAYOUT_C_SCREEN:
         comp->attr.screen.index = 0;
         break;
      case VIDEO_LAYOUT_C_RECT:
         break;
      case VIDEO_LAYOUT_C_DISK:
         break;
      case VIDEO_LAYOUT_C_IMAGE:
         comp->attr.image.file = NULL;
         comp->attr.image.alpha_file = NULL;
         comp->attr.image.image_idx = 0;
         comp->attr.image.alpha_idx = 0;
         comp->attr.image.loaded = false;
         break;
      case VIDEO_LAYOUT_C_TEXT:
         comp->attr.text.string = NULL;
         comp->attr.text.align = VIDEO_LAYOUT_TEXT_ALIGN_CENTER;
         break;
      case VIDEO_LAYOUT_C_COUNTER:
         comp->attr.counter.digits = 2;
         comp->attr.counter.max_state = 999;
         comp->attr.counter.align = VIDEO_LAYOUT_TEXT_ALIGN_CENTER;
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_X1:
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_H5:
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_H8:
         break;
      case VIDEO_LAYOUT_C_LED_7:
         break;
      case VIDEO_LAYOUT_C_LED_8_GTS1:
         break;
      case VIDEO_LAYOUT_C_LED_14:
         break;
      case VIDEO_LAYOUT_C_LED_14_SC:
         break;
      case VIDEO_LAYOUT_C_LED_16:
         break;
      case VIDEO_LAYOUT_C_LED_16_SC:
         break;
      case VIDEO_LAYOUT_C_REEL:
         break;
   }
}

void component_copy(component_t *comp, const component_t *src)
{
   comp->type = src->type;
   comp->bounds = src->bounds;
   comp->render_bounds = src->render_bounds;
   comp->orientation = src->orientation;
   comp->color = src->color;
   comp->enabled_state = src->enabled_state;

   switch (comp->type)
   {
      case VIDEO_LAYOUT_C_UNKNOWN:
         break;
      case VIDEO_LAYOUT_C_SCREEN:
         comp->attr.screen.index = src->attr.screen.index;
         break;
      case VIDEO_LAYOUT_C_RECT:
         break;
      case VIDEO_LAYOUT_C_DISK:
         break;
      case VIDEO_LAYOUT_C_IMAGE:
         comp->attr.image.file = init_string(src->attr.image.file);
         comp->attr.image.alpha_file = init_string(src->attr.image.alpha_file);
         comp->attr.image.image_idx = src->attr.image.image_idx;
         comp->attr.image.alpha_idx = src->attr.image.alpha_idx;
         comp->attr.image.loaded = src->attr.image.loaded;
         break;
      case VIDEO_LAYOUT_C_TEXT:
         comp->attr.text.string = init_string(src->attr.text.string);
         comp->attr.text.align = src->attr.text.align;
         break;
      case VIDEO_LAYOUT_C_COUNTER:
         comp->attr.counter.digits = src->attr.counter.digits;
         comp->attr.counter.max_state = src->attr.counter.max_state;
         comp->attr.counter.align = src->attr.counter.align;
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_X1:
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_H5:
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_H8:
         break;
      case VIDEO_LAYOUT_C_LED_7:
         break;
      case VIDEO_LAYOUT_C_LED_8_GTS1:
         break;
      case VIDEO_LAYOUT_C_LED_14:
         break;
      case VIDEO_LAYOUT_C_LED_14_SC:
         break;
      case VIDEO_LAYOUT_C_LED_16:
         break;
      case VIDEO_LAYOUT_C_LED_16_SC:
         break;
      case VIDEO_LAYOUT_C_REEL:
         break;
   }
}

void component_deinit(component_t *comp)
{
   switch (comp->type)
   {
      case VIDEO_LAYOUT_C_UNKNOWN:
         break;
      case VIDEO_LAYOUT_C_SCREEN:
         break;
      case VIDEO_LAYOUT_C_RECT:
         break;
      case VIDEO_LAYOUT_C_DISK:
         break;
      case VIDEO_LAYOUT_C_IMAGE:
         free(comp->attr.image.file);
         free(comp->attr.image.alpha_file);
         break;
      case VIDEO_LAYOUT_C_TEXT:
         free(comp->attr.text.string);
         break;
      case VIDEO_LAYOUT_C_COUNTER:
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_X1:
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_H5:
         break;
      case VIDEO_LAYOUT_C_DOTMATRIX_H8:
         break;
      case VIDEO_LAYOUT_C_LED_7:
         break;
      case VIDEO_LAYOUT_C_LED_8_GTS1:
         break;
      case VIDEO_LAYOUT_C_LED_14:
         break;
      case VIDEO_LAYOUT_C_LED_14_SC:
         break;
      case VIDEO_LAYOUT_C_LED_16:
         break;
      case VIDEO_LAYOUT_C_LED_16_SC:
         break;
      case VIDEO_LAYOUT_C_REEL:
         break;
   }
}
