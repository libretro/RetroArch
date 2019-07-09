#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <lists/string_list.h>

#include "../core.h"
#include "mem_util.h"
#include "copy_load_info.h"

retro_ctx_load_content_info_t *load_content_info;
enum rarch_core_type last_core_type;

static struct retro_game_info* clone_retro_game_info(const
      struct retro_game_info *src)
{
   void *data                   = NULL;
   struct retro_game_info *dest = NULL;

   if (!src)
      return NULL;

   dest       = (struct retro_game_info*)calloc(1,
         sizeof(struct retro_game_info));
   if (!dest)
      return NULL;

   dest->data    = NULL;
   dest->path    = strcpy_alloc(src->path);

   if (src->size && src->data)
   {
      data = malloc(src->size);

      if (data)
      {
         memcpy(data, src->data, src->size);
         dest->data = data;
      }
   }

   dest->size    = src->size;
   dest->meta    = strcpy_alloc(src->meta);

   return dest;
}

static void free_retro_ctx_load_content_info(struct
      retro_ctx_load_content_info *dest)
{
   if (!dest)
      return;

   core_free_retro_game_info(dest->info);
   string_list_free((struct string_list*)dest->content);
   if (dest->info)
      free(dest->info);

   dest->info    = NULL;
   dest->content = NULL;
}

static struct retro_ctx_load_content_info
*clone_retro_ctx_load_content_info(
      const struct retro_ctx_load_content_info *src)
{
   struct retro_ctx_load_content_info *dest = NULL;
   if (!src || src->special != NULL)
      return NULL;   /* refuse to deal with the Special field */

   dest          = (struct retro_ctx_load_content_info*)
      calloc(1, sizeof(*dest));

   if (!dest)
      return NULL;

   dest->info       = clone_retro_game_info(src->info);
   dest->content    = NULL;
   dest->special    = NULL;

   if (src->content)
      dest->content = string_list_clone(src->content);
   return dest;
}

void set_load_content_info(const retro_ctx_load_content_info_t *ctx)
{
   free_retro_ctx_load_content_info(load_content_info);
   free(load_content_info);
   load_content_info = clone_retro_ctx_load_content_info(ctx);
}

void set_last_core_type(enum rarch_core_type type)
{
   last_core_type = type;
}
