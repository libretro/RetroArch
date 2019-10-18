#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <formats/rxml.h>

#include "../../verbosity.h"

#include "internal.h"
#include "view.h"
#include "scope.h"

int video_layout_io_find(const char *name);

static const char *const comp_type_str[] = {
   NULL, /* VIDEO_LAYOUT_C_UNKNOWN */
   NULL, /* VIDEO_LAYOUT_C_SCREEN */
   "rect",
   "disk",
   "image",
   "text",
   "dotmatrixdot",
   "dotmatrix5dot",
   "dotmatrix",
   "led7seg",
   "led8seg_gts1",
   "led14seg",
   "led14segsc",
   "led16seg",
   "led16segsc",
   "simplecounter",
   "reel"
};

static const char *const video_layout_internal_device_params[] =
{
   "devicetag"            , ":",
   "devicebasetag"        , "root",
   "devicename"           , "RetroArch",
   "deviceshortname"      , "libretro"
};

static const char *const video_layout_internal_screen_params[] =
{
   "scr#physicalxaspect"  , "1",
   "scr#physicalyaspect"  , "1",
   "scr#nativexaspect"    , "1",
   "scr#nativeyaspect"    , "1",
   "scr#width"            , "1",
   "scr#height"           , "1"
};

static int child_count(rxml_node_t *node)
{
   rxml_node_t *child;
   int res = 0;

   for (child = node->children; child; child = child->next)
      ++res;

   return res;
}

static comp_type_t comp_type_from_str(const char *s)
{
   size_t i;

   for (i = 2; i < ARRAY_SIZE(comp_type_str); ++i)
   {
      if (strcmp(s, comp_type_str[i]) == 0)
         return (comp_type_t)(int)i;
   }

   return VIDEO_LAYOUT_C_UNKNOWN;
}

static void init_device_params(scope_t *scope)
{
   size_t i;

   for (i = 0; i < ARRAY_SIZE(video_layout_internal_device_params); i += 2)
   {
      scope_param(scope, video_layout_internal_device_params[i], video_layout_internal_device_params[i + 1]);
   }
}

static void init_screen_params(scope_t *scope, int screen_index)
{
   char buf[64];
   size_t i;

   for (i = 0; i < ARRAY_SIZE(video_layout_internal_screen_params); i += 2)
   {
      strcpy(buf, video_layout_internal_screen_params[i + 1]);
      buf[3] = '0' + screen_index;

      scope_param(scope, video_layout_internal_screen_params[i], buf);
   }
}

static video_layout_bounds_t parse_bounds(scope_t *scope, rxml_node_t *node)
{
   const char *prop;
   video_layout_bounds_t bounds = make_bounds_unit();

   if ((prop = scope_eval(scope, rxml_node_attrib(node, "x"))))       bounds.x = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "y"))))       bounds.y = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "width"))))   bounds.w = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "height"))))  bounds.h = get_dec(prop);

   if ((prop = scope_eval(scope, rxml_node_attrib(node, "left"))))    bounds.x = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "top"))))     bounds.y = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "right"))))   bounds.w = get_dec(prop) - bounds.x;
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "bottom"))))  bounds.h = get_dec(prop) - bounds.y;

   return bounds;
}

static video_layout_color_t parse_color(scope_t *scope, rxml_node_t *node)
{
   const char *prop;
   video_layout_color_t color = make_color_white();

   if ((prop = scope_eval(scope, rxml_node_attrib(node, "red"))))    color.r = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "green"))))  color.g = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "blue"))))   color.b = get_dec(prop);
   if ((prop = scope_eval(scope, rxml_node_attrib(node, "alpha"))))  color.a = get_dec(prop);

   return color;
}

static video_layout_orientation_t parse_orientation(scope_t *scope, rxml_node_t *node)
{
   const char *prop;
   video_layout_orientation_t result = VIDEO_LAYOUT_ROT0;

   if ((prop = scope_eval(scope, rxml_node_attrib(node, "rotate"))))
   {
      if (strcmp(prop, "90") == 0)
         result = VIDEO_LAYOUT_ROT90;

      else if (strcmp(prop, "180") == 0)
         result = VIDEO_LAYOUT_ROT180;

      else if (strcmp(prop, "270") == 0)
         result = VIDEO_LAYOUT_ROT270;
   }

   if ((prop = scope_eval(scope, rxml_node_attrib(node, "swapxy"))))
   {
      if (strcmp(prop, "no") != 0)
         result ^= VIDEO_LAYOUT_SWAP_XY;
   }

   if ((prop = scope_eval(scope, rxml_node_attrib(node, "flipx"))))
   {
      if (strcmp(prop, "no") != 0)
         result ^= VIDEO_LAYOUT_FLIP_X;
   }

   if ((prop = scope_eval(scope, rxml_node_attrib(node, "flipy"))))
   {
      if (strcmp(prop, "no") != 0)
         result ^= VIDEO_LAYOUT_FLIP_Y;
   }

   return result;
}

