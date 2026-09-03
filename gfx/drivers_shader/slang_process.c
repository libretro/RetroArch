/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <stdint.h>

#include "glslang_util.h"
#if defined(HAVE_GLSLANG)
#include "slang_cache.h"
#endif
/* The vendored SPIRV-Cross headers end their enumerator lists with a
 * comma, which the C89 lane rejects under -pedantic; they are upstream
 * files and are not edited here. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <spirv_cross_c.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

/* SPIR-V words are uint32_t on our side; the C API declares SpvId
 * (hardcoded 'unsigned int' in spirv.h).  Same width everywhere by
 * spec, but distinct types on toolchains where uint32_t is unsigned
 * long (devkitPPC newlib), so the boundary casts below are required
 * and this guarantees they are safe: */
typedef char slang_spvid_word_size_check[
      (sizeof(SpvId) == sizeof(uint32_t)) ? 1 : -1];

#include "slang_process.h"

#include "../../verbosity.h"

static const char *texture_semantic_names[] = {
   "Original",
   "Source",
   "OriginalHistory",
   "PassOutput",
   "PassFeedback",
   "User",
   NULL
};

static const char *texture_semantic_uniform_names[] = {
   "OriginalSize",
   "SourceSize",
   "OriginalHistorySize",
   "PassOutputSize",
   "PassFeedbackSize",
   "UserSize",
   NULL
};

static const char *semantic_uniform_names[] = {
   "MVP",
   "OutputSize",
   "FinalViewportSize",
   "FrameCount",
   "FrameDirection",
   "FrameTimeDelta",
   "OriginalFPS",
   "Rotation",
   "OriginalAspect",
   "OriginalAspectRotated",
   "TotalSubFrames",
   "CurrentSubFrame",
   "HDRMode",
   "BrightnessNits",
   "Scanlines",
   "SubpixelLayout",
   "ExpandGamut",
   "InverseTonemap",
   "HDR10",
   "Gyroscope",
   "Accelerometer",
   "AccelerometerRest"
};

static bool slang_reflect(
      spvc_compiler vertex_compiler, spvc_compiler fragment_compiler,
      spvc_resources vertex_resources, spvc_resources fragment_resources,
      slang_reflection *reflection);

/* Fetch one typed resource list, treating any API failure as an
 * empty list plus error return. */
static bool spvc_fetch_list(spvc_resources resources,
      spvc_resource_type type,
      const spvc_reflected_resource **list, size_t *count)
{
   *list  = NULL;
   *count = 0;
   return spvc_resources_get_resource_list_for_type(
         resources, type, list, count) == SPVC_SUCCESS;
}

/* ---- C name maps -------------------------------------------------- */

/* Compose name+suffix into @out (SLANG_NAME_MAP_NAME_MAX bytes) and
 * return the total length, or 0 if it does not fit. */
static size_t slang_map_compose_name(char *out,
      const char *name, const char *suffix)
{
   size_t n_len = strlen(name);
   size_t s_len = suffix ? strlen(suffix) : 0;
   if (n_len + s_len + 1 > SLANG_NAME_MAP_NAME_MAX)
      return 0;
   memcpy(out, name, n_len);
   if (s_len)
      memcpy(out + n_len, suffix, s_len);
   out[n_len + s_len] = '\0';
   return n_len + s_len;
}

/* The two map types are structurally identical apart from the
 * semantic enum; a tiny generic core avoids writing the grow and
 * lookup logic twice.  Entries carry their length so lookups gate a
 * memcmp behind two byte loads, as the parameter scans do. */
static void *slang_name_map_find(void *entries, size_t count,
      size_t entry_size, const char *name, size_t name_len)
{
   size_t i;
   unsigned char *e = (unsigned char*)entries;
   for (i = 0; i < count; i++, e += entry_size)
   {
      const slang_texture_semantic_map_entry *ent =
         (const slang_texture_semantic_map_entry*)e;
      if (     ent->name[0]  == name[0]
            && ent->name_len == name_len
            && !memcmp(ent->name, name, name_len))
         return (void*)e;
   }
   return NULL;
}

static void *slang_name_map_append(void **entries, size_t *count,
      size_t *cap, size_t entry_size)
{
   unsigned char *e;
   if (*count == *cap)
   {
      size_t new_cap = *cap ? *cap * 2 : 16;
      void *grown    = realloc(*entries, new_cap * entry_size);
      if (!grown)
         return NULL;
      *entries = grown;
      *cap     = new_cap;
   }
   e = (unsigned char*)*entries + (*count)++ * entry_size;
   memset(e, 0, entry_size);
   return e;
}

bool slang_texture_semantic_name_map_set_unique(
      slang_texture_semantic_name_map *map,
      const char *name, const char *suffix,
      enum slang_texture_semantic semantic, unsigned index)
{
   char full[SLANG_NAME_MAP_NAME_MAX];
   slang_texture_semantic_map_entry *ent;
   size_t len = slang_map_compose_name(full, name, suffix);
   if (!len)
      return false;
   if (slang_name_map_find(map->entries, map->count,
            sizeof(*map->entries), full, len))
      return false; /* Alias already exists */
   ent = (slang_texture_semantic_map_entry*)slang_name_map_append(
         (void**)&map->entries, &map->count, &map->cap,
         sizeof(*map->entries));
   if (!ent)
      return false;
   memcpy(ent->name, full, len + 1);
   ent->name_len = (unsigned char)len;
   ent->semantic = semantic;
   ent->index    = index;
   return true;
}

void slang_texture_semantic_name_map_free(
      slang_texture_semantic_name_map *map)
{
   if (!map)
      return;
   free(map->entries);
   memset(map, 0, sizeof(*map));
}

bool slang_semantic_name_map_set_unique(
      slang_semantic_name_map *map,
      const char *name, const char *suffix,
      enum slang_semantic semantic, unsigned index)
{
   char full[SLANG_NAME_MAP_NAME_MAX];
   slang_semantic_map_entry *ent;
   size_t len = slang_map_compose_name(full, name, suffix);
   if (!len)
      return false;
   if (slang_name_map_find(map->entries, map->count,
            sizeof(*map->entries), full, len))
      return false; /* Alias already exists */
   ent = (slang_semantic_map_entry*)slang_name_map_append(
         (void**)&map->entries, &map->count, &map->cap,
         sizeof(*map->entries));
   if (!ent)
      return false;
   memcpy(ent->name, full, len + 1);
   ent->name_len = (unsigned char)len;
   ent->semantic = semantic;
   ent->index    = index;
   return true;
}

void slang_semantic_name_map_free(slang_semantic_name_map *map)
{
   if (!map)
      return;
   free(map->entries);
   memset(map, 0, sizeof(*map));
}

static const char *slang_texture_map_semantic_name(
      const slang_texture_semantic_name_map *map,
      enum slang_texture_semantic semantic, unsigned index)
{
   size_t i;
   for (i = 0; i < map->count; i++)
      if (     map->entries[i].semantic == semantic
            && map->entries[i].index    == index)
         return map->entries[i].name;
   return "";
}

static const char *slang_map_semantic_name(
      const slang_semantic_name_map *map,
      enum slang_semantic semantic, unsigned index)
{
   size_t i;
   for (i = 0; i < map->count; i++)
      if (     map->entries[i].semantic == semantic
            && map->entries[i].index    == index)
         return map->entries[i].name;
   return "";
}

static enum slang_texture_semantic slang_name_to_texture_semantic(
      const slang_texture_semantic_name_map *semantic_map,
      const char *name, unsigned *index)
{
   const slang_texture_semantic_map_entry *ent =
      (const slang_texture_semantic_map_entry*)slang_name_map_find(
            semantic_map->entries, semantic_map->count,
            sizeof(*semantic_map->entries), name, strlen(name));
   if (ent)
   {
      *index = ent->index;
      return ent->semantic;
   }

   return slang_name_to_texture_semantic_array(
         name, texture_semantic_names, index);
}

static enum slang_texture_semantic slang_uniform_name_to_texture_semantic(
      const slang_texture_semantic_name_map *semantic_map,
      const char *name, unsigned *index)
{
   const slang_texture_semantic_map_entry *ent =
      (const slang_texture_semantic_map_entry*)slang_name_map_find(
            semantic_map->entries, semantic_map->count,
            sizeof(*semantic_map->entries), name, strlen(name));
   if (ent)
   {
      *index = ent->index;
      return ent->semantic;
   }

   return slang_name_to_texture_semantic_array(name,
         texture_semantic_uniform_names, index);
}

static enum slang_semantic slang_uniform_name_to_semantic(
      const slang_semantic_name_map *semantic_map,
      const char *name, unsigned *index)
{
   unsigned i = 0;
   const slang_semantic_map_entry *ent =
      (const slang_semantic_map_entry*)slang_name_map_find(
            semantic_map->entries, semantic_map->count,
            sizeof(*semantic_map->entries), name, strlen(name));
   if (ent)
   {
      *index = ent->index;
      return ent->semantic;
   }

   /* No builtin semantics are arrayed. */
   *index = 0;
   for (i = 0; i < sizeof(semantic_uniform_names) / sizeof(semantic_uniform_names[0]); i++)
   {
      if (!strcmp(name, semantic_uniform_names[i]))
         return (enum slang_semantic)i;
   }

   return SLANG_INVALID_SEMANTIC;
}

/* ---- C reflection arrays ------------------------------------------ */

static void slang_semantic_location_init(slang_semantic_location *loc)
{
   loc->ubo_vertex    = -1;
   loc->push_vertex   = -1;
   loc->ubo_fragment  = -1;
   loc->push_fragment = -1;
}

/* Grow @arr to at least @minimum elements, zeroing new elements and
 * presetting their GL locations to -1 (the previous default-
 * constructed state). */
