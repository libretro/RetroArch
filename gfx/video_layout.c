#include <formats/rxml.h>
#include <file/file_path.h>
#include <file/archive_file.h>
#include <compat/strl.h>

#include "video_layout.h"
#include "video_layout/view.h"

#include "../retroarch.h"
#include "../verbosity.h"

bool load(view_array_t *view_array, rxml_document_t *doc);

typedef struct io
{
   char *name;
   int   base_value;
   int   value;
}
io_t;

typedef struct video_layout_state
{
   video_layout_render_info_t render_info;
   const video_layout_render_interface_t *render;

   view_array_t  view_array;

   view_t   *view;
   int       view_index;

   io_t     *io;
   int       io_count;

   void    **images;
   int       images_count;

   char     *base_path;

   bool      is_archive;
   bool      view_changed;
}
video_layout_state_t;

static video_layout_state_t *video_layout_state = NULL;

void video_layout_init(void *video_driver_data, const video_layout_render_interface_t *render)
{
   if (video_layout_state)
      video_layout_deinit();

   video_layout_state = (video_layout_state_t*)calloc(1, sizeof(video_layout_state_t));
   video_layout_state->render_info.video_driver_data = video_driver_data;
   video_layout_state->render = render;

   vec_size((void**)&video_layout_state->images, sizeof(void*), 1);

   video_layout_state->images[0] = NULL;
   video_layout_state->images_count = 1;
}

void video_layout_deinit(void)
{
   int i;

   if (!video_layout_state)
      return;

   free(video_layout_state->base_path);

   for (i = 1; i < video_layout_state->images_count; ++i)
   {
      video_layout_state->render->free_image(
         video_layout_state->render_info.video_driver_data,
         video_layout_state->images[i]
      );
   }

   free(video_layout_state->images);

   for (i = 0; i < video_layout_state->io_count; ++i)
      free(video_layout_state->io[i].name);

   free(video_layout_state->io);

   view_array_deinit(&video_layout_state->view_array);

   free(video_layout_state);
   video_layout_state = NULL;
}

int video_layout_io_assign(const char *name, int base_value)
{
   int index;

   index = video_layout_state->io_count;

   vec_size((void**)&video_layout_state->io, sizeof(io_t), ++video_layout_state->io_count);

   video_layout_state->io[index].name       = init_string(name);
   video_layout_state->io[index].base_value = base_value;
   video_layout_state->io[index].value      = base_value;

   return index;
}

int video_layout_io_find(const char *name)
{
   int i;

   for (i = 0; i < video_layout_state->io_count; ++i)
   {
      if (strcmp(video_layout_state->io[i].name, name) == 0)
         return i;
   }

   return -1;
}

int video_layout_io_get(int index)
{
   return video_layout_state->io[index].value;
}

void video_layout_io_set(int index, int value)
{
   video_layout_state->io[index].value = value;
}

bool video_layout_load(const char *path)
{
   rxml_document_t *doc;
   bool result;

   if(!path || !strlen(path))
      return true;

   video_layout_state->is_archive = path_is_compressed_file(path);

   doc = NULL;

   if(video_layout_state->is_archive)
   {
      void *buf;
      int64_t len;

      char respath[PATH_MAX_LENGTH];
      strlcpy(respath, path, sizeof(respath));
      strlcat(respath, "#", sizeof(respath));
      set_string(&video_layout_state->base_path, respath);

      strlcat(respath, "default.lay", sizeof(respath));
      if (file_archive_compressed_read(respath, &buf, NULL, &len))
      {
         char *str;
         if ((str = (char*)realloc(buf, (size_t)len + 1)))
         {
            str[(size_t)len] = '\0';
            doc = rxml_load_document_string(str);
            free(str);
         }
         else free(buf);
      }
   }
   else
   {
      char respath[PATH_MAX_LENGTH];
      fill_pathname_basedir(respath, path, sizeof(respath));
      set_string(&video_layout_state->base_path, respath);
      doc = rxml_load_document(path);
   }

   if (!doc)
   {
      RARCH_LOG("video_layout: unable to open file \"%s\"\n", path);
      return false;
   }

   result = load(&video_layout_state->view_array, doc);
   rxml_free_document(doc);

   video_layout_view_select(video_layout_view_index());
   return result;
}