static bool load_param(scope_t *scope, rxml_node_t *node, bool can_repeat)
{
   const char *name;
   const char *value;
   const char *start;

   if (!(name = rxml_node_attrib(node, "name")))
   {
      RARCH_LOG("video_layout: <param> is missing 'name' attribute\n");
      return false;
   }

   value = rxml_node_attrib(node, "value");
   start = rxml_node_attrib(node, "start");

   if (can_repeat && start)
   {
      const char *inc = rxml_node_attrib(node, "increment");
      const char *ls  = rxml_node_attrib(node, "lshift");
      const char *rs  = rxml_node_attrib(node, "rshift");

      if (inc || ls || rs)
      {
         scope_generator(scope, name, start, inc, ls, rs);
      }
      else
      {
         RARCH_LOG("video_layout: invalid generator <param name=\"%s\" /> missing increment/shift\n",
            scope_eval(scope, name));
         return false;
      }
   }
   else if (name && value)
   {
      scope_param(scope, name, value);
   }
   else
   {
      RARCH_LOG("video_layout: invalid parameter <param name=\"%s\" /> missing value\n",
         scope_eval(scope, name));
      return false;
   }

   return true;
}

static bool load_component(scope_t *scope, component_t *comp, rxml_node_t *node)
{
   const char *state;
   const char *attr;
   rxml_node_t *n;
   comp_type_t type = comp_type_from_str(node->name);
   bool      result = true;

   if (type == VIDEO_LAYOUT_C_UNKNOWN)
   {
      RARCH_LOG("video_layout: invalid component <%s />\n", node->name);
      return false;
   }

   component_init(comp, type);

   if ((state = rxml_node_attrib(node, "state")))
      comp->enabled_state = get_int(scope_eval(scope, state));

   for (n = node->children; n; n = n->next)
   {
      if (strcmp(n->name, "bounds") == 0)
         comp->bounds = parse_bounds(scope, n);

      else if (strcmp(n->name, "color") == 0)
         comp->color = parse_color(scope, n);
   }

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
         {
            if (!(attr = rxml_node_attrib(node, "file")))
            {
               RARCH_LOG("video_layout: invalid component <%s />, missing 'file' attribute\n", node->name);
               result = false;
            }
            set_string(&comp->attr.image.file, scope_eval(scope, attr));

            if ((attr = rxml_node_attrib(node, "alphafile")))
               set_string(&comp->attr.image.alpha_file, scope_eval(scope, attr));
         }
         break;
      case VIDEO_LAYOUT_C_TEXT:
         {
            if (!(attr = rxml_node_attrib(node, "string")))
            {
               RARCH_LOG("video_layout: invalid component <%s />, missing 'string' attribute\n", node->name);
               result = false;
            }
            set_string(&comp->attr.text.string, scope_eval(scope, attr));

            if ((attr = rxml_node_attrib(node, "align")))
               comp->attr.text.align = (video_layout_text_align_t)get_int(scope_eval(scope, attr));
         }
         break;
      case VIDEO_LAYOUT_C_COUNTER:
         {
            if ((attr = rxml_node_attrib(node, "digits")))
               comp->attr.counter.digits = get_int(scope_eval(scope, attr));

            if ((attr = rxml_node_attrib(node, "maxstate")))
               comp->attr.counter.max_state = get_int(scope_eval(scope, attr));

            if ((attr = rxml_node_attrib(node, "align")))
               comp->attr.counter.align = (video_layout_text_align_t)get_int(scope_eval(scope, attr));
         }
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

   return result;
}

static bool load_element(scope_t *scope, rxml_node_t *node)
{
   const char *name;
   const char *state;
   int i;
   element_t *elem;
   rxml_node_t *n;
   video_layout_bounds_t dim;
   bool result = true;

   if (!(name = rxml_node_attrib(node, "name")))
   {
      RARCH_LOG("video_layout: <element> is missing 'name' attribute\n");
      return false;
   }

   elem = scope_add_element(scope);
   element_init(elem, scope_eval(scope, name), child_count(node));

   if ((state = rxml_node_attrib(node, "defstate")))
      elem->state = get_int(scope_eval(scope, state));

   i = 0;
   for (n = node->children; n; n = n->next, ++i)
   {
      component_t *comp;
      comp = &elem->components[i];

      if (load_component(scope, comp, n))
         elem->bounds = bounds_union(&elem->bounds, &comp->bounds);
      else
         result = false;
   }

   if (bounds_valid(&elem->bounds))
   {
      dim.x = elem->bounds.x / elem->bounds.w;
      dim.y = elem->bounds.y / elem->bounds.h;
      dim.w = 1.0f / elem->bounds.w;
      dim.h = 1.0f / elem->bounds.h;
   }
   else
   {
      dim = make_bounds_unit();
   }

   for (i = 0; i < elem->components_count; ++i)
   {
      component_t *comp;
      comp = &elem->components[i];

      if (bounds_valid(&comp->bounds))
         bounds_scale(&comp->bounds, &dim);
      else
         comp->bounds = dim;

      comp->bounds.x -= dim.x;
      comp->bounds.y -= dim.y;
   }

   elem->bounds = make_bounds_unit();

   return result;
}