static bool slang_texture_sem_array_resize_min(
      slang_texture_semantic_array *arr, unsigned minimum)
{
   if (arr->size >= minimum)
      return true;
   if (minimum > arr->cap)
   {
      size_t new_cap = arr->cap ? arr->cap : 4;
      slang_texture_semantic_meta *grown;
      while (new_cap < minimum)
         new_cap *= 2;
      grown = (slang_texture_semantic_meta*)realloc(arr->data,
            new_cap * sizeof(*grown));
      if (!grown)
         return false;
      arr->data = grown;
      arr->cap  = new_cap;
   }
   while (arr->size < minimum)
   {
      slang_texture_semantic_meta *m = &arr->data[arr->size++];
      memset(m, 0, sizeof(*m));
      slang_semantic_location_init(&m->location);
   }
   return true;
}

static bool slang_float_params_resize_min(
      slang_reflection *reflection, unsigned minimum)
{
   if (reflection->num_float_parameters >= minimum)
      return true;
   if (minimum > reflection->cap_float_parameters)
   {
      size_t new_cap = reflection->cap_float_parameters
         ? reflection->cap_float_parameters : 8;
      slang_semantic_meta *grown;
      while (new_cap < minimum)
         new_cap *= 2;
      grown = (slang_semantic_meta*)realloc(
            reflection->semantic_float_parameters,
            new_cap * sizeof(*grown));
      if (!grown)
         return false;
      reflection->semantic_float_parameters = grown;
      reflection->cap_float_parameters      = new_cap;
   }
   while (reflection->num_float_parameters < minimum)
   {
      slang_semantic_meta *m = &reflection->semantic_float_parameters[
         reflection->num_float_parameters++];
      memset(m, 0, sizeof(*m));
      slang_semantic_location_init(&m->location);
   }
   return true;
}

bool slang_reflection_init(slang_reflection *reflection)
{
   unsigned i;
   memset(reflection, 0, sizeof(*reflection));
   for (i = 0; i < SLANG_NUM_SEMANTICS; i++)
      slang_semantic_location_init(&reflection->semantics[i].location);
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      if (!slang_texture_semantic_is_array((enum slang_texture_semantic)i))
      {
         if (!slang_texture_sem_array_resize_min(
                  &reflection->semantic_textures[i], 1))
         {
            slang_reflection_free(reflection);
            return false;
         }
      }
   }
   return true;
}

void slang_reflection_free(slang_reflection *reflection)
{
   unsigned i;
   if (!reflection)
      return;
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
      free(reflection->semantic_textures[i].data);
   free(reflection->semantic_float_parameters);
   memset(reflection, 0, sizeof(*reflection));
}


static bool sem_array_push_uniform(uniform_sem_t **arr,
      int *count, int *cap, const uniform_sem_t *u)
{
   if (*count == *cap)
   {
      int new_cap = *cap ? *cap * 2 : 16;
      uniform_sem_t *grown = (uniform_sem_t*)realloc(*arr,
            (size_t)new_cap * sizeof(**arr));
      if (!grown)
         return false;
      *arr = grown;
      *cap = new_cap;
   }
   (*arr)[(*count)++] = *u;
   return true;
}

static bool sem_array_push_texture(texture_sem_t **arr,
      int *count, int *cap, const texture_sem_t *t)
{
   if (*count == *cap)
   {
      int new_cap = *cap ? *cap * 2 : 16;
      texture_sem_t *grown = (texture_sem_t*)realloc(*arr,
            (size_t)new_cap * sizeof(**arr));
      if (!grown)
         return false;
      *arr = grown;
      *cap = new_cap;
   }
   (*arr)[(*count)++] = *t;
   return true;
}

static bool slang_process_reflection(
      spvc_compiler          vs_compiler,
      spvc_compiler          ps_compiler,
      spvc_resources         vs_resources,
      spvc_resources         ps_resources,
      struct video_shader*   shader_info,
      unsigned               pass_number,
      const semantics_map_t* map,
      pass_semantics_t*      out)
{
   int semantic;
   unsigned i;
   bool ret                    = false;
   texture_sem_t *textures     = NULL;
   int texture_count           = 0;
   int texture_cap             = 0;
   uniform_sem_t *uniforms[SLANG_CBUFFER_MAX];
   int uniform_count[SLANG_CBUFFER_MAX];
   int uniform_cap[SLANG_CBUFFER_MAX];
   slang_texture_semantic_name_map texture_semantic_map;
   slang_texture_semantic_name_map texture_semantic_uniform_map;
   slang_semantic_name_map uniform_semantic_map;
   slang_reflection sl_reflection;

   memset(uniforms, 0, sizeof(uniforms));
   memset(uniform_count, 0, sizeof(uniform_count));
   memset(uniform_cap, 0, sizeof(uniform_cap));
   memset(&texture_semantic_map, 0, sizeof(texture_semantic_map));
   memset(&texture_semantic_uniform_map, 0,
         sizeof(texture_semantic_uniform_map));
   memset(&uniform_semantic_map, 0, sizeof(uniform_semantic_map));
   if (!slang_reflection_init(&sl_reflection))
      return false;

   for (i = 0; i < shader_info->passes; i++)
   {
      const char *name = shader_info->pass[i].alias;
      if (!*name)
         continue;

      if (!slang_texture_semantic_name_map_set_unique(
               &texture_semantic_map, name, NULL,
               SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i))
         goto out;

      if (!slang_texture_semantic_name_map_set_unique(
               &texture_semantic_uniform_map, name, "Size",
               SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i))
         goto out;

      if (!slang_texture_semantic_name_map_set_unique(
               &texture_semantic_map, name, "Feedback",
               SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i))
         goto out;

      if (!slang_texture_semantic_name_map_set_unique(
               &texture_semantic_uniform_map, name, "FeedbackSize",
               SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i))
         goto out;
   }

   for (i = 0; i < shader_info->luts; i++)
   {
      if (!slang_texture_semantic_name_map_set_unique(
               &texture_semantic_map, shader_info->lut[i].id, NULL,
               SLANG_TEXTURE_SEMANTIC_USER, i))
         goto out;

      if (!slang_texture_semantic_name_map_set_unique(
               &texture_semantic_uniform_map,
               shader_info->lut[i].id, "Size",
               SLANG_TEXTURE_SEMANTIC_USER, i))
         goto out;
   }

   for (i = 0; i < shader_info->num_parameters; i++)
   {
      if (!slang_semantic_name_map_set_unique(
               &uniform_semantic_map, shader_info->parameters[i].id,
               NULL, SLANG_SEMANTIC_FLOAT_PARAMETER, i))
         goto out;
   }

   sl_reflection.pass_number                  = pass_number;
   sl_reflection.texture_semantic_map         = &texture_semantic_map;
   sl_reflection.texture_semantic_uniform_map = &texture_semantic_uniform_map;
   sl_reflection.semantic_map                 = &uniform_semantic_map;

   if (!slang_reflect(vs_compiler, ps_compiler,
            vs_resources, ps_resources, &sl_reflection))
   {
      RARCH_ERR("[Slang] Failed to reflect SPIR-V."
            " Resource usage is inconsistent with "
                "expectations.\n");
      goto out;
   }

   out->cbuffers[SLANG_CBUFFER_UBO].stage_mask = sl_reflection.ubo_stage_mask;
   out->cbuffers[SLANG_CBUFFER_UBO].binding    = sl_reflection.ubo_binding;
   out->cbuffers[SLANG_CBUFFER_UBO].size       = (unsigned)((sl_reflection.ubo_size + 0xF) & ~0xF);
   out->cbuffers[SLANG_CBUFFER_PC].stage_mask  = sl_reflection.push_constant_stage_mask;
   out->cbuffers[SLANG_CBUFFER_PC].binding     = sl_reflection.ubo_binding ? 0 : 1;
   out->cbuffers[SLANG_CBUFFER_PC].size        = (unsigned)((sl_reflection.push_constant_size + 0xF) & ~0xF);

   for (semantic = 0; semantic < SLANG_NUM_BASE_SEMANTICS; semantic++)
   {
      slang_semantic_meta *src = &sl_reflection.semantics[semantic];
      if (src->push_constant || src->uniform)
      {
         uniform_sem_t uniform;
         enum slang_semantic _semantic   = (enum slang_semantic)semantic;

         uniform.data = map->uniforms[semantic];
         uniform.size = src->num_components * (unsigned)sizeof(float);
         if (semantic < (int)(sizeof(semantic_uniform_names) / sizeof(*semantic_uniform_names)))
            strlcpy(uniform.id, semantic_uniform_names[_semantic], sizeof(uniform.id));
         else
            strlcpy(uniform.id, slang_map_semantic_name(sl_reflection.semantic_map, _semantic, 0), sizeof(uniform.id));

         if (src->push_constant)
         {
            uniform.offset = (unsigned)src->push_constant_offset;
            if (!sem_array_push_uniform(&uniforms[SLANG_CBUFFER_PC],
                     &uniform_count[SLANG_CBUFFER_PC],
                     &uniform_cap[SLANG_CBUFFER_PC], &uniform))
               goto out;
         }
         else
         {
            uniform.offset = (unsigned)src->ubo_offset;
            if (!sem_array_push_uniform(&uniforms[SLANG_CBUFFER_UBO],
                     &uniform_count[SLANG_CBUFFER_UBO],
                     &uniform_cap[SLANG_CBUFFER_UBO], &uniform))
               goto out;
         }
      }
   }

   for (i = 0; i < sl_reflection.num_float_parameters; i++)
   {
      slang_semantic_meta *src = &sl_reflection.semantic_float_parameters[i];

      if (src->push_constant || src->uniform)
      {
         uniform_sem_t uniform;

         uniform.data = &shader_info->parameters[i].current;
         uniform.size = sizeof(float);
         strlcpy(uniform.id, slang_map_semantic_name(sl_reflection.semantic_map, SLANG_SEMANTIC_FLOAT_PARAMETER, i), sizeof(uniform.id));

         if (src->push_constant)
         {
            uniform.offset = (unsigned)src->push_constant_offset;
            if (!sem_array_push_uniform(&uniforms[SLANG_CBUFFER_PC],
                     &uniform_count[SLANG_CBUFFER_PC],
                     &uniform_cap[SLANG_CBUFFER_PC], &uniform))
               goto out;
         }
         else
         {
            uniform.offset = (unsigned)src->ubo_offset;
            if (!sem_array_push_uniform(&uniforms[SLANG_CBUFFER_UBO],
                     &uniform_count[SLANG_CBUFFER_UBO],
                     &uniform_cap[SLANG_CBUFFER_UBO], &uniform))
               goto out;
         }
      }
   }
   for (semantic = 0; semantic < SLANG_NUM_TEXTURE_SEMANTICS; semantic++)
   {
      unsigned index;

      for (index = 0; index <
            sl_reflection.semantic_textures[semantic].size; index++)
      {
         slang_texture_semantic_meta *src =
            &sl_reflection.semantic_textures[semantic].data[index];

         if (src->stage_mask)
         {
            static const char* names[] = {
               "Original", "Source", "OriginalHistory", "PassOutput", "PassFeedback",
            };
            texture_sem_t texture;
            enum slang_texture_semantic
               _semantic              = (enum slang_texture_semantic)semantic;
            texture.id[0]             = '\0';
			
			if (semantic == (int)SLANG_TEXTURE_SEMANTIC_ORIGINAL)
			{
				strlcpy(texture.id, names[semantic], sizeof(texture.id));
				texture.wrap    = shader_info->pass[0].wrap;
				texture.filter  = shader_info->pass[0].filter;
			}
			else if (semantic == (int)SLANG_TEXTURE_SEMANTIC_SOURCE)
			{
				strlcpy(texture.id, names[semantic], sizeof(texture.id));
				texture.wrap    = shader_info->pass[pass_number].wrap;
				texture.filter  = shader_info->pass[pass_number].filter;
			}
			else if (semantic == (int)SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY)
			{
				size_t _len = strlcpy(texture.id, names[semantic], sizeof(texture.id));
				snprintf(texture.id + _len, sizeof(texture.id) - _len, "%d", index);
				texture.wrap    = shader_info->pass[0].wrap;
				texture.filter  = shader_info->pass[0].filter;
			}
			else if (semantic == (int)SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT)
			{
				size_t _len = strlcpy(texture.id, names[semantic], sizeof(texture.id));
				snprintf(texture.id + _len, sizeof(texture.id) - _len, "%d", index);
				if ((index + 1) < shader_info->passes)
				{
					texture.wrap    = shader_info->pass[index + 1].wrap;
					texture.filter  = shader_info->pass[index + 1].filter;
				}
				else /* should not happen and already be checked */
				{
					texture.wrap    = shader_info->pass[index].wrap;
					texture.filter  = shader_info->pass[index].filter;
				}
			}
			else if (semantic == (int)SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK)
			{
				size_t _len = strlcpy(texture.id, names[semantic], sizeof(texture.id));
				snprintf(texture.id + _len, sizeof(texture.id) - _len, "%d", index);
				texture.wrap    = shader_info->pass[index].wrap;
				texture.filter  = shader_info->pass[index].filter;
			}
			else /* SLANG_TEXTURE_SEMANTIC_USER */
			{
				strlcpy(texture.id, slang_texture_map_semantic_name(sl_reflection.texture_semantic_map, _semantic, index), sizeof(texture.id));
				texture.wrap    = shader_info->lut[index].wrap;
				texture.filter  = shader_info->lut[index].filter;
			}
			
            texture.texture_data =
               (void*)((uintptr_t)map->textures[semantic].image + index * map->textures[semantic].image_stride);

            texture.stage_mask = src->stage_mask;
            texture.binding    = src->binding;

            if (!sem_array_push_texture(&textures, &texture_count,
                     &texture_cap, &texture))
               goto out;

            if (semantic == SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK)
               shader_info->pass[index].feedback = true;

            if (semantic == SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY &&
                (unsigned)shader_info->history_size < index)
               shader_info->history_size = index;
         }

         if (src->push_constant || src->uniform)
         {
            uniform_sem_t uniform;
            enum slang_texture_semantic _semantic = (enum slang_texture_semantic)semantic;
            static const char* names[] = {
               "OriginalSize", "SourceSize", "OriginalHistorySize", "PassOutputSize", "PassFeedbackSize",
            };

            uniform.data = (void*)((uintptr_t)map->textures[semantic].size
                  + index * map->textures[semantic].size_stride);
            uniform.size = 4 * sizeof(float);
            if (semantic < (int)SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY)
               strlcpy(uniform.id, names[_semantic], sizeof(uniform.id));
            else
            {
               int size = sizeof(names) / sizeof(*names);
               if (semantic < size)
               {
                  size_t _len = strlcpy(uniform.id, names[_semantic], sizeof(uniform.id));
                  snprintf(uniform.id + _len, sizeof(uniform.id) - _len, "%d", index);
               }
               else
                  strlcpy(uniform.id, slang_texture_map_semantic_name(sl_reflection.texture_semantic_uniform_map, _semantic, index), sizeof(uniform.id));
            }

            if (src->push_constant)
            {
               uniform.offset = (unsigned)src->push_constant_offset;
               if (!sem_array_push_uniform(&uniforms[SLANG_CBUFFER_PC],
                        &uniform_count[SLANG_CBUFFER_PC],
                        &uniform_cap[SLANG_CBUFFER_PC], &uniform))
                  goto out;
            }
            else
            {
               uniform.offset = (unsigned)src->ubo_offset;
               if (!sem_array_push_uniform(&uniforms[SLANG_CBUFFER_UBO],
                        &uniform_count[SLANG_CBUFFER_UBO],
                        &uniform_cap[SLANG_CBUFFER_UBO], &uniform))
                  goto out;
            }
         }
      }
   }

   {
      /* Hand the arrays to the caller directly: append the NULL
       * sentinel each consumer expects and transfer ownership - the
       * previous implementation built std::vectors and then malloc-
       * copied them wholesale. */
      texture_sem_t tex_sentinel;
      memset(&tex_sentinel, 0, sizeof(tex_sentinel));
      out->texture_count = texture_count;
      if (!sem_array_push_texture(&textures, &texture_count,
               &texture_cap, &tex_sentinel))
         goto out;
      out->textures = textures;
      textures      = NULL;

      for (i = 0; i < SLANG_CBUFFER_MAX; i++)
      {
         uniform_sem_t uni_sentinel;
         if (!uniform_count[i])
            continue;
         memset(&uni_sentinel, 0, sizeof(uni_sentinel));
         out->cbuffers[i].uniform_count = uniform_count[i];
         if (!sem_array_push_uniform(&uniforms[i], &uniform_count[i],
                  &uniform_cap[i], &uni_sentinel))
            goto out;
         out->cbuffers[i].uniforms = uniforms[i];
         uniforms[i]               = NULL;
      }
   }

   ret = true;

out:
   free(textures);
   for (i = 0; i < SLANG_CBUFFER_MAX; i++)
      free(uniforms[i]);
   slang_texture_semantic_name_map_free(&texture_semantic_map);
   slang_texture_semantic_name_map_free(&texture_semantic_uniform_map);
   slang_semantic_name_map_free(&uniform_semantic_map);
   slang_reflection_free(&sl_reflection);
   return ret;
}

