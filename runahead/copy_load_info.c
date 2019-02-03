#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <lists/string_list.h>

#include "../core.h"
#include "mem_util.h"
#include "copy_load_info.h"

retro_ctx_load_content_info_t *load_content_info;
enum rarch_core_type last_core_type;

static void free_retro_game_info(struct retro_game_info *dest)
{
   if (!dest)
      return;
   if (dest->path)
      free((void*)dest->path);
   if (dest->data)
      free((void*)dest->data);
   if (dest->meta)
      free((void*)dest->meta);
   dest->path = NULL;
   dest->data = NULL;
   dest->meta = NULL;
}

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

static void free_string_list(struct string_list *dest)
{
   unsigned i;
   if (!dest)
      return;
   for (i = 0; i < dest->size; i++)
   {
      if (dest->elems[i].data)
         free(dest->elems[i].data);
      dest->elems[i].data = NULL;
   }

   if (dest->elems)
      free(dest->elems);
   dest->elems = NULL;
}

static struct string_list *string_list_clone(
      const struct string_list *src)
{
   unsigned i;
   struct string_list_elem *elems = NULL;
   struct string_list *dest       = (struct string_list*)
      calloc(1, sizeof(struct string_list));

   if (!dest)
      return NULL;

   dest->size      = src->size;
   dest->cap       = src->cap;
   if (dest->cap < dest->size)
   {
      dest->cap = dest->size;
   }

   elems           = (struct string_list_elem*)
      calloc(dest->cap, sizeof(struct string_list_elem));

   if (!elems)
   {
      free(dest);
      return NULL;
   }

   dest->elems     = elems;

   for (i = 0; i < src->size; i++)
   {
      dest->elems[i].data = strcpy_alloc(src->elems[i].data);
      dest->elems[i].attr = src->elems[i].attr;
   }

   return dest;
}

#if 0
/* for cloning the Special field, however, attempting
 * to use this feature crashes retroarch */
static void free_retro_subsystem_memory_info(struct
      retro_subsystem_memory_info *dest)
{
   if (!dest)
      return;
   if (dest->extension)
      free(dest->extension);
   dest->extension = NULL;
}

static void clone_retro_subsystem_memory_info(struct
      retro_subsystem_memory_info* dest,
      const struct retro_subsystem_memory_info *src)
{
   dest->extension = strcpy_alloc(src->extension);
   dest->type      = src->type;
}

static void free_retro_subsystem_rom_info(struct
      retro_subsystem_rom_info *dest)
{
   int i;
   if (!dest)
      return;

   if (dest->desc)
      free(dest->desc);
   if (dest->valid_extensions)
      free(dest->valid_extensions);
   dest->desc             = NULL;
   dest->valid_extensions = NULL;

   for (i = 0; i < dest->num_memory; i++)
      free_retro_subsystem_memory_info((struct
               retro_subsystem_memory_info*)&dest->memory[i]);

   if (dest->memory)
      free(dest->memory);
   dest->memory = NULL;
}

static void clone_retro_subsystem_rom_info(struct
      retro_subsystem_rom_info *dest,
      const struct retro_subsystem_rom_info *src)
{
   int i;
   retro_subsystem_memory_info *memory = NULL;

   dest->need_fullpath    = src->need_fullpath;
   dest->block_extract    = src->block_extract;
   dest->required         = src->required;
   dest->num_memory       = src->num_memory;
   dest->desc             = strcpy_alloc(src->desc);
   dest->valid_extensions = strcpy_alloc(src->valid_extensions);

   memory                 = (struct retro_subsystem_memory_info*)calloc(1,
         dest->num_memory * sizeof(struct retro_subsystem_memory_info));

   dest->memory           = memory;

   for (i = 0; i < dest->num_memory; i++)
      clone_retro_subsystem_memory_info(&memory[i], &src->memory[i]);
}

static void free_retro_subsystem_info(struct retro_subsystem_info *dest)
{
   int i;

   if (!dest)
      return;

   if (dest->desc)
      free(dest->desc);
   if (dest->ident)
      free(dest->ident);
   dest->desc  = NULL;
   dest->ident = NULL;

   for (i = 0; i < dest->num_roms; i++)
      free_retro_subsystem_rom_info((struct
               retro_subsystem_rom_info*)&dest->roms[i]);

   if (dest->roms)
      free(dest->roms);
   dest->roms = NULL;
}

static retro_subsystem_info* clone_retro_subsystem_info(struct
      const retro_subsystem_info *src)
{
   int i;
   retro_subsystem_info *dest     = NULL;
   retro_subsystem_rom_info *roms = NULL;

   if (!src)
      return NULL;
   dest           = (struct retro_subsystem_info*)calloc(1,
         sizeof(struct retro_subsystem_info));
   dest->desc     = strcpy_alloc(src->desc);
   dest->ident    = strcpy_alloc(src->ident);
   dest->num_roms = src->num_roms;
   dest->id       = src->id;
   roms           = (struct retro_subsystem_rom_info*)
      calloc(src->num_roms, sizeof(struct retro_subsystem_rom_info));
   dest->roms     = roms;

   for (i = 0; i < src->num_roms; i++)
      clone_retro_subsystem_rom_info(&roms[i], &src->roms[i]);

   return dest;
}
#endif

static void free_retro_ctx_load_content_info(struct
      retro_ctx_load_content_info *dest)
{
   if (!dest)
      return;

   free_retro_game_info(dest->info);
   free_string_list((struct string_list*)dest->content);
   if (dest->info)
      free(dest->info);
   if (dest->content)
      free((void*)dest->content);

   dest->info    = NULL;
   dest->content = NULL;

#if 0
   free_retro_subsystem_info((retro_subsystem_info*)dest->special);
   if (dest->special)
      free(dest->special);
   dest->special = NULL;
#endif
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
#if 0
   dest->special    = clone_retro_subsystem_info(src->special);
#endif
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