static bool load_screen(scope_t *scope, element_t *elem, rxml_node_t *node)
{
   component_t *comp;
   const char *index = rxml_node_attrib(node, "index");

   element_init(elem, NULL, 1);
   comp = &elem->components[0];

   component_init(comp, VIDEO_LAYOUT_C_SCREEN);
   comp->bounds = make_bounds_unit();
   comp->attr.screen.index = get_int(scope_eval(scope, index));

   return true;
}

static void merge_group(scope_t *scope, view_t *view, view_t *group,
   bool has_bounds, video_layout_bounds_t n_bounds, video_layout_orientation_t n_orient, video_layout_color_t n_color)
{
   int i, j, k;
   bool constrain = bounds_valid(&n_bounds);

   for (i = 0; i < group->layers_count; ++i)
   {
      layer_t *group_layer;
      layer_t *layer;

      group_layer = &group->layers[i];
      layer = view_emplace_layer(view, group_layer->name);

      for (j = 0; j < group_layer->elements_count; ++j)
      {
         element_t *elem;
         elem = layer_add_element(layer);

         element_copy(elem, &group_layer->elements[j]);

         for (k = 0; k < elem->components_count; ++k)
            color_mod(&elem->components->color, &n_color);

         if (n_orient)
            element_apply_orientation(elem, n_orient);

         if (constrain)
         {
            bounds_scale(&elem->bounds, &n_bounds);
            elem->bounds.x += n_bounds.x;
            elem->bounds.y += n_bounds.y;
         }

         if (!has_bounds)
            view->bounds = bounds_union(&view->bounds, &elem->bounds);
      }
   }
}