/* Grow-buffer used to assemble stage source in C.  Returns false on
 * allocation failure; the buffer is always left in a freeable state. */
struct stage_source_buf
{
   char *data;
   size_t len;
   size_t cap;
};

#if defined(HAVE_GLSLANG)
static bool stage_source_append(struct stage_source_buf *buf,
      const char *s, size_t s_len)
{
   if (buf->len + s_len + 1 > buf->cap)
   {
      char *grown;
      size_t new_cap = buf->cap ? buf->cap : 4096;
      while (buf->len + s_len + 1 > new_cap)
         new_cap *= 2;
      grown = (char*)realloc(buf->data, new_cap);
      if (!grown)
         return false;
      buf->data = grown;
      buf->cap  = new_cap;
   }
   memcpy(buf->data + buf->len, s, s_len);
   buf->len            += s_len;
   buf->data[buf->len]  = '\0';
   return true;
}

static bool stage_source_append_line(struct stage_source_buf *buf,
      const char *line)
{
   if (!stage_source_append(buf, line, strlen(line)))
      return false;
   return stage_source_append(buf, "\n", 1);
}

/* Assemble the source for one stage from the preprocessed line buffer.
 * Returns a malloc'd '\0'-terminated string the caller must free(),
 * or NULL on allocation failure.  An empty line buffer yields an
 * empty (but non-NULL) string, matching the previous behavior of
 * returning "". */
static char *build_stage_source(
      const struct shader_line_buf *lines, const char *stage)
{
   size_t i;
   struct stage_source_buf buf;
   bool active = true;

   buf.data = NULL;
   buf.len  = 0;
   buf.cap  = 0;

   if (!lines || lines->num_lines < 1)
   {
      if (!stage_source_append(&buf, "", 0))
         return NULL;
      return buf.data;
   }

   /* Reserve the full preprocessed length up front (lines->len counts
    * every line plus separators), so assembly never reallocates - the
    * same exact-fit reservation the std::string implementation made
    * via str.reserve(lines->len). */
   buf.data = (char*)malloc(lines->len + 2);
   if (!buf.data)
      return NULL;
   buf.cap     = lines->len + 2;
   buf.data[0] = '\0';

   /* Version header (line 0). */
   if (!stage_source_append_line(&buf, shader_line_buf_get(lines, 0)))
      goto error;

   for (i = 1; i < lines->num_lines; i++)
   {
      const char *line = shader_line_buf_get(lines, i);
      if (!memcmp(line, "#pragma", sizeof("#pragma")-1))
      {
         if (!memcmp(line, "#pragma stage ", sizeof("#pragma stage ")-1))
         {
            if (stage && *stage)
            {
               char expected[128];
               size_t _len = strlcpy_lit(expected, "#pragma stage ", sizeof(expected));
               strlcpy(expected + _len, stage, sizeof(expected) - _len);
               active = !strcmp(expected, line);
            }
         }
         else if (
                  !memcmp(line, "#pragma name ", sizeof("#pragma name ")-1)
               || !memcmp(line, "#pragma format ", sizeof("#pragma format ")-1))
         {
            /* Ignore */
         }
         else if (active)
         {
            if (!stage_source_append_line(&buf, line))
               goto error;
         }
      }
      else if (active)
      {
         if (!stage_source_append_line(&buf, line))
            goto error;
      }
   }
   return buf.data;

error:
   free(buf.data);
   return NULL;
}
#endif /* HAVE_GLSLANG */