bool video_layout_valid(void)
{
   return video_layout_state && video_layout_state->view;
}

static int video_layout_load_image(const char *path)
{
   struct texture_image image;
   void *handle;
   int index;

   image.supports_rgba = video_driver_supports_rgba();

   if (video_layout_state->is_archive)
   {
      void *buf;
      int64_t len;

      char respath[PATH_MAX_LENGTH];
      strlcpy(respath, video_layout_state->base_path, sizeof(respath));
      strlcat(respath, path, sizeof(respath));

      if (!file_archive_compressed_read(respath, &buf, NULL, &len))
      {
         RARCH_LOG("video_layout: failed to decompress image: %s\n", respath);
         return 0;
      }

      if (!image_texture_load_buffer(&image, image_texture_get_type(path), buf, (size_t)len))
      {
         free(buf);

         RARCH_LOG("video_layout: failed to load image: %s\n", respath);
         return 0;
      }

      free(buf);
   }
   else
   {
      char respath[PATH_MAX_LENGTH];
      strlcpy(respath, video_layout_state->base_path, sizeof(respath));
      strlcat(respath, path, sizeof(respath));

      if (!image_texture_load(&image, respath))
      {
         RARCH_LOG("video_layout: failed to load image: %s\n", respath);
         return 0;
      }
   }

   handle = video_layout_state->render->take_image(
      video_layout_state->render_info.video_driver_data, image);

   if (!handle)
      return 0;

   index = video_layout_state->images_count;

   vec_size((void**)&video_layout_state->images, sizeof(void*), ++video_layout_state->images_count);

   video_layout_state->images[index] = handle;

   return index;
}

int video_layout_view_count(void)
{
   return video_layout_state->view_array.views_count;
}

const char *video_layout_view_name(int index)
{
   return video_layout_state->view_array.views[index].name;
}

int video_layout_view_select(int index)
{
   index = MAX(0, MIN(index, video_layout_state->view_array.views_count - 1));

   video_layout_state->view_index = index;
   video_layout_state->view = video_layout_state->view_array.views_count ?
      &video_layout_state->view_array.views[index] : NULL;

   video_layout_view_change();

   return index;
}

int video_layout_view_cycle(void)
{
   return video_layout_view_select(
      (video_layout_state->view_index + 1) % video_layout_state->view_array.views_count);
}

int video_layout_view_index(void)
{
   return video_layout_state->view_index;
}

void video_layout_view_change(void)
{
   video_layout_state->view_changed = true;
}

bool video_layout_view_on_change(void)
{
   if (video_layout_state->view_changed)
   {
      video_layout_state->view_changed = false;
      return true;
   }
   return false;
}

void video_layout_view_fit_bounds(video_layout_bounds_t bounds)
{
   view_t *view;
   float c, dx, dy;
   int i, j, k;

   view = video_layout_state->view;

   c = MIN(bounds.w / view->bounds.w, bounds.h / view->bounds.h);

   dx = view->bounds.w * c;
   dy = view->bounds.h * c;

   view->render_bounds.w = dx;
   view->render_bounds.h = dy;
   view->render_bounds.x = (bounds.w - dx) / 2.f;
   view->render_bounds.y = (bounds.h - dy) / 2.f;

   for (i = 0; i < view->layers_count; ++i)
   {
      layer_t *layer;
      layer = &view->layers[i];

      for (j = 0; j < layer->elements_count; ++j)
      {
         element_t *elem;
         elem = &layer->elements[j];

         elem->render_bounds.x = elem->bounds.x * view->render_bounds.w + view->render_bounds.x;
         elem->render_bounds.y = elem->bounds.y * view->render_bounds.h + view->render_bounds.y;
         elem->render_bounds.w = elem->bounds.w * view->render_bounds.w;
         elem->render_bounds.h = elem->bounds.h * view->render_bounds.h;

         for (k = 0; k < elem->components_count; ++k)
         {
            component_t *comp;
            comp = &elem->components[k];

            comp->render_bounds.x = comp->bounds.x * elem->render_bounds.w + elem->render_bounds.x;
            comp->render_bounds.y = comp->bounds.y * elem->render_bounds.h + elem->render_bounds.y;
            comp->render_bounds.w = comp->bounds.w * elem->render_bounds.w;
            comp->render_bounds.h = comp->bounds.h * elem->render_bounds.h;

            if (comp->type == VIDEO_LAYOUT_C_SCREEN)
               view->screens[comp->attr.screen.index] = comp->render_bounds;
         }
      }
   }
}