static bool load_view(scope_t *scope, view_t *view, rxml_node_t *node, bool is_named)
{
   bool result, has_bounds;
   rxml_node_t *n;
   rxml_node_t *o;
   int i;

   if (is_named)
   {
      const char *name;

      if (!(name = rxml_node_attrib(node, "name")))
      {
         RARCH_LOG("video_layout: <view> is missing 'name' attribute\n");
         return false;
      }

      view_init(view, scope_eval(scope, name));
   }

   result     = true;
   has_bounds = false;

   for (n = node->children; n; n = n->next)
   {
      video_layout_color_t       n_color;
      video_layout_bounds_t      n_bounds;
      video_layout_orientation_t n_orient;

      if (strcmp(n->name, "param") == 0)
      {
         if (!load_param(scope, n, true))
            result = false;
         continue;
      }

      else if (strcmp(n->name, "bounds") == 0)
      {
         view->bounds = parse_bounds(scope, n);
         has_bounds = true;
         continue;
      }

      n_color  = make_color_white();
      n_bounds = make_bounds();
      n_orient = VIDEO_LAYOUT_ROT0;

      for (o = n->children; o; o = o->next)
      {
         if (strcmp(o->name, "color") == 0)
            n_color = parse_color(scope, o);

         else if (strcmp(o->name, "bounds") == 0)
            n_bounds = parse_bounds(scope, o);

         else if (strcmp(o->name, "orientation") == 0)
            n_orient = parse_orientation(scope, o);
      }

      if (strcmp(n->name, "group") == 0)
      {
         const char *ref;
         if ((ref = rxml_node_attrib(n, "ref")))
         {
            view_t *group;
            if ((group = scope_find_group(scope, scope_eval(scope, ref))))
            {
               merge_group(scope, view, group, has_bounds, n_bounds, n_orient, n_color);
            }
            else
            {
               RARCH_LOG("video_layout: group \"%s\" is missing\n", scope_eval(scope, ref));
               result = false;
            }
         }
         else
         {
            RARCH_LOG("video_layout: <group> is missing 'ref' attribute\n");
            result = false;
         }
      }

      else if (strcmp(n->name, "repeat") == 0)
      {
         const char *count_s;
         int count;

         if (!(count_s = rxml_node_attrib(n, "count")))
         {
            RARCH_LOG("video_layout: <repeat> is missing 'count' attribute\n");
            result = false;
            continue;
         }

         count = get_int(scope_eval(scope, count_s));

         scope_push(scope);

         for (o = n->children; o; o = o->next)
         {
            if (strcmp(o->name, "param") == 0)
            {
               if (!load_param(scope, o, true))
                  result = false;
            }
         }

         for (i = 0; i < count; ++i)
         {
            view_t rep;
            view_init(&rep, NULL);

            if (!load_view(scope, &rep, n, false))
               result = false;

            merge_group(scope, view, &rep, has_bounds, n_bounds, n_orient, n_color);

            view_deinit(&rep);

            scope_repeat(scope);
         }

         scope_pop(scope);
      }

      else /* element */
      {
         layer_t *layer;
         element_t *elem;

         layer = view_emplace_layer(view, n->name);
         elem = layer_add_element(layer);

         if (strcmp(n->name, "screen") == 0)
         {
            if (!load_screen(scope, elem, n))
               result = false;
         }
         else
         {
            const char *elem_name;
            const char *attr;

            if ((elem_name = rxml_node_attrib(n, "element")))
            {
               element_t *elem_src;
               if ((elem_src = scope_find_element(scope, elem_name)))
               {
                  element_copy(elem, elem_src);

                  if ((attr = rxml_node_attrib(n, "name")))
                     elem->o_bind = video_layout_io_find(scope_eval(scope, attr));

                  if ((attr = rxml_node_attrib(n, "inputtag")))
                     elem->i_bind = video_layout_io_find(scope_eval(scope, attr));

                  if ((attr = rxml_node_attrib(n, "inputmask")))
                     elem->i_mask = get_int(scope_eval(scope, attr));

                  if ((attr = rxml_node_attrib(n, "inputraw")))
                     elem->i_raw = get_int(scope_eval(scope, attr)) ? true : false;
               }
               else
               {
                  RARCH_LOG("video_layout: element \"%s\" is missing\n", scope_eval(scope, elem_name));
                  result = false;
               }
            }
            else
            {
               RARCH_LOG("video_layout: <%s> is missing 'element' attribute\n", n->name);
               result = false;
            }
         }

         for (i = 0; i < elem->components_count; ++i)
            color_mod(&elem->components->color, &n_color);

         elem->bounds = n_bounds;

         if (n_orient)
            element_apply_orientation(elem, n_orient);

         if (!has_bounds)
            view->bounds = bounds_union(&view->bounds, &elem->bounds);
      }
   }

   return result;
}

static bool load_group(scope_t *scope, rxml_node_t *node)
{
   bool result = true;

   view_t *group = scope_add_group(scope);

   scope_push(scope);

   if (!load_view(scope, group, node, true))
      result = false;

   scope_pop(scope);

   return result;
}

static bool load_top_level(scope_t *scope, int *view_count, rxml_node_t *root)
{
   rxml_node_t *node;
   bool result = true;
   *view_count = 0;

   for (node = root->children; node; node = node->next)
   {
      if (strcmp(node->name, "param") == 0)
      {
         if (!load_param(scope, node, false))
            result = false;
      }

      else if (strcmp(node->name, "element") == 0)
      {
         if (!load_element(scope, node))
            result = false;
      }

      else if (strcmp(node->name, "group") == 0)
      {
         if (!load_group(scope, node))
            result = false;
      }

      else if (strcmp(node->name, "view") == 0)
         ++(*view_count);
   }

   return result;
}

static bool load_views(scope_t *scope, view_array_t *view_array, rxml_node_t *root)
{
   rxml_node_t *node;
   bool result = true;
   int i = 0;

   for (node = root->children; node; node = node->next)
   {
      if (strcmp(node->name, "view") == 0)
      {
         view_t *view;
         view = &view_array->views[i];

         scope_push(scope);

         if (!load_view(scope, view, node, true))
            result = false;

         view_sort_layers(view);
         view_normalize(view);
         view_count_screens(view);

         scope_pop(scope);

         ++i;
      }
   }

   return result;
}

bool load(view_array_t *view_array, rxml_document_t *doc)
{
   bool result;
   scope_t scope;
   int view_count;
   rxml_node_t *root = rxml_root_node(doc);

   if (strcmp(root->name, "mamelayout") ||
         strcmp(rxml_node_attrib(root, "version"), "2"))
   {
      RARCH_LOG("video_layout: invalid MAME Layout file\n");
      return false;
   }

   result = false;

   scope_init(&scope);
   init_device_params(&scope);
   init_screen_params(&scope, 0);
   init_screen_params(&scope, 1);

   if (!load_top_level(&scope, &view_count, root))
      result = false;

   view_array_init(view_array, view_count);

   if (!load_views(&scope, view_array, root))
      result = false;

   scope_deinit(&scope);

   return result;
}