void glslang_output_init(glslang_output *output)
{
   memset(output, 0, sizeof(*output));
   output->meta.rt_format = SLANG_FORMAT_UNKNOWN;
}

void glslang_output_free(glslang_output *output)
{
   if (!output)
      return;
   free(output->vertex);
   free(output->fragment);
   free(output->meta.parameters);
   glslang_output_init(output);
}

bool glslang_meta_add_parameter(glslang_meta *meta,
      const glslang_parameter *param)
{
   if (meta->num_parameters == meta->cap_parameters)
   {
      glslang_parameter *grown;
      size_t new_cap = meta->cap_parameters ? meta->cap_parameters * 2 : 16;
      grown = (glslang_parameter*)realloc(meta->parameters,
            new_cap * sizeof(*grown));
      if (!grown)
         return false;
      meta->parameters     = grown;
      meta->cap_parameters = new_cap;
   }
   meta->parameters[meta->num_parameters++] = *param;
   return true;
}

static bool glslang_parse_meta(const struct shader_line_buf *lines,
      glslang_meta *meta)
{
   char id[64];
   char desc[64];
   size_t i;

   id[0]   = '\0';
   desc[0] = '\0';

   /* Pre-count parameters to size the array in one allocation */
   {
      size_t param_count = 0;
      for (i = 0; i < lines->num_lines; i++)
      {
         const char *line = shader_line_buf_get(lines, i);
         if (line && !memcmp(line, "#pragma parameter ",
                  sizeof("#pragma parameter ") - 1))
            param_count++;
      }
      if (param_count > meta->cap_parameters)
      {
         glslang_parameter *grown = (glslang_parameter*)realloc(
               meta->parameters, param_count * sizeof(*grown));
         if (!grown)
            return false;
         meta->parameters     = grown;
         meta->cap_parameters = param_count;
      }
   }

   for (i = 0; i < lines->num_lines; i++)
   {
      const char *line = shader_line_buf_get(lines, i);
      if (!line)
         continue;

      if (memcmp(line, "#pragma", sizeof("#pragma") - 1))
         continue;

      /* Check for shader identifier */
      if (!memcmp(line, "#pragma name ",
               sizeof("#pragma name ") - 1))
      {
         const char *str = line + (sizeof("#pragma name ") - 1);
         while (*str == ' ' || *str == '\t')
            str++;
         if (meta->name[0])
         {
            RARCH_ERR("[Slang] Trying to declare multiple names for file.\n");
            return false;
         }
         strlcpy(meta->name, str, sizeof(meta->name));
      }
      /* Check for shader parameters */
      else if (!memcmp(line, "#pragma parameter ",
               sizeof("#pragma parameter ") - 1))
      {
         float initial, minimum, maximum, step;
         int fields         = 0;
         const char *s      = line + (sizeof("#pragma parameter ") - 1);
         size_t len         = 0;
         size_t id_len, desc_len;
         char *end          = NULL;

         /* Parse id */
         while (*s == ' ' || *s == '\t')
            s++;
         len = 0;
         while (s[len] && s[len] != ' ' && s[len] != '\t')
            len++;
         if (len == 0 || len >= sizeof(id))
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         memcpy(id, s, len);
         id[len] = '\0';
         id_len  = len;
         s += len;

         /* Parse quoted description */
         while (*s == ' ' || *s == '\t')
            s++;
         if (*s != '"')
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s++;
         len = 0;
         while (s[len] && s[len] != '"')
            len++;
         if (s[len] != '"' || len >= sizeof(desc))
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         memcpy(desc, s, len);
         desc[len] = '\0';
         desc_len  = len;
         s += len + 1;

         /* Parse initial */
         while (*s == ' ' || *s == '\t')
            s++;
         initial = (float)strtod(s, &end);
         if (end == s)
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s = end;

         /* Parse minimum */
         while (*s == ' ' || *s == '\t')
            s++;
         minimum = (float)strtod(s, &end);
         if (end == s)
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s = end;

         /* Parse maximum */
         while (*s == ' ' || *s == '\t')
            s++;
         maximum = (float)strtod(s, &end);
         if (end == s)
         {
            RARCH_ERR("[Slang] Invalid #pragma parameter line: \"%s\".\n", line);
            return false;
         }
         s       = end;
         fields  = 5;

         /* Parse step (optional) */
         while (*s == ' ' || *s == '\t')
            s++;
         if (*s)
         {
            step = (float)strtod(s, &end);
            if (end != s)
               fields = 6;
         }

         if (fields == 5)
         {
            step    = 0.1f * (maximum - minimum);
            fields  = 6;
         }

         {
            bool parameter_found   = false;
            size_t parameter_index = 0;
            size_t j;

            for (j = 0; j < meta->num_parameters; j++)
            {
               /* Cheap inline gate before the memcmp call: first byte
                * plus the terminator at id_len (which doubles as a
                * length check) reject almost every candidate without a
                * libc call.  Mega Bezel-class shaders carry 600+
                * parameters, making this scan O(n^2)-hot; the previous
                * std::string version got the same effect from its
                * stored size. */
               const char *pid = meta->parameters[j].id;
               if (pid[0] == id[0] && pid[id_len] == '\0'
                     && !memcmp(pid, id, id_len))
               {
                  parameter_found = true;
                  parameter_index = j;
                  break;
               }
            }

            /* Allow duplicate #pragma parameter, but only
             * if they are exactly the same. */
            if (parameter_found)
            {
               const glslang_parameter *parameter =
                  &meta->parameters[parameter_index];
               if (     memcmp(parameter->desc, desc, desc_len + 1)
                     || (parameter->initial != initial)
                     || (parameter->minimum != minimum)
                     || (parameter->maximum != maximum)
                     || (parameter->step    != step)
                  )
               {
                  RARCH_ERR("[Slang] Duplicate parameters"
                        " found for \"%s\", but arguments"
                        " do not match.\n", id);
                  return false;
               }
            }
            else if (meta->num_parameters < meta->cap_parameters)
            {
               /* The pre-count above reserved one slot per #pragma
                * parameter line, so a fresh (non-duplicate) parameter
                * always fits; write it in place with the lengths the
                * parse just measured. */
               glslang_parameter *p = &meta->parameters[meta->num_parameters++];
               memcpy(p->id, id, id_len + 1);
               memcpy(p->desc, desc, desc_len + 1);
               p->initial = initial;
               p->minimum = minimum;
               p->maximum = maximum;
               p->step    = step;
            }
            else
            {
               glslang_parameter p;
               strlcpy(p.id, id, sizeof(p.id));
               strlcpy(p.desc, desc, sizeof(p.desc));
               p.initial = initial;
               p.minimum = minimum;
               p.maximum = maximum;
               p.step    = step;
               if (!glslang_meta_add_parameter(meta, &p))
                  return false;
            }
         }
      }
      /* Check for framebuffer format */
      else if (!memcmp(line, "#pragma format ",
               sizeof("#pragma format ") - 1))
      {
         const char *str = line + (sizeof("#pragma format ") - 1);
         while (*str == ' ' || *str == '\t')
            str++;
         if (meta->rt_format != SLANG_FORMAT_UNKNOWN)
         {
            RARCH_ERR("[Slang] Trying to declare format"
                  " multiple times for file.\n");
            return false;
         }
         meta->rt_format = glslang_find_format(str);
         if (meta->rt_format == SLANG_FORMAT_UNKNOWN)
         {
            RARCH_ERR("[Slang] Failed to find format \"%s\".\n", str);
            return false;
         }
      }
   }

   return true;
}

/* -----------------------------------------------------------------------
 * glslang_compile_shader — now uses shader_line_buf
 * ----------------------------------------------------------------------- */
bool glslang_compile_shader(const char *shader_path, glslang_output *output)
{
   return glslang_compile_shader_cached(shader_path, output, NULL);
}