int video_layout_layer_count(void)
{
   return video_layout_state->view->layers_count;
}

void video_layout_layer_render(void *video_driver_frame_data, int index)
{
   video_layout_render_info_t *info;
   const video_layout_render_interface_t *r;
   layer_t *layer;
   int i, j;
   
   info  = &video_layout_state->render_info;
   r     =  video_layout_state->render;
   layer = &video_layout_state->view->layers[index];

   info->video_driver_frame_data = video_driver_frame_data;

   r->layer_begin(info);

   for (i = 0; i < layer->elements_count; ++i)
   {
      element_t *elem;
      elem = &layer->elements[i];

      if (elem->o_bind != -1)
         elem->state = video_layout_state->io[elem->o_bind].value;

      for (j = 0; j < elem->components_count; ++j)
      {
         component_t *comp;
         comp = &elem->components[j];

         if (comp->enabled_state != -1)
         {
            if(comp->enabled_state != elem->state)
               continue;
         }

         info->bounds = comp->render_bounds;
         info->orientation = comp->orientation;
         info->color = comp->color;

         switch (comp->type)
         {
         case VIDEO_LAYOUT_C_UNKNOWN:
            break;
         case VIDEO_LAYOUT_C_SCREEN:
            r->screen(info, comp->attr.screen.index);
            break;
         case VIDEO_LAYOUT_C_RECT:
            r->rect(info);
            break;
         case VIDEO_LAYOUT_C_DISK:
            r->ellipse(info);
            break;
         case VIDEO_LAYOUT_C_IMAGE:
            if(!comp->attr.image.loaded)
            {
               comp->attr.image.image_idx = video_layout_load_image(comp->attr.image.file);
               if(comp->attr.image.alpha_file)
                  comp->attr.image.alpha_idx = video_layout_load_image(comp->attr.image.alpha_file);
               comp->attr.image.loaded = true;
            }
            r->image(info,
               video_layout_state->images[comp->attr.image.image_idx],
               video_layout_state->images[comp->attr.image.alpha_idx]);
            break;
         case VIDEO_LAYOUT_C_TEXT:
            r->text(info, comp->attr.text.string);
            break;
         case VIDEO_LAYOUT_C_COUNTER:
            r->counter(info, MIN(elem->state, comp->attr.counter.max_state));
            break;
         case VIDEO_LAYOUT_C_DOTMATRIX_X1:
            r->led_dot(info, 1, elem->state);
            break;
         case VIDEO_LAYOUT_C_DOTMATRIX_H5:
            r->led_dot(info, 5, elem->state);
            break;
         case VIDEO_LAYOUT_C_DOTMATRIX_H8:
            r->led_dot(info, 8, elem->state);
            break;
         case VIDEO_LAYOUT_C_LED_7:
            r->led_seg(info, VIDEO_LAYOUT_LED_7, elem->state);
            break;
         case VIDEO_LAYOUT_C_LED_8_GTS1:
            r->led_seg(info, VIDEO_LAYOUT_LED_8_GTS1, elem->state);
            break;
         case VIDEO_LAYOUT_C_LED_14:
            r->led_seg(info, VIDEO_LAYOUT_LED_14, elem->state);
            break;
         case VIDEO_LAYOUT_C_LED_14_SC:
            r->led_seg(info, VIDEO_LAYOUT_LED_14_SC, elem->state);
            break;
         case VIDEO_LAYOUT_C_LED_16:
            r->led_seg(info, VIDEO_LAYOUT_LED_16, elem->state);
            break;
         case VIDEO_LAYOUT_C_LED_16_SC:
            r->led_seg(info, VIDEO_LAYOUT_LED_16_SC, elem->state);
            break;
         case VIDEO_LAYOUT_C_REEL:
            /* not implemented */
            break;
         }
      }
   }

   r->layer_end(info, layer->blend);
}

const video_layout_bounds_t *video_layout_screen(int index)
{
   return &video_layout_state->view->screens[index];
}

int video_layout_screen_count(void)
{
   return video_layout_state->view->screens_count;
}