bool glslang_compile_shader_cached(const char *shader_path,
      glslang_output *output, void *include_cache)
{
   glslang_output_init(output);

#if defined(HAVE_GLSLANG)
   {
      struct shader_line_buf lines;
      char cache_filename[PATH_MAX_LENGTH];
      char *vertex_source   = NULL;
      char *fragment_source = NULL;

      if (!shader_line_buf_init(&lines))
         return false;

      RARCH_LOG("[Slang] Compiling shader: \"%s\".\n", shader_path);

      if (!glslang_read_shader_file_cached(shader_path, &lines, true, false,
               include_cache))
         goto error;

      /* Assemble both stage sources once; used for the cache key and,
       * on a cache miss, for compilation. */
      vertex_source   = build_stage_source(&lines, "vertex");
      fragment_source = build_stage_source(&lines, "fragment");
      if (!vertex_source || !fragment_source)
         goto error;

      spirv_cache_compute_hash(vertex_source, fragment_source,
            cache_filename);

      /* Try to load from cache */
      if (spirv_cache_load(cache_filename, output))
      {
         RARCH_LOG("[Slang] Loaded shader from cache: \"%s\".\n", shader_path);
         free(vertex_source);
         free(fragment_source);
         shader_line_buf_free(&lines);
         return true;
      }

      if (!glslang_parse_meta(&lines, &output->meta))
         goto error;

      if (!glslang_compile_spirv(vertex_source,
               GLSLANG_COMPILE_STAGE_VERTEX,
               &output->vertex, &output->vertex_len))
      {
         RARCH_ERR("[Slang] Failed to compile vertex shader stage.\n");
         goto error;
      }

      /* The vertex source is dead now; release it before the fragment
       * compile so only one stage source is resident at a time. */
      free(vertex_source);
      vertex_source = NULL;

      if (!glslang_compile_spirv(fragment_source,
               GLSLANG_COMPILE_STAGE_FRAGMENT,
               &output->fragment, &output->fragment_len))
      {
         RARCH_ERR("[Slang] Failed to compile fragment shader stage.\n");
         goto error;
      }

      /* Save to cache */
      spirv_cache_save(cache_filename, output);

      free(vertex_source);
      free(fragment_source);
      shader_line_buf_free(&lines);

      return true;

error:
      free(vertex_source);
      free(fragment_source);
      shader_line_buf_free(&lines);
      glslang_output_free(output);
   }
#endif

   {
      size_t _len;
      char msg[NAME_MAX_LENGTH];

      _len = snprintf(msg, sizeof(msg), "Failed to compile shader: \"%s\".",
            path_basename(shader_path));

      runloop_msg_queue_push(msg, _len, 1, 120, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   }

   return false;
}

bool slang_preprocess_parse_parameters_meta(const glslang_meta *meta,
      struct video_shader *shader)
{
   unsigned i;
   unsigned old_num_parameters = shader->num_parameters;

   /* Assumes num_parameters is
    * initialized to something sane. */
   for (i = 0; i < meta->num_parameters; i++)
   {
      unsigned k;
      struct video_shader_parameter *p = NULL;
      bool mismatch_dup                = false;
      const char *mid                  = meta->parameters[i].id;
      size_t mid_len                   = strlen(mid);
      for (k = 0; k < shader->num_parameters; k++)
      {
         /* This scan is O(meta x shader) and Mega Bezel-class presets
          * accumulate 1000+ parameters with long shared prefixes, so
          * gate the memcmp behind two byte loads (first char + the
          * terminator at mid_len, which doubles as a length check). */
         const char *sid = shader->parameters[k].id;
         if (sid[0] == mid[0] && sid[mid_len] == '\0'
               && !memcmp(sid, mid, mid_len))
         {
            p = &shader->parameters[k];
            break;
         }
      }

      if (p != NULL)
      {
         /* Allow duplicate #pragma parameter, but only
          * if they are exactly the same. */
         if (     strcmp(meta->parameters[i].desc, p->desc)
               || meta->parameters[i].initial != p->initial
               || meta->parameters[i].minimum != p->minimum
               || meta->parameters[i].maximum != p->maximum
               || meta->parameters[i].step    != p->step)
         {
            RARCH_ERR("[Slang] Duplicate parameters"
                  " found for \"%s\", but arguments do not match.\n",
                  p->id);
            mismatch_dup = true;
         }
         else
            continue;
      }

      if (mismatch_dup || shader->num_parameters == GFX_MAX_PARAMETERS)
      {
         shader->num_parameters = old_num_parameters;
         return false;
      }

      if (!(p = (struct video_shader_parameter*)
         &shader->parameters[shader->num_parameters++]))
         continue;

      strlcpy(p->id,   meta->parameters[i].id,   sizeof(p->id));
      strlcpy(p->desc, meta->parameters[i].desc, sizeof(p->desc));
      p->initial = meta->parameters[i].initial;
      p->minimum = meta->parameters[i].minimum;
      p->maximum = meta->parameters[i].maximum;
      p->step    = meta->parameters[i].step;
      p->current = meta->parameters[i].initial;
   }

   return true;
}

/* -----------------------------------------------------------------------
 * slang_preprocess_parse_parameters (C-linkage overload) — uses shader_line_buf
 * ----------------------------------------------------------------------- */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader)
{
   return slang_preprocess_parse_parameters_cached(shader_path, shader,
         NULL);
}

bool slang_preprocess_parse_parameters_cached(const char *shader_path,
      struct video_shader *shader, void *include_cache)
{
   struct shader_line_buf lines;

   memset(&lines, 0, sizeof(lines));

   if (shader_line_buf_init(&lines))
   {
      if (glslang_read_shader_file_cached(shader_path, &lines, true, false,
               include_cache))
      {
         glslang_meta meta;
         memset(&meta, 0, sizeof(meta));
         meta.rt_format = SLANG_FORMAT_UNKNOWN;
         if (glslang_parse_meta(&lines, &meta))
         {
            bool ret = slang_preprocess_parse_parameters_meta(&meta, shader);
            free(meta.parameters);
            shader_line_buf_free(&lines);
            return ret;
         }
         free(meta.parameters);
      }
   }

   shader_line_buf_free(&lines);
   return false;
}

bool slang_process(
      struct video_shader*   shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out)
{
   glslang_output          output;
   spvc_context            ctx         = NULL;
   spvc_parsed_ir          vs_ir       = NULL;
   spvc_parsed_ir          ps_ir       = NULL;
   spvc_compiler           vs_compiler = NULL;
   spvc_compiler           ps_compiler = NULL;
   spvc_resources          vs_resources = NULL;
   spvc_resources          ps_resources = NULL;
   spvc_compiler_options   vs_options  = NULL;
   spvc_compiler_options   ps_options  = NULL;
   spvc_backend            backend     = SPVC_BACKEND_GLSL;
   const char             *vs_code     = NULL;
   const char             *ps_code     = NULL;
   const spvc_reflected_resource *list = NULL;
   size_t                  list_num    = 0;
   struct video_shader_pass *pass      = &shader_info->pass[pass_number];

   if (!glslang_compile_shader(pass->source.path, &output))
      return false;

   if (!slang_preprocess_parse_parameters_meta(&output.meta, shader_info))
   {
      glslang_output_free(&output);
      return false;
   }

   if (!*pass->alias && output.meta.name[0])
      strlcpy(pass->alias, output.meta.name, sizeof(pass->alias) - 1);

   out->format          = output.meta.rt_format;
   out->explicit_format = (output.meta.rt_format != SLANG_FORMAT_UNKNOWN);

   if (out->format == SLANG_FORMAT_UNKNOWN)
   {
      if (pass->fbo.flags & FBO_SCALE_FLAG_SRGB_FBO)
         out->format = SLANG_FORMAT_R8G8B8A8_SRGB;
      else if (pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
         out->format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
      else if (pass->fbo.flags & FBO_SCALE_FLAG_RGB10_FBO)
         out->format = SLANG_FORMAT_A2B10G10R10_UNORM_PACK32;
      else
         out->format = SLANG_FORMAT_R8G8B8A8_UNORM;
   }

   pass->source.string.vertex   = NULL;
   pass->source.string.fragment = NULL;

   switch (dst_type)
   {
      case RARCH_SHADER_HLSL:
      case RARCH_SHADER_CG:
#ifdef HAVE_HLSL
         backend = SPVC_BACKEND_HLSL;
         break;
#else
         RARCH_ERR("[Slang] HLSL backend not compiled in.\n");
         goto error;
#endif
      case RARCH_SHADER_METAL:
         backend = SPVC_BACKEND_MSL;
         break;
      default:
         backend = SPVC_BACKEND_GLSL;
         break;
   }

   if (spvc_context_create(&ctx) != SPVC_SUCCESS)
      goto error;

   if (   spvc_context_parse_spirv(ctx, (const SpvId*)output.vertex,
            output.vertex_len, &vs_ir) != SPVC_SUCCESS
       || spvc_context_parse_spirv(ctx, (const SpvId*)output.fragment,
            output.fragment_len, &ps_ir) != SPVC_SUCCESS
       || spvc_context_create_compiler(ctx, backend, vs_ir,
            SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
            &vs_compiler) != SPVC_SUCCESS
       || spvc_context_create_compiler(ctx, backend, ps_ir,
            SPVC_CAPTURE_MODE_TAKE_OWNERSHIP,
            &ps_compiler) != SPVC_SUCCESS
       || spvc_compiler_create_shader_resources(vs_compiler,
            &vs_resources) != SPVC_SUCCESS
       || spvc_compiler_create_shader_resources(ps_compiler,
            &ps_resources) != SPVC_SUCCESS)
      goto spvc_error;

   /* Normalize buffer bindings the way the C++ implementation did:
    * uniform buffer at binding 0, push constant buffer at binding 1. */
   if (!spvc_fetch_list(vs_resources,
            SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &list_num))
      goto spvc_error;
   if (list_num)
      spvc_compiler_set_decoration(vs_compiler, list[0].id,
            SpvDecorationBinding, 0);
   if (!spvc_fetch_list(ps_resources,
            SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &list_num))
      goto spvc_error;
   if (list_num)
      spvc_compiler_set_decoration(ps_compiler, list[0].id,
            SpvDecorationBinding, 0);

   if (!spvc_fetch_list(vs_resources,
            SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &list, &list_num))
      goto spvc_error;
   if (list_num)
      spvc_compiler_set_decoration(vs_compiler, list[0].id,
            SpvDecorationBinding, 1);
   if (!spvc_fetch_list(ps_resources,
            SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &list, &list_num))
      goto spvc_error;
   if (list_num)
      spvc_compiler_set_decoration(ps_compiler, list[0].id,
            SpvDecorationBinding, 1);

   if (   spvc_compiler_create_compiler_options(vs_compiler,
            &vs_options) != SPVC_SUCCESS
       || spvc_compiler_create_compiler_options(ps_compiler,
            &ps_options) != SPVC_SUCCESS)
      goto spvc_error;

   switch (dst_type)
   {
      case RARCH_SHADER_HLSL:
      case RARCH_SHADER_CG:
         spvc_compiler_options_set_uint(vs_options,
               SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, version);
         spvc_compiler_options_set_uint(ps_options,
               SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, version);
         break;
      case RARCH_SHADER_METAL:
         spvc_compiler_options_set_uint(vs_options,
               SPVC_COMPILER_OPTION_MSL_VERSION, version);
         spvc_compiler_options_set_uint(ps_options,
               SPVC_COMPILER_OPTION_MSL_VERSION, version);
         break;
      default:
         spvc_compiler_options_set_uint(vs_options,
               SPVC_COMPILER_OPTION_GLSL_VERSION, version);
         spvc_compiler_options_set_uint(ps_options,
               SPVC_COMPILER_OPTION_GLSL_VERSION, version);
         break;
   }

   if (   spvc_compiler_install_compiler_options(vs_compiler,
            vs_options) != SPVC_SUCCESS
       || spvc_compiler_install_compiler_options(ps_compiler,
            ps_options) != SPVC_SUCCESS)
      goto spvc_error;

   if (dst_type == RARCH_SHADER_METAL)
   {
      /* Explicit 1:1 binding remaps, as the C++ implementation set up
       * via MSLResourceBinding.  The compiler and its resource list
       * pairs are (vs, vs_resources) and (ps, ps_resources). */
      static const spvc_resource_type remap_types[] = {
         SPVC_RESOURCE_TYPE_PUSH_CONSTANT,
         SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,
         SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
      };
      unsigned c, t;
      for (c = 0; c < 2; c++)
      {
         spvc_compiler comp  = c ? ps_compiler : vs_compiler;
         spvc_resources res  = c ? ps_resources : vs_resources;
         for (t = 0; t < 3; t++)
         {
            size_t r;
            if (!spvc_fetch_list(res, remap_types[t], &list, &list_num))
               goto spvc_error;
            for (r = 0; r < list_num; r++)
            {
               spvc_msl_resource_binding binding;
               unsigned msl_binding = spvc_compiler_get_decoration(comp,
                     list[r].id, SpvDecorationBinding);
               spvc_msl_resource_binding_init(&binding);
               binding.stage = spvc_compiler_get_execution_model(comp);
               if (remap_types[t] == SPVC_RESOURCE_TYPE_PUSH_CONSTANT)
               {
                  binding.desc_set    = SPVC_MSL_PUSH_CONSTANT_DESC_SET;
                  binding.binding     = SPVC_MSL_PUSH_CONSTANT_BINDING;
                  /* Use earlier decoration override. */
                  binding.msl_buffer  = msl_binding;
                  binding.msl_texture = 0;
                  binding.msl_sampler = 0;
               }
               else
               {
                  binding.desc_set    = spvc_compiler_get_decoration(comp,
                        list[r].id, SpvDecorationDescriptorSet);
                  binding.binding     = msl_binding;
                  binding.msl_buffer  = msl_binding;
                  binding.msl_texture = msl_binding;
                  binding.msl_sampler = msl_binding;
               }
               if (spvc_compiler_msl_add_resource_binding(comp,
                        &binding) != SPVC_SUCCESS)
                  goto spvc_error;
            }
         }
      }
   }

   if (   spvc_compiler_compile(vs_compiler, &vs_code) != SPVC_SUCCESS
       || spvc_compiler_compile(ps_compiler, &ps_code) != SPVC_SUCCESS)
      goto spvc_error;

   pass->source.string.vertex   = strdup(vs_code);
   pass->source.string.fragment = strdup(ps_code);
   if (!pass->source.string.vertex || !pass->source.string.fragment)
      goto error;

   if (!slang_process_reflection(
             vs_compiler, ps_compiler,
             vs_resources, ps_resources, shader_info, pass_number,
             semantics_map, out))
      goto error;

   /* The context owns the IRs, compilers, options, resource lists and
    * compiled source strings (already strdup'd above). */
   spvc_context_destroy(ctx);
   glslang_output_free(&output);

   return true;

spvc_error:
   RARCH_ERR("[Slang] SPIRV-Cross: %s.\n",
         spvc_context_get_last_error_string(ctx));

error:
   free(pass->source.string.vertex);
   free(pass->source.string.fragment);

   pass->source.string.vertex   = NULL;
   pass->source.string.fragment = NULL;

   spvc_context_destroy(ctx);
   glslang_output_free(&output);

   return false;
}

static bool set_ubo_texture_offset(
      slang_reflection *reflection,
      enum slang_texture_semantic semantic,
      unsigned index,
      size_t offset, bool push_constant)
{
   slang_texture_semantic_meta *sem;
   bool   *active;
   size_t *active_offset;

   if (!slang_texture_sem_array_resize_min(
            &reflection->semantic_textures[semantic], index + 1))
      return false;
   /* Resolve after the resize: it may reallocate the array. */
   sem           = &reflection->semantic_textures[semantic].data[index];
   active        = push_constant ? &sem->push_constant : &sem->uniform;
   active_offset = push_constant ? &sem->push_constant_offset : &sem->ubo_offset;

   if (*active)
   {
      if (*active_offset != offset)
      {
         RARCH_ERR("[Slang] Vertex and fragment have"
               " different offsets for same semantic %s #%u (%u vs. %u).\n",
               texture_semantic_uniform_names[semantic],
               index,
               (unsigned)*active_offset,
               (unsigned)(offset));
         return false;
      }
   }

   *active        = true;
   *active_offset = offset;
   return true;
}

static bool set_ubo_float_parameter_offset(
      slang_reflection *reflection,
      unsigned index, size_t offset,
      unsigned num_components,
      bool push_constant)
{
   slang_semantic_meta *sem;
   bool   *active;
   size_t *active_offset;

   if (!slang_float_params_resize_min(reflection, index + 1))
      return false;
   /* Resolve after the resize: it may reallocate the array. */
   sem           = &reflection->semantic_float_parameters[index];
   active        = push_constant ? &sem->push_constant : &sem->uniform;
   active_offset = push_constant ? &sem->push_constant_offset : &sem->ubo_offset;

   if (*active)
   {
      if (*active_offset != offset)
      {
         RARCH_ERR("[Slang] Vertex and fragment have different"
               " offsets for same parameter #%u (%u vs. %u).\n",
               index,
               (unsigned)*active_offset,
               (unsigned)(offset));
         return false;
      }
   }

   if (  (sem->num_components != num_components) &&
         (sem->uniform || sem->push_constant))
   {
      RARCH_ERR("[Slang] Vertex and fragment have different "
            "components for same parameter #%u (%u vs. %u).\n",
            index,
            (unsigned)sem->num_components,
            (unsigned)(num_components));
      return false;
   }

   *active             = true;
   *active_offset      = offset;
   sem->num_components = num_components;
   return true;
}

static bool set_ubo_offset(
      slang_reflection *reflection,
      enum slang_semantic semantic,
      size_t offset, unsigned num_components, bool push_constant)
{
   slang_semantic_meta *sem = &reflection->semantics[semantic];
   bool   *active        = push_constant ? &sem->push_constant : &sem->uniform;
   size_t *active_offset = push_constant ? &sem->push_constant_offset : &sem->ubo_offset;

   if (*active)
   {
      if (*active_offset != offset)
      {
         RARCH_ERR("[Slang] Vertex and fragment have "
               "different offsets for same semantic %s (%u vs. %u).\n",
               semantic_uniform_names[semantic],
               (unsigned)*active_offset,
               (unsigned)(offset));
         return false;
      }
   }

   if (  (sem->num_components != num_components) &&
         (sem->uniform || sem->push_constant))
   {
      RARCH_ERR("[Slang] Vertex and fragment have different"
            " components for same semantic %s (%u vs. %u).\n",
            semantic_uniform_names[semantic],
            (unsigned)sem->num_components,
            (unsigned)(num_components));
      return false;
   }

   *active             = true;
   *active_offset      = offset;
   sem->num_components = num_components;
   return true;
}

static bool validate_type_for_semantic(spvc_type type, enum slang_semantic sem)
{
   if (spvc_type_get_num_array_dimensions(type) != 0)
      return false;
   if (     spvc_type_get_basetype(type) != SPVC_BASETYPE_FP32
         && spvc_type_get_basetype(type) != SPVC_BASETYPE_INT32
         && spvc_type_get_basetype(type) != SPVC_BASETYPE_UINT32)
      return false;

   switch (sem)
   {
         /* mat4 */
      case SLANG_SEMANTIC_MVP:
         return
               spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            && spvc_type_get_vector_size(type)  == 4
            && spvc_type_get_columns(type)  == 4;
         /* uint */
      case SLANG_SEMANTIC_FRAME_COUNT:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* int */
      case SLANG_SEMANTIC_TOTAL_SUBFRAMES:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* int */
      case SLANG_SEMANTIC_CURRENT_SUBFRAME:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* int */
      case SLANG_SEMANTIC_FRAME_DIRECTION:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_INT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* uint */
      case SLANG_SEMANTIC_FRAME_TIME_DELTA:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* uint */
      case SLANG_SEMANTIC_ORIGINAL_FPS:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* uint */
      case SLANG_SEMANTIC_ROTATION:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_CORE_ASPECT:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_CORE_ASPECT_ROT:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* vec3 - sensor uniforms */
      case SLANG_SEMANTIC_GYROSCOPE:
      case SLANG_SEMANTIC_ACCELEROMETER:
      case SLANG_SEMANTIC_ACCELEROMETER_REST:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 3
            &&  spvc_type_get_columns(type)  == 1;
         /* float */
      case SLANG_SEMANTIC_FLOAT_PARAMETER:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_HDR:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_PAPER_WHITE_NITS:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_SCANLINES:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_SUBPIXEL_LAYOUT:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_EXPAND_GAMUT:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_UINT32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_INVERSE_TONEMAP:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
      case SLANG_SEMANTIC_HDR10:
         return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
            &&  spvc_type_get_vector_size(type)  == 1
            &&  spvc_type_get_columns(type)  == 1;
         /* vec4 */
      default:
         break;
   }
   return spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32
      &&  spvc_type_get_vector_size(type)  == 4
      &&  spvc_type_get_columns(type)  == 1;
}

static bool validate_type_for_texture_semantic(spvc_type type)
{
   return    (spvc_type_get_num_array_dimensions(type) == 0)
          && (spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32)
          && (spvc_type_get_vector_size(type) == 4)
          && (spvc_type_get_columns(type) == 1);
}

/* Validate that a texture semantic's array index falls inside the
 * range admitted by the host filter chain.  The shader source is
 * untrusted (preset packs are downloadable / shipped third-party),
 * and the index suffix in arrayed semantic names like
 * `OriginalHistory42` / `PassFeedback9` / `User7` is parsed via
 * strtoul in slang_name_to_texture_semantic_array() with no upper
 * bound.  The downstream resize_minimum() call in
 * set_ubo_texture_offset() and the direct-binding loop below would
 * otherwise grow reflection->semantic_textures[sem] to the index+1
 * the shader requested -- which on a malicious preset can be near
 * UINT32_MAX, producing an unhandled std::bad_alloc that terminates
 * RetroArch.  PASS_OUTPUT was already bounded against
 * reflection->pass_number; extend the same defensive shape to the
 * other arrayed semantics with their natural caps.  USER is the
 * lookup name for LUTs, ORIGINAL_HISTORY is bounded by the per-
 * frame ring used by init_history(), and PASS_FEEDBACK is bounded
 * by the maximum number of passes the chain can hold. */
static bool validate_texture_semantic_index(slang_reflection *reflection,
      enum slang_texture_semantic tex_sem, unsigned index)
{
   unsigned cap = 0;
   const char *cap_label = NULL;

   switch (tex_sem)
   {
      case SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT:
         cap       = reflection->pass_number;
         cap_label = "preceding passes";
         break;
      case SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK:
         cap       = GFX_MAX_SHADERS;
         cap_label = "GFX_MAX_SHADERS";
         break;
      case SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY:
         cap       = GFX_MAX_FRAME_HISTORY;
         cap_label = "GFX_MAX_FRAME_HISTORY";
         break;
      case SLANG_TEXTURE_SEMANTIC_USER:
         cap       = GFX_MAX_TEXTURES;
         cap_label = "GFX_MAX_TEXTURES";
         break;
      default:
         /* Non-arrayed semantics (Original, Source) -- index is
          * always 0 by construction in slang_name_to_texture_
          * semantic_array(). */
         return true;
   }

   if (index >= cap)
   {
      if (tex_sem == SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT)
         RARCH_ERR("[Slang] Non causal filter chain detected. "
               "Shader is trying to use output from pass #%u,"
               " but this shader is pass #%u.\n",
               index, reflection->pass_number);
      else
         RARCH_ERR("[Slang] Texture semantic %s index #%u exceeds"
               " bound (%s = %u).\n",
               texture_semantic_names[tex_sem],
               index, cap_label, cap);
      return false;
   }
   return true;
}

static bool add_active_buffer_ranges(
      spvc_compiler compiler,
      const spvc_reflected_resource *resource,
      slang_reflection *reflection,
      bool push_constant)
{
   size_t i;
   size_t num_ranges               = 0;
   const spvc_buffer_range *ranges = NULL;
   spvc_type base_type             =
      spvc_compiler_get_type_handle(compiler, resource->base_type_id);

   /* Get which uniforms are actually in use by this shader. */
   if (spvc_compiler_get_active_buffer_ranges(compiler,
            resource->id, &ranges, &num_ranges) != SPVC_SUCCESS)
      return false;

   for (i = 0; i < num_ranges; i++)
   {
      unsigned sem_index             = 0;
      unsigned tex_sem_index         = 0;
      const char *name               = spvc_compiler_get_member_name(
            compiler, resource->base_type_id, ranges[i].index);
      spvc_type type                 = spvc_compiler_get_type_handle(
            compiler,
            spvc_type_get_member_type(base_type, ranges[i].index));
      enum slang_semantic sem             = slang_uniform_name_to_semantic(
            reflection->semantic_map, name, &sem_index);
      enum slang_texture_semantic tex_sem = slang_uniform_name_to_texture_semantic(
            reflection->texture_semantic_uniform_map,
            name, &tex_sem_index);

      if (tex_sem != SLANG_INVALID_TEXTURE_SEMANTIC &&
            !validate_texture_semantic_index(reflection,
                  tex_sem, tex_sem_index))
         return false;

      if (sem != SLANG_INVALID_SEMANTIC)
      {
         if (!validate_type_for_semantic(type, sem))
         {
            RARCH_ERR("[Slang] Underlying type of semantic is invalid.\n");
            return false;
         }

         switch (sem)
         {
            case SLANG_SEMANTIC_FLOAT_PARAMETER:
               if (!set_ubo_float_parameter_offset(reflection, sem_index,
                        ranges[i].offset, spvc_type_get_vector_size(type),
                        push_constant))
                  return false;
               break;

            default:
               if (!set_ubo_offset(reflection, sem,
                        ranges[i].offset,
                        spvc_type_get_vector_size(type)
                        * spvc_type_get_columns(type), push_constant))
                  return false;
               break;
         }
      }
      else if (tex_sem != SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         if (!validate_type_for_texture_semantic(type))
         {
            RARCH_ERR("[Slang] Underlying type of texture"
                  " semantic is invalid.\n");
            return false;
         }

         if (!set_ubo_texture_offset(reflection, tex_sem, tex_sem_index,
                  ranges[i].offset, push_constant))
            return false;
      }
      else
      {
         RARCH_ERR("[Slang] Unknown semantic found: \"%s\".\n", name);
         return false;
      }
   }
   return true;
}


static bool slang_reflect(
      spvc_compiler vertex_compiler, spvc_compiler fragment_compiler,
      spvc_resources vertex_resources, spvc_resources fragment_resources,
      slang_reflection *reflection)
{
   uint32_t location_mask = 0;
   uint32_t binding_mask  = 0;
   unsigned i             = 0;
   const spvc_reflected_resource *v_inputs, *v_ubos, *v_pushes, *v_imgs;
   const spvc_reflected_resource *v_sbos, *v_subs, *v_simgs, *v_atomics;
   const spvc_reflected_resource *f_outputs, *f_ubos, *f_pushes, *f_imgs;
   const spvc_reflected_resource *f_sbos, *f_subs, *f_simgs, *f_atomics;
   size_t v_num_inputs, v_num_ubos, v_num_pushes, v_num_imgs;
   size_t v_num_sbos, v_num_subs, v_num_simgs, v_num_atomics;
   size_t f_num_outputs, f_num_ubos, f_num_pushes, f_num_imgs;
   size_t f_num_sbos, f_num_subs, f_num_simgs, f_num_atomics;
   uint32_t vertex_ubo, fragment_ubo, vertex_push, fragment_push;
   unsigned vertex_ubo_binding, fragment_ubo_binding, ubo_binding;
   bool has_ubo;

   if (   !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_STAGE_INPUT, &v_inputs, &v_num_inputs)
       || !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &v_ubos, &v_num_ubos)
       || !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &v_pushes, &v_num_pushes)
       || !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &v_imgs, &v_num_imgs)
       || !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &v_sbos, &v_num_sbos)
       || !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_SUBPASS_INPUT, &v_subs, &v_num_subs)
       || !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &v_simgs, &v_num_simgs)
       || !spvc_fetch_list(vertex_resources,
            SPVC_RESOURCE_TYPE_ATOMIC_COUNTER, &v_atomics, &v_num_atomics)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &f_outputs, &f_num_outputs)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &f_ubos, &f_num_ubos)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &f_pushes, &f_num_pushes)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &f_imgs, &f_num_imgs)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &f_sbos, &f_num_sbos)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_SUBPASS_INPUT, &f_subs, &f_num_subs)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &f_simgs, &f_num_simgs)
       || !spvc_fetch_list(fragment_resources,
            SPVC_RESOURCE_TYPE_ATOMIC_COUNTER, &f_atomics, &f_num_atomics))
   {
      RARCH_ERR("[Slang] Failed to enumerate shader resources.\n");
      return false;
   }

   /* Validate use of unexpected types. */
   if (     v_num_imgs || v_num_sbos || v_num_subs
         || v_num_simgs || v_num_atomics
         || f_num_sbos || f_num_subs || f_num_simgs || f_num_atomics)
   {
      RARCH_ERR("[Slang] Invalid resource type detected.\n");
      return false;
   }

   /* Validate vertex input. */
   if (v_num_inputs != 2)
   {
      RARCH_ERR("[Slang] Vertex must have two attributes.\n");
      return false;
   }

   if (f_num_outputs != 1)
   {
      RARCH_ERR("[Slang] Multiple render targets not supported.\n");
      return false;
   }

   if (spvc_compiler_get_decoration(fragment_compiler,
            f_outputs[0].id, SpvDecorationLocation) != 0)
   {
      RARCH_ERR("[Slang] Render target must use location = 0.\n");
      return false;
   }

   for (i = 0; i < v_num_inputs; i++)
      location_mask |= 1 << spvc_compiler_get_decoration(vertex_compiler,
            v_inputs[i].id, SpvDecorationLocation);

   if (location_mask != 0x3)
   {
      RARCH_ERR("[Slang] The two vertex attributes do not"
            " use location = 0 and location = 1.\n");
      return false;
   }

   /* Validate the single uniform buffer. */
   if (v_num_ubos > 1)
   {
      RARCH_ERR("[Slang] Vertex must use zero or one uniform buffer.\n");
      return false;
   }

   if (f_num_ubos > 1)
   {
      RARCH_ERR("[Slang] Fragment must use zero or one uniform buffer.\n");
      return false;
   }

   /* Validate the single push constant buffer. */
   if (v_num_pushes > 1)
   {
      RARCH_ERR("[Slang] Vertex must use zero or one push constant buffers.\n");
      return false;
   }

   if (f_num_pushes > 1)
   {
      RARCH_ERR("[Slang] Fragment must use zero or one push constant buffer.\n");
      return false;
   }

   vertex_ubo    = v_num_ubos   ? (uint32_t)v_ubos[0].id   : 0;
   fragment_ubo  = f_num_ubos   ? (uint32_t)f_ubos[0].id   : 0;
   vertex_push   = v_num_pushes ? (uint32_t)v_pushes[0].id : 0;
   fragment_push = f_num_pushes ? (uint32_t)f_pushes[0].id : 0;

   if (vertex_ubo &&
         spvc_compiler_get_decoration(vertex_compiler,
            vertex_ubo, SpvDecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[Slang] Resources must use descriptor set #0.\n");
      return false;
   }

   if (fragment_ubo &&
         spvc_compiler_get_decoration(fragment_compiler,
            fragment_ubo, SpvDecorationDescriptorSet) != 0)
   {
      RARCH_ERR("[Slang] Resources must use descriptor set #0.\n");
      return false;
   }

   vertex_ubo_binding   = vertex_ubo
      ? spvc_compiler_get_decoration(vertex_compiler,
            vertex_ubo, SpvDecorationBinding)
      : (unsigned)-1;
   fragment_ubo_binding = fragment_ubo
      ? spvc_compiler_get_decoration(fragment_compiler,
            fragment_ubo, SpvDecorationBinding)
      : (unsigned)-1;
   has_ubo              = vertex_ubo || fragment_ubo;

   if (  (vertex_ubo_binding   != (unsigned)-1) &&
         (fragment_ubo_binding != (unsigned)-1) &&
         (vertex_ubo_binding   != fragment_ubo_binding))
   {
      RARCH_ERR("[Slang] Vertex and fragment uniform buffer must have same binding.\n");
      return false;
   }

   ubo_binding = (vertex_ubo_binding != (unsigned)-1)
      ? vertex_ubo_binding
      : fragment_ubo_binding;

   if (has_ubo && ubo_binding >= SLANG_NUM_BINDINGS)
   {
      RARCH_ERR("[Slang] Binding %u is out of range.\n", ubo_binding);
      return false;
   }

   reflection->ubo_binding              = has_ubo ? ubo_binding : 0;
   reflection->ubo_stage_mask           = 0;
   reflection->ubo_size                 = 0;
   reflection->push_constant_size       = 0;
   reflection->push_constant_stage_mask = 0;

   if (vertex_ubo)
   {
      size_t _y = 0;
      reflection->ubo_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      if (spvc_compiler_get_declared_struct_size(vertex_compiler,
               spvc_compiler_get_type_handle(vertex_compiler,
                  v_ubos[0].base_type_id), &_y) != SPVC_SUCCESS)
         return false;
      reflection->ubo_size        = MAX(reflection->ubo_size, _y);
   }

   if (fragment_ubo)
   {
      size_t _y = 0;
      reflection->ubo_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      if (spvc_compiler_get_declared_struct_size(fragment_compiler,
               spvc_compiler_get_type_handle(fragment_compiler,
                  f_ubos[0].base_type_id), &_y) != SPVC_SUCCESS)
         return false;
      reflection->ubo_size        = MAX(reflection->ubo_size, _y);
   }

   if (vertex_push)
   {
      size_t _y = 0;
      reflection->push_constant_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      if (spvc_compiler_get_declared_struct_size(vertex_compiler,
               spvc_compiler_get_type_handle(vertex_compiler,
                  v_pushes[0].base_type_id), &_y) != SPVC_SUCCESS)
         return false;
      reflection->push_constant_size        = MAX(
            reflection->push_constant_size, _y);
   }

   if (fragment_push)
   {
      size_t _y = 0;
      reflection->push_constant_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      if (spvc_compiler_get_declared_struct_size(fragment_compiler,
               spvc_compiler_get_type_handle(fragment_compiler,
                  f_pushes[0].base_type_id), &_y) != SPVC_SUCCESS)
         return false;
      reflection->push_constant_size        = MAX(
            reflection->push_constant_size, _y);
   }

   /* Validate push constant size against Vulkan's
    * minimum spec to avoid cross-vendor issues. */
   if (reflection->push_constant_size > 128)
   {
      RARCH_ERR("[Slang] Exceeded maximum size of 128 bytes"
            " for push constant buffer.\n");
      return false;
   }

   /* Find all relevant uniforms and push constants. */
   if (vertex_ubo && !add_active_buffer_ranges(vertex_compiler,
            &v_ubos[0], reflection, false))
      return false;
   if (fragment_ubo && !add_active_buffer_ranges(fragment_compiler,
            &f_ubos[0], reflection, false))
      return false;
   if (vertex_push && !add_active_buffer_ranges(vertex_compiler,
            &v_pushes[0], reflection, true))
      return false;
   if (fragment_push && !add_active_buffer_ranges(fragment_compiler,
            &f_pushes[0], reflection, true))
      return false;

   if (has_ubo)
      binding_mask = 1 << ubo_binding;

   /* On to textures. */
   for (i = 0; i < f_num_imgs; i++)
   {
      unsigned array_index = 0;
      unsigned set         = spvc_compiler_get_decoration(fragment_compiler,
            f_imgs[i].id, SpvDecorationDescriptorSet);
      unsigned binding     = spvc_compiler_get_decoration(fragment_compiler,
            f_imgs[i].id, SpvDecorationBinding);
      enum slang_texture_semantic index;

      if (set != 0)
      {
         RARCH_ERR("[Slang] Resources must use descriptor set #0.\n");
         return false;
      }

      if (binding >= SLANG_NUM_BINDINGS)
      {
         RARCH_ERR("[Slang] Binding %u is out of range.\n", ubo_binding);
         return false;
      }

      if (binding_mask & (1 << binding))
      {
         RARCH_ERR("[Slang] Binding %u is already in use.\n", binding);
         return false;
      }
      binding_mask |= 1 << binding;

      index = slang_name_to_texture_semantic(
            reflection->texture_semantic_map,
            f_imgs[i].name, &array_index);

      if (index == SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         RARCH_ERR("[Slang] Texture name \"%s\" not found in semantic map, "
                   "Probably the texture name or pass alias is not defined "
                   "in the preset (Non-semantic textures not supported yet)\n",
                   f_imgs[i].name);
         return false;
      }

      if (!validate_texture_semantic_index(reflection, index, array_index))
         return false;

      if (!slang_texture_sem_array_resize_min(
               &reflection->semantic_textures[index], array_index + 1))
         return false;
      {
         slang_texture_semantic_meta *m =
            &reflection->semantic_textures[index].data[array_index];
         m->binding    = binding;
         m->stage_mask = SLANG_STAGE_FRAGMENT_MASK;
         m->texture    = true;
      }
   }

#ifdef DEBUG
   RARCH_LOG("[Slang] Reflection\n");
   RARCH_LOG("[Slang]   Textures:\n");

   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      unsigned index = 0;
      unsigned j;
      for (j = 0; j < reflection->semantic_textures[i].size; j++)
      {
         const slang_texture_semantic_meta *sem = &reflection->semantic_textures[i].data[j];
         if (sem->texture)
            RARCH_LOG("[Slang]      %s (#%u)\n",
                  texture_semantic_names[i], index);
         index++;
      }
   }

   RARCH_LOG("[Slang]   Uniforms (Vertex: %s, Fragment: %s):\n",
         reflection->ubo_stage_mask & SLANG_STAGE_VERTEX_MASK ? "yes": "no",
         reflection->ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK ? "yes": "no");
   RARCH_LOG("[Slang]   Push Constants (Vertex: %s, Fragment: %s):\n",
         reflection->push_constant_stage_mask & SLANG_STAGE_VERTEX_MASK ? "yes": "no",
         reflection->push_constant_stage_mask & SLANG_STAGE_FRAGMENT_MASK ? "yes": "no");

   for (i = 0; i < SLANG_NUM_SEMANTICS; i++)
   {
      if (reflection->semantics[i].uniform)
      {
         RARCH_LOG("[Slang]      %s (Offset: %u)\n",
               semantic_uniform_names[i],
               (unsigned)(reflection->semantics[i].ubo_offset));
      }

      if (reflection->semantics[i].push_constant)
      {
         RARCH_LOG("[Slang]      %s (PushOffset: %u)\n",
               semantic_uniform_names[i],
               (unsigned)(reflection->semantics[i].push_constant_offset));
      }
   }

   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      unsigned index = 0;
      unsigned j;
      for (j = 0; j < reflection->semantic_textures[i].size; j++)
      {
         const slang_texture_semantic_meta *sem = &reflection->semantic_textures[i].data[j];
         if (sem->uniform)
         {
            RARCH_LOG("[Slang]      %s (#%u) (Offset: %u)\n",
                  texture_semantic_uniform_names[i],
                  index,
                  (unsigned)sem->ubo_offset);
         }

         if (sem->push_constant)
         {
            RARCH_LOG("[Slang]      %s (#%u) (PushOffset: %u)\n",
                  texture_semantic_uniform_names[i],
                  index,
                  (unsigned)sem->push_constant_offset);
         }
         index++;
      }
   }

   RARCH_LOG("[Slang]   Parameters:\n");

   for (i = 0; i < reflection->num_float_parameters; i++)
   {
      const slang_semantic_meta *param = &reflection->semantic_float_parameters[i];

      if (param->uniform)
         RARCH_LOG("[Slang]     #%u (Offset: %u)\n", i,
               (unsigned int)param->ubo_offset);
      if (param->push_constant)
         RARCH_LOG("[Slang]     #%u (PushOffset: %u)\n", i,
               (unsigned int)param->push_constant_offset);
   }
#endif

   return true;
}

bool slang_reflect_spirv(const uint32_t *vertex, size_t vertex_len,
      const uint32_t *fragment, size_t fragment_len,
      slang_reflection *reflection)
{
   spvc_context ctx           = NULL;
   spvc_parsed_ir vs_ir       = NULL;
   spvc_parsed_ir ps_ir       = NULL;
   spvc_compiler vs_compiler  = NULL;
   spvc_compiler ps_compiler  = NULL;
   spvc_resources vs_res      = NULL;
   spvc_resources ps_res      = NULL;
   bool ret                   = false;

   if (spvc_context_create(&ctx) != SPVC_SUCCESS)
      return false;

   if (   spvc_context_parse_spirv(ctx, (const SpvId*)vertex, vertex_len,
            &vs_ir) != SPVC_SUCCESS
       || spvc_context_parse_spirv(ctx, (const SpvId*)fragment, fragment_len,
            &ps_ir) != SPVC_SUCCESS
       || spvc_context_create_compiler(ctx, SPVC_BACKEND_NONE, vs_ir,
            SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &vs_compiler) != SPVC_SUCCESS
       || spvc_context_create_compiler(ctx, SPVC_BACKEND_NONE, ps_ir,
            SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &ps_compiler) != SPVC_SUCCESS
       || spvc_compiler_create_shader_resources(vs_compiler,
            &vs_res) != SPVC_SUCCESS
       || spvc_compiler_create_shader_resources(ps_compiler,
            &ps_res) != SPVC_SUCCESS)
   {
      RARCH_ERR("[Slang] SPIRV-Cross: %s.\n",
            spvc_context_get_last_error_string(ctx));
      goto out;
   }

   if (!slang_reflect(vs_compiler, ps_compiler, vs_res, ps_res, reflection))
   {
      RARCH_ERR("[Slang] Failed to reflect SPIR-V."
            " Resource usage is inconsistent with expectations.\n");
      goto out;
   }

   ret = true;

out:
   /* The context owns the IRs, compilers and resource lists. */
   spvc_context_destroy(ctx);
   return ret;
}
