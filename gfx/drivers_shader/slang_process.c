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
/* Names the reflection table below needs in every build, with or
 * without the compiler. */
#include "slang_rect.h"
#include "slang_blob.h"
#include "slang_process.h"
#if defined(HAVE_GLSLANG) || defined(HAVE_RSLC)
#include "slang_cache.h"
#endif
#if defined(HAVE_RSLC)
#include <gfx/shaders/rslc.h>
#endif
#if !defined(SLANG_RSLC_ONLY)
/* The SPIRV-Cross C header ends its enumerator lists with a
 * comma, which the C89 lane rejects under -pedantic. */
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

/* A pass's two stages, parsed by SPIRV-Cross */
struct slang_spvc_handles
{
   spvc_compiler  vs;
   spvc_compiler  ps;
   spvc_resources vs_res;
   spvc_resources ps_res;
};
#else
/* Without SPIRV-Cross every stage is reflected from rslc's tables */
struct slang_spvc_handles;
#endif


#include "../video_defines.h"
#include "../../verbosity.h"

/* The first pass's SourceSize while the preset carries a simulated
 * size. A pass's uniforms keep its address after the semantics they
 * were reflected into are copied and freed, so it lives here. */
static float slang_simulated_size[4];

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
   "AccelerometerRest",
   "SwapCount",
   SLANG_RECT_NAME_RECT,
   SLANG_RECT_NAME_CLAMP,
   SLANG_RECT_NAME_TEXELS,
   "PaperWhiteNits",
   "PeakNits",
   "MaxNits",
   "EnableHDR",
   "MinNits",
   "TonemapOperator",
   "SourceHDR",
   "Passthrough"
};

#if !defined(SLANG_RSLC_ONLY)
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
#endif

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

/* Whether both stages of @output carry the compiler's tables, so the
 * pass reflects without a SPIR-V parse. */
static bool slang_output_has_tables(const glslang_output *output)
{
#if defined(HAVE_RSLC)
   return output->vertex_blob && output->fragment_blob
      && output->vertex_blob->refl && output->fragment_blob->refl;
#else
   (void)output;
   return false;
#endif
}

/* @spvc is read only when @output carries no tables. */
static bool slang_process_reflection(
      const struct slang_spvc_handles *spvc,
      const glslang_output*  output,
      struct video_shader*   shader_info,
      unsigned               pass_number,
      const semantics_map_t* map,
      pass_semantics_t*      out)
{
   static const float unknown_min_nits = 0.0f;
   static const uint32_t rational_tonemap_operator = 0;
   static const uint32_t non_pq_source_hdr = 0;
   static const uint32_t no_hdr_ownership = 0;
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

   if (slang_output_has_tables(output)
#if defined(HAVE_RSLC)
         ? !slang_reflect_blobs(output->vertex_blob, output->fragment_blob,
            &sl_reflection)
#else
         ? true
#endif
#if !defined(SLANG_RSLC_ONLY)
         : (!spvc || !slang_reflect(spvc->vs, spvc->ps,
            spvc->vs_res, spvc->ps_res, &sl_reflection)))
#else
         : (!spvc || true))
#endif
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

         uniform.data  = map->uniforms[semantic];
         uniform.param = NULL;
         if (_semantic == SLANG_SEMANTIC_MIN_NITS && !uniform.data)
            uniform.data = (void*)&unknown_min_nits;
         if (_semantic == SLANG_SEMANTIC_TONEMAP_OPERATOR && !uniform.data)
            uniform.data = (void*)&rational_tonemap_operator;
         if (_semantic == SLANG_SEMANTIC_SOURCE_HDR && !uniform.data)
            uniform.data = (void*)&non_pq_source_hdr;
         if (_semantic == SLANG_SEMANTIC_PASSTHROUGH && !uniform.data)
            uniform.data = (void*)&no_hdr_ownership;
         /* A backend that has not wired a source for this semantic leaves
          * the slot NULL; the shader then reads its zero-initialised
          * default rather than the upload copying from NULL. */
         if (!uniform.data)
            continue;
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

         uniform.data  = NULL;
         uniform.param = &shader_info->parameters[i];
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
            unsigned sim = 0;

            if (     semantic == (int)SLANG_TEXTURE_SEMANTIC_SOURCE
                  && pass_number == 0)
               sim = shader_info->sim_source_dims;

            if (sim)
            {
               slang_simulated_size[0] = (float)VIDEO_SCALE_W(sim);
               slang_simulated_size[1] = (float)VIDEO_SCALE_H(sim);
               slang_simulated_size[2] = 1.0f / slang_simulated_size[0];
               slang_simulated_size[3] = 1.0f / slang_simulated_size[1];
               uniform.data            = slang_simulated_size;
            }
            else
               uniform.data  = (void*)((uintptr_t)map->textures[semantic].size
                     + index * map->textures[semantic].size_stride);
            uniform.param = NULL;
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


void glslang_output_init(glslang_output *output)
{
   memset(output, 0, sizeof(*output));
   output->meta.rt_format = SLANG_FORMAT_UNKNOWN;
}

/* Gives a stage of @output to @blob: the output's reference, and the
 * word view the readers of the output use. */
void glslang_output_set_blob(glslang_output *output, bool fragment,
      struct slang_blob *blob)
{
   if (fragment)
   {
      slang_blob_unref(output->fragment_blob);
      output->fragment_blob = blob;
      output->fragment      = blob ? blob->words : NULL;
      output->fragment_len  = blob ? blob->nwords : 0;
   }
   else
   {
      slang_blob_unref(output->vertex_blob);
      output->vertex_blob = blob;
      output->vertex      = blob ? blob->words : NULL;
      output->vertex_len  = blob ? blob->nwords : 0;
   }
}

void glslang_output_free(glslang_output *output)
{
   if (!output)
      return;
   slang_blob_unref(output->vertex_blob);
   slang_blob_unref(output->fragment_blob);
   free(output->vertex_text);
   free(output->fragment_text);
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
 * Stage compilation.  A build with one compiler uses it.  A build with
 * both (rslc and the system's glslang + SPIRV-Cross) follows the
 * Shader Compiler driver setting: 1 glslang, anything else rslc, with
 * glslang behind rslc as the fallback for a stage rslc rejects.
 * ----------------------------------------------------------------------- */
#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
#include "../../configuration.h"

#define SLANG_COMPILER_GLSLANG 1

/* rslc unless the setting names glslang: one byte read per stage, the
 * path redirected, nothing copied. */
static bool slang_prefer_rslc(void)
{
   const settings_t *settings = config_get_ptr();
   return !settings
      || settings->uints.video_shader_compiler != SLANG_COMPILER_GLSLANG;
}
#endif

#if defined(HAVE_RSLC)
/* While a deferred load holds the pool, its compiles - on the compile
 * tasks, several at once, and on the chain's thread - reuse rslc
 * arenas from it instead of allocating each stage's arena chunk by
 * chunk; a block lives in its slot until the last holder lets go. */
#define SLANG_ARENAS    8
#define SLANG_ARENA_LEN (4u * 1024u * 1024u)

struct slang_arena
{
   void              *block;
   size_t             len;
   retro_atomic_int_t held;
};

static struct slang_arena slang_arenas[SLANG_ARENAS];
static retro_atomic_int_t slang_arena_holders;

static struct slang_arena *slang_arena_take(void)
{
   unsigned i;
   if (!retro_atomic_load_acquire_int(&slang_arena_holders))
      return NULL;
   for (i = 0; i < SLANG_ARENAS; i++)
   {
      struct slang_arena *a = &slang_arenas[i];
      if (!retro_atomic_cas_int(&a->held, 0, 1))
         continue;
      if (!a->block && (a->block = malloc(SLANG_ARENA_LEN)))
         a->len = SLANG_ARENA_LEN;
      if (a->block)
         return a;
      retro_atomic_store_release_int(&a->held, 0);
      return NULL;
   }
   return NULL;
}

static void slang_arena_give(struct slang_arena *a)
{
   /* the pool let go while this was out: nothing reuses it */
   if (!retro_atomic_load_acquire_int(&slang_arena_holders))
   {
      free(a->block);
      a->block = NULL;
      a->len   = 0;
   }
   retro_atomic_store_release_int(&a->held, 0);
}

/* A larger block for the stages to come, after one outgrew it */
static void slang_arena_grow(struct slang_arena *a)
{
   size_t len = a->len * 2;
   void  *block;
   if (len > RSLC_ARENA_CAP_DEFAULT || !(block = malloc(len)))
      return;
   free(a->block);
   a->block = block;
   a->len   = len;
}

void slang_arena_pool_hold(void)
{
   retro_atomic_fetch_add_int(&slang_arena_holders, 1);
}

void slang_arena_pool_drop(void)
{
   unsigned i;
   if (retro_atomic_fetch_add_int(&slang_arena_holders, -1) != 1)
      return;
   /* the blocks not out now; those out go when given back */
   for (i = 0; i < SLANG_ARENAS; i++)
   {
      struct slang_arena *a = &slang_arenas[i];
      if (!retro_atomic_cas_int(&a->held, 0, 1))
         continue;
      free(a->block);
      a->block = NULL;
      a->len   = 0;
      retro_atomic_store_release_int(&a->held, 0);
   }
}

/* rslc_compile, or rslc_compile_spirv with @words, in a pooled arena
 * when there is one to be had.  A stage the block cannot hold - or
 * one that fails - is compiled again on the allocator, whose result
 * and message stand, and the block grows for the stages to come. */
static bool slang_rslc_run(const char *source, const uint32_t *words,
      size_t len, enum rslc_stage stage, struct rslc_opts *opts,
      struct rslc_result *res)
{
   struct slang_arena *a = slang_arena_take();
   bool ok;
   if (a)
   {
      opts->arena     = a->block;
      opts->arena_len = a->len;
      ok = words
         ? rslc_compile_spirv(words, len, stage, opts, res)
         : rslc_compile(source, len, stage, opts, res);
      opts->arena     = NULL;
      opts->arena_len = 0;
      if (ok)
      {
         slang_arena_give(a);
         return true;
      }
   }
   ok = words
      ? rslc_compile_spirv(words, len, stage, opts, res)
      : rslc_compile(source, len, stage, opts, res);
   if (a)
   {
      if (ok)
         slang_arena_grow(a);
      slang_arena_give(a);
   }
   return ok;
}
#else
void slang_arena_pool_hold(void) { }
void slang_arena_pool_drop(void) { }
#endif

#if defined(HAVE_RSLC)
/* rslc builds the stage in its own arena and returns one blob holding
 * the words and the reflection tables; the output adopts it whole.  On
 * a refusal @err (256 bytes) holds rslc's message. */
static bool slang_rslc_compile_stage(const char *source,
      enum glslang_compile_stage stage, struct slang_blob **blob, char *err)
{
   struct rslc_opts   opts;
   struct rslc_result res;

   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.err     = err;
   opts.err_cap = 256;
   err[0]       = '\0';

   if (!slang_rslc_run(source, NULL, strlen(source),
            stage == GLSLANG_COMPILE_STAGE_VERTEX
            ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT, &opts, &res))
      return false;
   *blob = slang_blob_adopt_result(&res, &opts);
   return *blob != NULL;
}
#endif

#if defined(HAVE_RSLC)
/* A cached module read back through rslc's ingest: the words a hit
 * serves must be a module this compiler accepts for the stage, or the
 * hit is a miss and the stage is compiled again from its GLSL.  The
 * ingest's blob - the words with their tables - replaces the bare
 * words the cache handed over, so a hit binds by the tables as a miss
 * does. */
static bool slang_rslc_cached_stage_ok(glslang_output *output, bool fragment)
{
   struct rslc_opts   opts;
   struct rslc_result res;
   struct slang_blob *blob;
   char               err[256];

   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   if (!slang_rslc_run(NULL, fragment ? output->fragment : output->vertex,
            fragment ? output->fragment_len : output->vertex_len,
            fragment ? RSLC_STAGE_FRAGMENT : RSLC_STAGE_VERTEX, &opts, &res))
   {
      RARCH_WARN("[Slang] Cached SPIR-V refused: %s\n", err);
      return false;
   }
   if (!(blob = slang_blob_adopt_result(&res, &opts)))
      return false;
   glslang_output_set_blob(output, fragment, blob);
   return true;
}
#endif

#if defined(HAVE_RSLC)
/* Prints one stage as the text a driver asked for; NULL when rslc
 * cannot (the driver then crosses the SPIR-V). */
static char *slang_rslc_print_stage(const char *source,
      enum glslang_compile_stage stage, const struct slang_text_target *t,
      const struct rslc_remap *rm)
{
   struct rslc_opts   opts;
   struct rslc_result res;
   struct rslc_names  names;
   char              *text;
   char               err[256];

   memset(&names, 0, sizeof(names));
   names.attr_prefix    = t->attr_prefix;
   names.varying_prefix = t->varying_prefix;
   names.sampler_prefix = t->sampler_prefix;
   names.ubo_block      = t->ubo_block[stage == GLSLANG_COMPILE_STAGE_VERTEX ? 0 : 1];
   names.ubo_instance   = t->ubo_instance[stage == GLSLANG_COMPILE_STAGE_VERTEX ? 0 : 1];
   names.push_block     = t->push_block[stage == GLSLANG_COMPILE_STAGE_VERTEX ? 0 : 1];
   names.push_instance  = t->push_instance[stage == GLSLANG_COMPILE_STAGE_VERTEX ? 0 : 1];
   names.flatten        = t->flatten;
   memset(&opts, 0, sizeof(opts));
   opts.dst     = t->hlsl ? RSLC_DST_HLSL : t->msl ? RSLC_DST_MSL
      : t->es ? RSLC_DST_GLSL_ES : RSLC_DST_GLSL;
   opts.version = t->version;
   opts.names   = t->hlsl || t->msl ? NULL : &names;
   opts.remap   = rm;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   if (!slang_rslc_run(source, NULL, strlen(source),
            stage == GLSLANG_COMPILE_STAGE_VERTEX
            ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT, &opts, &res))
   {
      RARCH_WARN("[Slang] No printed stage: %s\n", err);
      return NULL;
   }
   text = (char*)malloc(res.text_len + 1);
   if (text)
      memcpy(text, res.text, res.text_len + 1);
   rslc_result_free(&res, &opts);
   return text;
}

char *slang_print_stage(const char *source, bool fragment,
      const struct slang_text_target *t)
{
   return slang_rslc_print_stage(source, fragment
         ? GLSLANG_COMPILE_STAGE_FRAGMENT : GLSLANG_COMPILE_STAGE_VERTEX,
         t, NULL);
}

bool slang_compile_builtin(const char *source, bool fragment,
      const struct slang_text_target *t, struct slang_blob **blob,
      char **text)
{
   char err[256];
   enum glslang_compile_stage stage = fragment
      ? GLSLANG_COMPILE_STAGE_FRAGMENT : GLSLANG_COMPILE_STAGE_VERTEX;
   *blob = NULL;
   *text = NULL;
   if (!slang_rslc_compile_stage(source, stage, blob, err))
   {
      RARCH_ERR("[Slang] A built-in %s stage: %s\n",
            fragment ? "fragment" : "vertex", err);
      return false;
   }
   if (t && !(*text = slang_rslc_print_stage(source, stage, t, NULL)))
   {
      slang_blob_unref(*blob);
      *blob = NULL;
      return false;
   }
   return true;
}

/* Prints both stages of @output for @text, or neither: the driver takes
 * one path for a pass. */
static void slang_output_print_text(glslang_output *output,
      const char *vertex_source, const char *fragment_source,
      const struct slang_text_target *text)
{
   output->vertex_text = slang_rslc_print_stage(vertex_source,
         GLSLANG_COMPILE_STAGE_VERTEX, text, NULL);
   if (output->vertex_text)
      output->fragment_text = slang_rslc_print_stage(fragment_source,
            GLSLANG_COMPILE_STAGE_FRAGMENT, text, NULL);
   if (!output->fragment_text)
   {
      free(output->vertex_text);
      output->vertex_text = NULL;
   }
}
#endif

/* Compiles one stage of the pass at @path, whose cache key is @key;
 * *compiler names what produced the words, for the cache entry's key. */
static bool slang_compile_stage(const char *source,
      enum glslang_compile_stage stage,
      struct slang_blob **blob, const char **compiler,
      const struct slang_text_target *text, bool detached,
      const char *path, const char *key)
{
#if defined(HAVE_GLSLANG)
   uint32_t *words = NULL;
   size_t    nwords = 0;
#endif
#if !defined(HAVE_RSLC)
   (void)path;
   (void)key;
#endif
#if !defined(HAVE_RSLC) || !defined(HAVE_GLSLANG)
   (void)text;
#endif
#if defined(HAVE_RSLC)
#if defined(HAVE_GLSLANG)
   if (slang_prefer_rslc())
#endif
   {
      char err[256];
      if (slang_rslc_compile_stage(source, stage, blob, err))
      {
         *compiler = "rslc";
         return true;
      }
      /* the line a tester reports: which stage, of which file, and why;
       * a detached compile tells the user nothing */
#if defined(HAVE_GLSLANG)
      if (!detached)
         RARCH_WARN("[Slang] rslc rejected the %s stage of \"%s\" (pass %.16s): %s; compiling it with glslang.\n",
               stage == GLSLANG_COMPILE_STAGE_VERTEX ? "vertex" : "fragment",
               path, key, err);
#else
      if (!detached)
         RARCH_ERR("[Slang] rslc rejected the %s stage of \"%s\" (pass %.16s): %s\n",
               stage == GLSLANG_COMPILE_STAGE_VERTEX ? "vertex" : "fragment",
               path, key, err);
#endif
   }
#endif
#if defined(HAVE_GLSLANG)
   *compiler = "glslang";
   if (!glslang_compile_spirv(source, stage, &words, &nwords))
      return false;
   *blob = slang_blob_adopt_words(words, nwords);
   return *blob != NULL;
#else
   (void)source;
   (void)stage;
   (void)blob;
   (void)compiler;
   return false;
#endif
}


/* -----------------------------------------------------------------------
 * glslang_compile_shader — now uses shader_line_buf
 * ----------------------------------------------------------------------- */
/* Gives @output the blobs the table lists under @key, both stages or
 * neither. */
static bool slang_output_take_listed(glslang_output *output, const char *key)
{
   struct slang_blob *vs = slang_blob_table_find(key, false);
   struct slang_blob *fs = vs ? slang_blob_table_find(key, true) : NULL;
   if (!fs)
   {
      slang_blob_unref(vs);
      return false;
   }
   glslang_output_set_blob(output, false, vs);
   glslang_output_set_blob(output, true, fs);
   return true;
}

/* Lists both stages of @output under @key for the passes still to
 * come. */
static void slang_output_list(const glslang_output *output, const char *key)
{
   slang_blob_table_add(output->vertex_blob, key, false);
   slang_blob_table_add(output->fragment_blob, key, true);
}

bool glslang_compile_shader(const char *shader_path, glslang_output *output)
{
   return glslang_compile_shader_cached(shader_path, output, NULL);
}

bool glslang_compile_shader_cached(const char *shader_path,
      glslang_output *output, void *include_cache)
{
   return glslang_compile_shader_text(shader_path, output, include_cache, NULL);
}

/* The compile of one pass.  Detached, it runs on a thread that is not
 * the chain's: it reads and compiles as the chain's thread would and
 * writes the cache, but neither asks the blob table nor lists in it,
 * prints no text and tells the user nothing; @key_out (33 bytes) gets
 * the cache key of the compiler that made the stages, under which the
 * chain's thread offers them to the table (slang_blob_offer). */
static bool slang_compile_pass(const char *shader_path,
      glslang_output *output, void *include_cache,
      const struct slang_text_target *text, bool detached, char *key_out)
{
   glslang_output_init(output);

#if !defined(HAVE_RSLC)
   (void)text;
#endif
#if defined(HAVE_GLSLANG) || defined(HAVE_RSLC)
   {
      struct shader_line_buf lines;
      char cache_filename[PATH_MAX_LENGTH];
#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
      char fallback_filename[PATH_MAX_LENGTH];
#endif
      char *vertex_source   = NULL;
      char *fragment_source = NULL;
      const char *compiler  =
#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
         slang_prefer_rslc() ? "rslc" : "glslang";
#elif defined(HAVE_RSLC)
         "rslc";
#else
         "glslang";
#endif
      const char *used      = compiler;
      struct slang_blob *blob = NULL;
      bool listed           = false;

      if (!shader_line_buf_init(&lines))
         return false;

      RARCH_LOG("[Slang] Compiling shader: \"%s\".\n", shader_path);

      if (!glslang_read_shader_file_cached(shader_path, &lines, true, false,
               include_cache))
         goto error;

      /* Assemble both stage sources once; used for the cache key and,
       * on a cache miss, for compilation. */
      vertex_source   = glslang_build_stage_source(&lines, "vertex");
      fragment_source = glslang_build_stage_source(&lines, "fragment");
      if (!vertex_source || !fragment_source)
         goto error;

      spirv_cache_compute_hash(vertex_source, fragment_source, compiler,
            cache_filename);
#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
      spirv_cache_compute_hash(vertex_source, fragment_source, "glslang",
            fallback_filename);
#endif

      if (key_out)
         strlcpy(key_out, cache_filename, 33);

      /* A pass a live chain already compiled: its blobs, by reference */
      if (!detached)
         listed = slang_output_take_listed(output, cache_filename);
#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
      if (!detached && !listed
            && slang_output_take_listed(output, fallback_filename))
      {
         listed = true;
         used   = "glslang";
      }
#endif
      if (listed)
      {
         if (!glslang_parse_meta(&lines, &output->meta))
            goto error;
#if defined(HAVE_RSLC)
         if (text && !strcmp(used, "rslc"))
            slang_output_print_text(output, vertex_source, fragment_source,
                  text);
#endif
         RARCH_LOG("[Slang] Reused the compiled stages of \"%s\".\n",
               shader_path);
         free(vertex_source);
         free(fragment_source);
         shader_line_buf_free(&lines);
         return true;
      }

      /* Try to load from cache */
      if (spirv_cache_load(cache_filename, output))
      {
#if defined(HAVE_RSLC)
         if (     !strcmp(compiler, "rslc")
               && (   !slang_rslc_cached_stage_ok(output, false)
                   || !slang_rslc_cached_stage_ok(output, true)))
         {
            RARCH_WARN("[Slang] Recompiling \"%s\": the cached SPIR-V is not a module rslc accepts.\n",
                  shader_path);
            glslang_output_free(output);
         }
         else
#endif
         {
#if defined(HAVE_RSLC)
            /* a hit serves the same text a miss would, from the sources
             * already in hand */
            if (text && !strcmp(compiler, "rslc"))
               slang_output_print_text(output, vertex_source, fragment_source,
                     text);
#endif
            if (!detached)
               slang_output_list(output, cache_filename);
            RARCH_LOG("[Slang] Loaded shader from cache: \"%s\".\n", shader_path);
            free(vertex_source);
            free(fragment_source);
            shader_line_buf_free(&lines);
            return true;
         }
      }

      if (!glslang_parse_meta(&lines, &output->meta))
         goto error;

      if (!slang_compile_stage(vertex_source,
               GLSLANG_COMPILE_STAGE_VERTEX, &blob, &used, text, detached,
               shader_path, cache_filename))
      {
         RARCH_ERR("[Slang] Failed to compile vertex shader stage.\n");
         goto error;
      }
      glslang_output_set_blob(output, false, blob);
#if defined(HAVE_RSLC)
      if (text && !strcmp(used, "rslc"))
         output->vertex_text = slang_rslc_print_stage(vertex_source,
               GLSLANG_COMPILE_STAGE_VERTEX, text, NULL);
#endif

      /* The vertex source is dead now; release it before the fragment
       * compile so only one stage source is resident at a time. */
      free(vertex_source);
      vertex_source = NULL;

      if (!slang_compile_stage(fragment_source,
               GLSLANG_COMPILE_STAGE_FRAGMENT, &blob, &used, text, detached,
               shader_path, cache_filename))
      {
         RARCH_ERR("[Slang] Failed to compile fragment shader stage.\n");
         goto error;
      }
      glslang_output_set_blob(output, true, blob);
#if defined(HAVE_RSLC)
      if (text && !strcmp(used, "rslc") && output->vertex_text)
         output->fragment_text = slang_rslc_print_stage(fragment_source,
               GLSLANG_COMPILE_STAGE_FRAGMENT, text, NULL);
      if (!output->fragment_text)
      {
         /* both stages or neither: the driver takes one path */
         free(output->vertex_text);
         output->vertex_text = NULL;
      }
#endif

      RARCH_LOG("[Slang] Compiled \"%s\" with %s.\n", shader_path, used);
#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
      if (key_out && used != compiler)
         strlcpy(key_out, fallback_filename, 33);
#endif

      /* Save to cache under the compiler that produced the words, so a
       * fallback compile never answers for the other one. */
#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
      spirv_cache_save(used != compiler ? fallback_filename : cache_filename,
            output);
      if (!detached)
         slang_output_list(output,
               used != compiler ? fallback_filename : cache_filename);
#else
      (void)used;
      spirv_cache_save(cache_filename, output);
      if (!detached)
         slang_output_list(output, cache_filename);
#endif

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

   if (!detached)
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

bool glslang_compile_shader_text(const char *shader_path,
      glslang_output *output, void *include_cache,
      const struct slang_text_target *text)
{
   return slang_compile_pass(shader_path, output, include_cache, text,
         false, NULL);
}

bool slang_compile_pass_detached(const char *shader_path,
      glslang_output *output, char *key)
{
#if defined(HAVE_RSLC) || defined(HAVE_GLSLANG)
   return slang_compile_pass(shader_path, output, NULL, NULL, true, key);
#else
   (void)shader_path;
   (void)output;
   (void)key;
   return false;
#endif
}

#if defined(HAVE_RSLC)
static const char *const slang_frame_names[3] = {
   "Original", "OriginalHistory0", "Source" };

/* One stage of a pass compiled with @rm: when rewritten, its blob and,
 * for @text, its text; false when rslc cannot. */
static bool slang_remap_stage(const char *source,
      enum glslang_compile_stage stage, const struct rslc_remap *rm,
      const struct slang_text_target *text, enum rslc_remap_result *verdict,
      struct slang_blob **blob, char **printed)
{
   struct rslc_opts   opts;
   struct rslc_result res;
   char               err[256];

   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.remap   = rm;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   if (!slang_rslc_run(source, NULL, strlen(source),
            stage == GLSLANG_COMPILE_STAGE_VERTEX
            ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT, &opts, &res))
   {
      RARCH_WARN("[Slang] Frame rectangle: %s\n", err);
      return false;
   }
   /* a stage left as it was keeps its own module and text */
   *verdict = res.remap;
   if (res.remap != RSLC_REMAP_REWRITTEN)
   {
      rslc_result_free(&res, &opts);
      return true;
   }
   if (!(*blob = slang_blob_adopt_result(&res, &opts)))
      return false;
   if (text && !(*printed = slang_rslc_print_stage(source, stage, text, rm)))
      return false;
   return true;
}

bool slang_compile_remapped(const char *shader_path,
      const uint32_t *vs, size_t vs_words,
      const uint32_t *fs, size_t fs_words,
      bool source, bool linear, enum slang_rect_wrap wrap,
      const struct slang_text_target *text,
      enum slang_rect_result *verdict,
      struct slang_blob **vs_out, struct slang_blob **fs_out,
      char **vs_text, char **fs_text)
{
   struct shader_line_buf  lines;
   struct slang_rect_place place;
   struct rslc_remap       rm;
   enum rslc_remap_result  vres  = RSLC_REMAP_UNSUPPORTED;
   enum rslc_remap_result  fres  = RSLC_REMAP_UNSUPPORTED;
   struct slang_blob      *vb    = NULL;
   struct slang_blob      *fb    = NULL;
   char                   *vt    = NULL;
   char                   *ft    = NULL;
   char                   *vsrc  = NULL;
   char                   *fsrc  = NULL;
   bool                    ok    = false;

   *verdict = SLANG_RECT_UNSUPPORTED;
   *vs_out  = NULL;
   *fs_out  = NULL;
   if (vs_text)
      *vs_text = NULL;
   if (fs_text)
      *fs_text = NULL;
   /* where the members go, as the SPIR-V rewrite would put them */
   if (!slang_rect_place_pass(vs, vs_words, fs, fs_words, &place))
      return true;

   memset(&rm, 0, sizeof(rm));
   rm.samplers      = slang_frame_names;
   rm.sampler_count = source ? 3 : 2;
   rm.rect_name     = SLANG_RECT_NAME_RECT;
   rm.clamp_name    = SLANG_RECT_NAME_CLAMP;
   rm.texels_name   = SLANG_RECT_NAME_TEXELS;
   rm.where         = place.where == SLANG_RECT_UBO
      ? RSLC_REMAP_UBO : RSLC_REMAP_PUSH;
   rm.set           = place.set;
   rm.binding       = place.binding;
   rm.offset        = place.offset;
   rm.wrap          = (enum rslc_remap_wrap)wrap;
   rm.linear        = linear;

   if (!shader_line_buf_init(&lines))
      return false;
   if (     glslang_read_shader_file_cached(shader_path, &lines, true, false,
               NULL)
         && (vsrc = glslang_build_stage_source(&lines, "vertex"))
         && (fsrc = glslang_build_stage_source(&lines, "fragment"))
         && slang_remap_stage(vsrc, GLSLANG_COMPILE_STAGE_VERTEX, &rm,
               text, &vres, &vb, &vt)
         && (   vres == RSLC_REMAP_UNSUPPORTED
             || slang_remap_stage(fsrc, GLSLANG_COMPILE_STAGE_FRAGMENT, &rm,
                  text, &fres, &fb, &ft)))
   {
      ok = true;
      if (vres == RSLC_REMAP_UNSUPPORTED || fres == RSLC_REMAP_UNSUPPORTED)
         *verdict = SLANG_RECT_UNSUPPORTED;
      else if (vres == RSLC_REMAP_REWRITTEN || fres == RSLC_REMAP_REWRITTEN)
      {
         *verdict = SLANG_RECT_REWRITTEN;
         *vs_out  = vb;
         *fs_out  = fb;
         vb       = NULL;
         fb       = NULL;
         if (vs_text && fs_text)
         {
            *vs_text = vt;
            *fs_text = ft;
            vt       = NULL;
            ft       = NULL;
         }
      }
      else
         *verdict = SLANG_RECT_UNCHANGED;
   }
   slang_blob_unref(vb);
   slang_blob_unref(fb);
   free(vt);
   free(ft);
   free(vsrc);
   free(fsrc);
   shader_line_buf_free(&lines);
   return ok;
}
#endif

/* Stages compiled detached, held here with one reference each so they
 * stay listed until the chain that asked for them takes them; the
 * oldest goes when the ring is full. */
#define SLANG_OFFERS 64
static struct
{
   struct slang_blob *vs[SLANG_OFFERS];
   struct slang_blob *fs[SLANG_OFFERS];
   unsigned           next;
} slang_offers;

void slang_blob_offer(const char *key, struct slang_blob *vs,
      struct slang_blob *fs)
{
   struct slang_blob *listed = slang_blob_table_find(key, false);
   unsigned i;
   if (listed || !vs || !fs)
   {
      /* already there, or half a pass: nothing to list */
      slang_blob_unref(listed);
      slang_blob_unref(vs);
      slang_blob_unref(fs);
      return;
   }
   slang_blob_table_add(vs, key, false);
   slang_blob_table_add(fs, key, true);
   i = slang_offers.next++ % SLANG_OFFERS;
   slang_blob_unref(slang_offers.vs[i]);
   slang_blob_unref(slang_offers.fs[i]);
   slang_offers.vs[i] = vs;
   slang_offers.fs[i] = fs;
}

void slang_blob_offers_release(void)
{
   unsigned i;
   for (i = 0; i < SLANG_OFFERS; i++)
   {
      slang_blob_unref(slang_offers.vs[i]);
      slang_blob_unref(slang_offers.fs[i]);
      slang_offers.vs[i] = NULL;
      slang_offers.fs[i] = NULL;
   }
}

bool slang_preprocess_parse_parameters_meta(const glslang_meta *meta,
      struct video_shader *shader, unsigned pass)
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

      /* The pass that first declares a parameter owns it: a later pass
       * naming the same id takes the branch above and adds nothing. */
      p->pass    = (int)pass;
      strlcpy(p->id,   meta->parameters[i].id,   sizeof(p->id));
      strlcpy(p->desc, meta->parameters[i].desc, sizeof(p->desc));
      p->initial = meta->parameters[i].initial;
      p->minimum = meta->parameters[i].minimum;
      p->maximum = meta->parameters[i].maximum;
      p->step    = meta->parameters[i].step;
      video_shader_param_set(p, meta->parameters[i].initial);
   }

   return true;
}

/* -----------------------------------------------------------------------
 * slang_preprocess_parse_parameters (C-linkage overload) — uses shader_line_buf
 * ----------------------------------------------------------------------- */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader, unsigned pass)
{
   return slang_preprocess_parse_parameters_cached(shader_path, shader,
         pass, NULL);
}

bool slang_preprocess_parse_parameters_cached(const char *shader_path,
      struct video_shader *shader, unsigned pass, void *include_cache)
{
   struct shader_line_buf lines;

   memset(&lines, 0, sizeof(lines));

   if (shader_line_buf_init(&lines))
   {
      if (glslang_read_shader_pragmas_cached(shader_path, &lines,
               include_cache))
      {
         glslang_meta meta;
         memset(&meta, 0, sizeof(meta));
         meta.rt_format = SLANG_FORMAT_UNKNOWN;
         if (glslang_parse_meta(&lines, &meta))
         {
            bool ret = slang_preprocess_parse_parameters_meta(&meta, shader,
                  pass);
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

#if !defined(SLANG_RSLC_ONLY)
/* The pass's text and reflection through SPIRV-Cross, for stages the
 * compiler printed no text or wrote no tables for; @output stays the
 * caller's. */
static bool slang_process_cross(glslang_output *output,
      struct video_shader *shader_info, unsigned pass_number,
      enum rarch_shader_type dst_type, unsigned version,
      const semantics_map_t *semantics_map, pass_semantics_t *out)
{
   struct slang_spvc_handles handles;
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
   const char             *backend_name = "GLSL";
   const char             *vs_code     = NULL;
   const char             *ps_code     = NULL;
   const spvc_reflected_resource *list = NULL;
   size_t                  list_num    = 0;
   struct video_shader_pass *pass      = &shader_info->pass[pass_number];

   switch (dst_type)
   {
      case RARCH_SHADER_HLSL:
      case RARCH_SHADER_CG:
         backend      = SPVC_BACKEND_HLSL;
         backend_name = "HLSL";
         break;
      case RARCH_SHADER_METAL:
         backend      = SPVC_BACKEND_MSL;
         backend_name = "MSL";
         break;
      default:
         backend = SPVC_BACKEND_GLSL;
         break;
   }

   if (spvc_context_create(&ctx) != SPVC_SUCCESS)
      goto error;

   if (   spvc_context_parse_spirv(ctx, (const SpvId*)output->vertex,
            output->vertex_len, &vs_ir) != SPVC_SUCCESS
       || spvc_context_parse_spirv(ctx, (const SpvId*)output->fragment,
            output->fragment_len, &ps_ir) != SPVC_SUCCESS
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

   if (output->vertex_text && output->fragment_text)
   {
      /* the compiler's text, owned by the pass from here; Cross still
       * reflects the SPIR-V */
      pass->source.string.vertex   = output->vertex_text;
      pass->source.string.fragment = output->fragment_text;
      output->vertex_text           = NULL;
      output->fragment_text         = NULL;
      RARCH_LOG("[Slang] Using the compiler's %s directly.\n", backend_name);
   }
   else
   {
      if (   spvc_compiler_compile(vs_compiler, &vs_code) != SPVC_SUCCESS
          || spvc_compiler_compile(ps_compiler, &ps_code) != SPVC_SUCCESS)
         goto spvc_error;

      pass->source.string.vertex   = strdup(vs_code);
      pass->source.string.fragment = strdup(ps_code);
      if (!pass->source.string.vertex || !pass->source.string.fragment)
         goto error;
   }

   handles.vs     = vs_compiler;
   handles.ps     = ps_compiler;
   handles.vs_res = vs_resources;
   handles.ps_res = ps_resources;
   if (!slang_process_reflection(&handles, output, shader_info,
            pass_number, semantics_map, out))
      goto error;

   /* The context owns the IRs, compilers, options, resource lists and
    * compiled source strings (already strdup'd above). */
   spvc_context_destroy(ctx);
   return true;

spvc_error:
   RARCH_ERR("[Slang] SPIRV-Cross, %s: %s\n", backend_name,
         spvc_context_get_last_error_string(ctx));

error:
   spvc_context_destroy(ctx);
   return false;
}
#endif


bool slang_process(
      struct video_shader*   shader_info,
      unsigned               pass_number,
      enum rarch_shader_type dst_type,
      unsigned               version,
      const semantics_map_t* semantics_map,
      pass_semantics_t*      out)
{
   glslang_output          output;
   struct video_shader_pass *pass      = &shader_info->pass[pass_number];
   struct slang_text_target text;

   /* D3D and Metal take the compiler's own HLSL / MSL when it prints
    * one; the other destinations cross the SPIR-V */
   memset(&text, 0, sizeof(text));
   text.version = version;
   text.hlsl    = dst_type == RARCH_SHADER_HLSL;
   text.msl     = dst_type == RARCH_SHADER_METAL;
   if (!glslang_compile_shader_text(pass->source.path, &output, NULL,
            text.hlsl || text.msl ? &text : NULL))
      return false;

   if (!slang_preprocess_parse_parameters_meta(&output.meta, shader_info,
            pass_number))
   {
      glslang_output_free(&output);
      return false;
   }

   if (!*pass->alias && output.meta.name[0])
      strlcpy(pass->alias, output.meta.name, sizeof(pass->alias) - 1);

   /* Every pass samples the frame as the first does; one with the
    * filter unspecified as the smooth setting the driver gives. */
   out->frame_in_place = false;
   out->frame_unspec   = false;
   out->frame_smooth   = semantics_map->frame_smooth;
   if (semantics_map->uniforms[SLANG_SEMANTIC_ORIGINAL_RECT])
   {
      enum slang_rect_result res;
      enum slang_rect_wrap   wrap   = SLANG_RECT_WRAP_BORDER;
      unsigned               filter = shader_info->pass[0].filter;
#if defined(HAVE_RSLC)
      struct slang_blob     *vb     = NULL;
      struct slang_blob     *fb     = NULL;
      char                  *vt     = NULL;
      char                  *ft     = NULL;
#endif
      if (filter == RARCH_FILTER_UNSPEC)
         filter = semantics_map->frame_smooth
            ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
      switch (shader_info->pass[0].wrap)
      {
         case RARCH_WRAP_EDGE:
            wrap = SLANG_RECT_WRAP_EDGE;
            break;
         case RARCH_WRAP_REPEAT:
            wrap = SLANG_RECT_WRAP_REPEAT;
            break;
         case RARCH_WRAP_MIRRORED_REPEAT:
            wrap = SLANG_RECT_WRAP_MIRROR;
            break;
         default:
            break;
      }
#if defined(HAVE_RSLC)
      /* the compiler printed the stages: it prints them remapped too */
      if (     output.vertex_text
            && slang_compile_remapped(pass->source.path,
                  output.vertex, output.vertex_len,
                  output.fragment, output.fragment_len,
                  pass_number == 0, filter == RARCH_FILTER_LINEAR, wrap,
                  &text, &res, &vb, &fb, &vt, &ft))
      {
         if (vb)
         {
            glslang_output_set_blob(&output, false, vb);
            free(output.vertex_text);
            output.vertex_text   = vt;
         }
         if (fb)
         {
            glslang_output_set_blob(&output, true, fb);
            free(output.fragment_text);
            output.fragment_text = ft;
         }
      }
      else
#endif
#if !defined(SLANG_RSLC_ONLY)
      {
         uint32_t *nv = NULL, *nf = NULL;
         size_t    nvl = 0, nfl = 0;
         res = slang_rect_remap_pass(
               output.vertex, output.vertex_len,
               output.fragment, output.fragment_len,
               pass_number == 0, filter == RARCH_FILTER_LINEAR, wrap,
               &nv, &nvl, &nf, &nfl);
         /* a rewritten stage is a module without tables */
         if (nv)
            glslang_output_set_blob(&output, false, slang_blob_adopt_words(nv, nvl));
         if (nf)
            glslang_output_set_blob(&output, true, slang_blob_adopt_words(nf, nfl));
         if ((nv && !output.vertex) || (nf && !output.fragment))
            goto error;
         if (res == SLANG_RECT_REWRITTEN)
         {
            /* the rewrite is on the SPIR-V: the printed text is behind it */
            free(output.vertex_text);
            free(output.fragment_text);
            output.vertex_text   = NULL;
            output.fragment_text = NULL;
         }
      }
#else
         /* nothing prints a module the SPIR-V rewrite made: the frame
          * is copied out instead */
         res = SLANG_RECT_UNSUPPORTED;
#endif
      out->frame_in_place = res != SLANG_RECT_UNSUPPORTED;
      out->frame_unspec   = res == SLANG_RECT_REWRITTEN
         && shader_info->pass[0].filter == RARCH_FILTER_UNSPEC;
   }

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

   out->format = glslang_hdr_output_format(shader_info, pass_number, out->format);
   if (pass_number + 1 == shader_info->passes
         && video_shader_get_hdr_mode(shader_info) > 0)
      out->explicit_format = true;

   pass->source.string.vertex   = NULL;
   pass->source.string.fragment = NULL;

   if (output.vertex_text && output.fragment_text
         && slang_output_has_tables(&output))
   {
      /* the compiler's text and its tables: nothing for SPIRV-Cross */
      pass->source.string.vertex   = output.vertex_text;
      pass->source.string.fragment = output.fragment_text;
      output.vertex_text           = NULL;
      output.fragment_text         = NULL;
      RARCH_LOG("[Slang] Using the compiler's %s and its tables directly.\n",
            dst_type == RARCH_SHADER_METAL ? "MSL" : "HLSL");
      if (!slang_process_reflection(NULL, &output,
               shader_info, pass_number, semantics_map, out))
         goto error;
      glslang_output_free(&output);
      return true;
   }

#if !defined(SLANG_RSLC_ONLY)
   if (slang_process_cross(&output, shader_info, pass_number, dst_type,
            version, semantics_map, out))
   {
      glslang_output_free(&output);
      return true;
   }
#else
   RARCH_ERR("[Slang] No %s with tables for \"%s\"; nothing else prints it.\n",
         dst_type == RARCH_SHADER_METAL ? "MSL"
         : dst_type == RARCH_SHADER_HLSL ? "HLSL" : "GLSL",
         pass->source.path);
#endif

error:
   free(pass->source.string.vertex);
   free(pass->source.string.fragment);
   pass->source.string.vertex   = NULL;
   pass->source.string.fragment = NULL;
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

/* The scalar kind, width and shape a semantic's member must have.
 * @kind: 0 float, 1 int, 2 uint; @vec: vector width or matrix rows;
 * @cols: matrix columns, 1 otherwise. */
static bool validate_shape_for_semantic(unsigned kind, unsigned vec,
      unsigned cols, bool arrayed, enum slang_semantic sem)
{
   if (arrayed)
      return false;
   if (kind > 2)
      return false;

   switch (sem)
   {
         /* mat4 */
      case SLANG_SEMANTIC_MVP:
         return kind == 0 && vec == 4 && cols == 4;
         /* uint */
      case SLANG_SEMANTIC_FRAME_COUNT:
      case SLANG_SEMANTIC_TOTAL_SUBFRAMES:
      case SLANG_SEMANTIC_CURRENT_SUBFRAME:
      case SLANG_SEMANTIC_SWAP_COUNT:
      case SLANG_SEMANTIC_FRAME_TIME_DELTA:
      case SLANG_SEMANTIC_ROTATION:
      case SLANG_SEMANTIC_PASSTHROUGH:
      case SLANG_SEMANTIC_SOURCE_HDR:
      case SLANG_SEMANTIC_TONEMAP_OPERATOR:
      case SLANG_SEMANTIC_HDR:
      case SLANG_SEMANTIC_SUBPIXEL_LAYOUT:
      case SLANG_SEMANTIC_EXPAND_GAMUT:
         return kind == 2 && vec == 1 && cols == 1;
         /* int */
      case SLANG_SEMANTIC_FRAME_DIRECTION:
         return kind == 1 && vec == 1 && cols == 1;
         /* float */
      case SLANG_SEMANTIC_ORIGINAL_FPS:
      case SLANG_SEMANTIC_CORE_ASPECT:
      case SLANG_SEMANTIC_CORE_ASPECT_ROT:
      case SLANG_SEMANTIC_FLOAT_PARAMETER:
      case SLANG_SEMANTIC_PAPER_WHITE_NITS:
      case SLANG_SEMANTIC_PAPER_WHITE_ALIAS:
      case SLANG_SEMANTIC_PEAK_NITS:
      case SLANG_SEMANTIC_MAX_NITS_ALIAS:
      case SLANG_SEMANTIC_ENABLE_HDR:
      case SLANG_SEMANTIC_MIN_NITS:
      case SLANG_SEMANTIC_SCANLINES:
      case SLANG_SEMANTIC_INVERSE_TONEMAP:
      case SLANG_SEMANTIC_HDR10:
         return kind == 0 && vec == 1 && cols == 1;
         /* vec3 - sensor uniforms */
      case SLANG_SEMANTIC_GYROSCOPE:
      case SLANG_SEMANTIC_ACCELEROMETER:
      case SLANG_SEMANTIC_ACCELEROMETER_REST:
         return kind == 0 && vec == 3 && cols == 1;
         /* vec4 */
      default:
         break;
   }
   return kind == 0 && vec == 4 && cols == 1;
}

#if !defined(SLANG_RSLC_ONLY)
static unsigned spvc_kind_of(spvc_type type)
{
   switch (spvc_type_get_basetype(type))
   {
      case SPVC_BASETYPE_FP32:  return 0;
      case SPVC_BASETYPE_INT32: return 1;
      case SPVC_BASETYPE_UINT32: return 2;
      default: return 3;
   }
}

static bool validate_type_for_semantic(spvc_type type, enum slang_semantic sem)
{
   return validate_shape_for_semantic(spvc_kind_of(type),
         spvc_type_get_vector_size(type), spvc_type_get_columns(type),
         spvc_type_get_num_array_dimensions(type) != 0, sem);
}

static bool validate_type_for_texture_semantic(spvc_type type)
{
   return    (spvc_type_get_num_array_dimensions(type) == 0)
          && (spvc_type_get_basetype(type) == SPVC_BASETYPE_FP32)
          && (spvc_type_get_vector_size(type) == 4)
          && (spvc_type_get_columns(type) == 1);
}
#endif

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

#if !defined(SLANG_RSLC_ONLY)
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
   const spvc_reflected_resource *f_active_imgs = NULL;
   size_t f_num_active_imgs = 0;
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

   /* The samplers the fragment stage reads: SPIRV-Cross's active
    * interface variables. */
   {
      spvc_set       active_set = NULL;
      spvc_resources active_res = NULL;
      if (     spvc_compiler_get_active_interface_variables(fragment_compiler,
                  &active_set) != SPVC_SUCCESS
            || spvc_compiler_create_shader_resources_for_active_variables(
                  fragment_compiler, &active_res, active_set) != SPVC_SUCCESS
            || !spvc_fetch_list(active_res, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
                  &f_active_imgs, &f_num_active_imgs))
      {
         RARCH_ERR("[Slang] Failed to enumerate active shader resources.\n");
         return false;
      }
   }

   /* On to textures. */
   for (i = 0; i < f_num_imgs; i++)
   {
      unsigned array_index = 0;
      size_t   j;
      bool     active      = false;
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

      /* A sampler nothing main reaches reads is checked as declared and
       * otherwise left out: no history frame, feedback buffer or bind
       * is kept for it. */
      for (j = 0; j < f_num_active_imgs && !active; j++)
         active = f_active_imgs[j].id == f_imgs[i].id;
      if (!active)
         continue;

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
#endif

static bool slang_feedback_name(const struct glslang_last_pass *state,
      const char *name, bool size)
{
   char expected[96];
   snprintf(expected, sizeof(expected), size ? "PassFeedbackSize%u"
         : "PassFeedback%u", state->index);
   if (!strcmp(name, expected))
      return true;
   if (!state->name[0])
      return false;
   snprintf(expected, sizeof(expected), size ? "%sFeedbackSize"
         : "%sFeedback", state->name);
   return !strcmp(name, expected);
}

#if defined(SLANG_RSLC_ONLY)
/* The feedback names a stage's tables hold: the samplers and block
 * members it reads. */
static void slang_feedback_tables(struct glslang_last_pass *state,
      const struct slang_blob *blob)
{
   const uint8_t             *mem = (const uint8_t*)blob->mem;
   const struct rslc_reflect *r   = blob->refl;
   const struct rslc_sampler *smp = (const struct rslc_sampler*)(mem + r->samplers);
   unsigned i, k;
   for (i = 0; i < r->sampler_count && !state->needs_target; i++)
      if (smp[i].active)
         state->needs_target = slang_feedback_name(state,
               RSLC_STR(mem, smp[i].name), false);
   for (k = 0; k < 2 && !state->needs_target; k++)
   {
      const struct rslc_block  *b = k ? &r->push : &r->ubo;
      const struct rslc_member *m = (const struct rslc_member*)(mem + b->members);
      if (!b->present)
         continue;
      for (i = 0; i < b->member_count && !state->needs_target; i++)
         if (m[i].active)
            state->needs_target = slang_feedback_name(state,
                  RSLC_STR(mem, m[i].name), true);
   }
}

bool slang_last_pass_feedback(struct glslang_last_pass *state,
      const glslang_output *output)
{
   if (!state->inspect_feedback || state->needs_target)
      return true;
   /* a module without tables: assume it reads its feedback */
   if (!slang_output_has_tables(output))
   {
      state->needs_target = true;
      return true;
   }
   slang_feedback_tables(state, output->vertex_blob);
   slang_feedback_tables(state, output->fragment_blob);
   return true;
}
#else
bool slang_last_pass_feedback(struct glslang_last_pass *state,
      const glslang_output *output)
{
   spvc_context ctx = NULL;
   unsigned stage, kind;
   bool ret = false;
   if (!state->inspect_feedback || state->needs_target)
      return true;
   if (spvc_context_create(&ctx) != SPVC_SUCCESS)
      return false;
   for (stage = 0; stage < 2 && !state->needs_target; stage++)
   {
      spvc_parsed_ir ir;
      spvc_compiler compiler;
      spvc_set active;
      spvc_resources resources;
      const uint32_t *words = stage ? output->fragment : output->vertex;
      size_t length = stage ? output->fragment_len : output->vertex_len;
      if (spvc_context_parse_spirv(ctx, (const SpvId*)words, length,
               &ir) != SPVC_SUCCESS
            || spvc_context_create_compiler(ctx, SPVC_BACKEND_NONE, ir,
               SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS
            || spvc_compiler_get_active_interface_variables(compiler,
               &active) != SPVC_SUCCESS
            || spvc_compiler_create_shader_resources_for_active_variables(
               compiler, &resources, active) != SPVC_SUCCESS)
         goto end;
      for (kind = 0; kind < 3 && !state->needs_target; kind++)
      {
         const spvc_reflected_resource *list;
         size_t count, i;
         spvc_resource_type type = kind == 0 ? SPVC_RESOURCE_TYPE_SAMPLED_IMAGE
            : kind == 1 ? SPVC_RESOURCE_TYPE_UNIFORM_BUFFER
            : SPVC_RESOURCE_TYPE_PUSH_CONSTANT;
         if (spvc_resources_get_resource_list_for_type(resources, type,
                  &list, &count) != SPVC_SUCCESS)
            goto end;
         for (i = 0; i < count && !state->needs_target; i++)
         {
            if (!kind)
               state->needs_target = slang_feedback_name(state, list[i].name, false);
            else
            {
               const spvc_buffer_range *ranges;
               size_t num_ranges, j;
               if (spvc_compiler_get_active_buffer_ranges(compiler, list[i].id,
                        &ranges, &num_ranges) != SPVC_SUCCESS)
                  goto end;
               for (j = 0; j < num_ranges && !state->needs_target; j++)
                  state->needs_target = slang_feedback_name(state,
                        spvc_compiler_get_member_name(compiler,
                           list[i].base_type_id, ranges[j].index), true);
            }
         }
      }
   }
   ret = true;
end:
   spvc_context_destroy(ctx);
   return ret;
}
#endif

#if defined(HAVE_RSLC)
/* ---- reflection from the compiler's tables ------------------------ */

/* One block of one stage, as add_active_buffer_ranges() reads it from
 * SPIRV-Cross's active ranges: every member the stage touches must be
 * a known semantic of the right shape, and lands at its offset. */
static bool slang_blob_block_ranges(const uint8_t *mem,
      const struct rslc_block *b, slang_reflection *reflection,
      bool push_constant)
{
   const struct rslc_member *m = (const struct rslc_member*)(mem + b->members);
   uint32_t i;

   for (i = 0; i < b->member_count; i++)
   {
      unsigned sem_index     = 0;
      unsigned tex_sem_index = 0;
      const char *name;
      unsigned kind;
      enum slang_semantic sem;
      enum slang_texture_semantic tex_sem;

      if (!m[i].active)
         continue;
      name    = RSLC_STR(mem, m[i].name);
      kind    = m[i].base == RSLC_BASE_FLOAT ? 0
              : m[i].base == RSLC_BASE_INT   ? 1
              : m[i].base == RSLC_BASE_UINT  ? 2 : 3;
      sem     = slang_uniform_name_to_semantic(
            reflection->semantic_map, name, &sem_index);
      tex_sem = slang_uniform_name_to_texture_semantic(
            reflection->texture_semantic_uniform_map, name, &tex_sem_index);

      if (tex_sem != SLANG_INVALID_TEXTURE_SEMANTIC &&
            !validate_texture_semantic_index(reflection, tex_sem, tex_sem_index))
         return false;

      if (sem != SLANG_INVALID_SEMANTIC)
      {
         if (!validate_shape_for_semantic(kind, m[i].rows, m[i].cols,
                  m[i].array_len != 0, sem))
         {
            RARCH_ERR("[Slang] Underlying type of semantic is invalid.\n");
            return false;
         }
         if (sem == SLANG_SEMANTIC_FLOAT_PARAMETER)
         {
            if (!set_ubo_float_parameter_offset(reflection, sem_index,
                     m[i].offset, m[i].rows, push_constant))
               return false;
         }
         else if (!set_ubo_offset(reflection, sem, m[i].offset,
                  m[i].rows * m[i].cols, push_constant))
            return false;
      }
      else if (tex_sem != SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         if (m[i].array_len || kind != 0 || m[i].rows != 4 || m[i].cols != 1)
         {
            RARCH_ERR("[Slang] Underlying type of texture"
                  " semantic is invalid.\n");
            return false;
         }
         if (!set_ubo_texture_offset(reflection, tex_sem, tex_sem_index,
                  m[i].offset, push_constant))
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

bool slang_reflect_blobs(const struct slang_blob *vertex,
      const struct slang_blob *fragment, slang_reflection *reflection)
{
   const struct rslc_reflect *vr = vertex->refl;
   const struct rslc_reflect *fr = fragment->refl;
   const uint8_t *vm = (const uint8_t*)vertex->mem;
   const uint8_t *fm = (const uint8_t*)fragment->mem;
   const struct rslc_io *io;
   const struct rslc_sampler *smp;
   uint32_t location_mask = 0;
   uint32_t binding_mask  = 0;
   unsigned vertex_ubo_binding, fragment_ubo_binding, ubo_binding;
   bool has_ubo;
   uint32_t i;

   /* Validate use of unexpected types: a sampler in the vertex stage
    * is what SPIRV-Cross would list under its sampled images. */
   if (vr->sampler_count)
   {
      RARCH_ERR("[Slang] Invalid resource type detected.\n");
      return false;
   }

   /* Validate vertex input. */
   if (vr->input_count != 2)
   {
      RARCH_ERR("[Slang] Vertex must have two attributes.\n");
      return false;
   }

   if (fr->output_count != 1)
   {
      RARCH_ERR("[Slang] Multiple render targets not supported.\n");
      return false;
   }

   io = (const struct rslc_io*)(fm + fr->outputs);
   if (io[0].location != 0)
   {
      RARCH_ERR("[Slang] Render target must use location = 0.\n");
      return false;
   }

   io = (const struct rslc_io*)(vm + vr->inputs);
   for (i = 0; i < vr->input_count; i++)
      if (io[i].location < 32)
         location_mask |= 1u << io[i].location;

   if (location_mask != 0x3)
   {
      RARCH_ERR("[Slang] The two vertex attributes do not"
            " use location = 0 and location = 1.\n");
      return false;
   }

   /* The tables hold at most one uniform block and one push block per
    * stage, as the dialect does; the set is 0 unless declared. */
   if ((vr->ubo.present && vr->ubo.set != RSLC_NO_BINDING && vr->ubo.set != 0)
         || (fr->ubo.present && fr->ubo.set != RSLC_NO_BINDING && fr->ubo.set != 0))
   {
      RARCH_ERR("[Slang] Resources must use descriptor set #0.\n");
      return false;
   }

   vertex_ubo_binding   = vr->ubo.present
      ? (vr->ubo.binding == RSLC_NO_BINDING ? 0 : vr->ubo.binding) : (unsigned)-1;
   fragment_ubo_binding = fr->ubo.present
      ? (fr->ubo.binding == RSLC_NO_BINDING ? 0 : fr->ubo.binding) : (unsigned)-1;
   has_ubo              = vr->ubo.present || fr->ubo.present;

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

   if (vr->ubo.present)
   {
      reflection->ubo_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      reflection->ubo_size        = MAX(reflection->ubo_size, vr->ubo.size);
   }
   if (fr->ubo.present)
   {
      reflection->ubo_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      reflection->ubo_size        = MAX(reflection->ubo_size, fr->ubo.size);
   }
   if (vr->push.present)
   {
      reflection->push_constant_stage_mask |= SLANG_STAGE_VERTEX_MASK;
      reflection->push_constant_size        = MAX(
            reflection->push_constant_size, vr->push.size);
   }
   if (fr->push.present)
   {
      reflection->push_constant_stage_mask |= SLANG_STAGE_FRAGMENT_MASK;
      reflection->push_constant_size        = MAX(
            reflection->push_constant_size, fr->push.size);
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
   if (vr->ubo.present && !slang_blob_block_ranges(vm, &vr->ubo, reflection, false))
      return false;
   if (fr->ubo.present && !slang_blob_block_ranges(fm, &fr->ubo, reflection, false))
      return false;
   if (vr->push.present && !slang_blob_block_ranges(vm, &vr->push, reflection, true))
      return false;
   if (fr->push.present && !slang_blob_block_ranges(fm, &fr->push, reflection, true))
      return false;

   if (has_ubo)
      binding_mask = 1 << ubo_binding;

   /* On to textures. */
   smp = (const struct rslc_sampler*)(fm + fr->samplers);
   for (i = 0; i < fr->sampler_count; i++)
   {
      unsigned array_index = 0;
      unsigned set         = smp[i].set == RSLC_NO_BINDING ? 0 : smp[i].set;
      unsigned binding     = smp[i].binding == RSLC_NO_BINDING ? 0 : smp[i].binding;
      const char *name     = RSLC_STR(fm, smp[i].name);
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
            reflection->texture_semantic_map, name, &array_index);

      if (index == SLANG_INVALID_TEXTURE_SEMANTIC)
      {
         RARCH_ERR("[Slang] Texture name \"%s\" not found in semantic map, "
                   "Probably the texture name or pass alias is not defined "
                   "in the preset (Non-semantic textures not supported yet)\n",
                   name);
         return false;
      }

      if (!validate_texture_semantic_index(reflection, index, array_index))
         return false;

      /* A sampler nothing main reaches reads is checked as declared and
       * otherwise left out: no history frame, feedback buffer or bind
       * is kept for it. */
      if (!smp[i].active)
         continue;

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
   return true;
}
#endif

bool slang_reflect_pass(const struct slang_blob *vertex,
      const struct slang_blob *fragment, slang_reflection *reflection)
{
#if defined(HAVE_RSLC)
   if (vertex->refl && fragment->refl)
   {
      RARCH_LOG("[Slang] Reflected pass %u from the compiler's tables.\n",
            reflection->pass_number);
      return slang_reflect_blobs(vertex, fragment, reflection);
   }
#endif
   return slang_reflect_spirv(vertex->words, vertex->nwords,
         fragment->words, fragment->nwords, reflection);
}

#if defined(SLANG_RSLC_ONLY)
/* One stage's tables, read back from its module by rslc */
static struct slang_blob *slang_ingest_stage(const uint32_t *words,
      size_t n, enum rslc_stage stage)
{
   struct rslc_opts   opts;
   struct rslc_result res;
   char               err[256];
   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   if (!slang_rslc_run(NULL, words, n, stage, &opts, &res))
   {
      RARCH_ERR("[Slang] %s\n", err);
      return NULL;
   }
   return slang_blob_adopt_result(&res, &opts);
}

bool slang_reflect_spirv(const uint32_t *vertex, size_t vertex_len,
      const uint32_t *fragment, size_t fragment_len,
      slang_reflection *reflection)
{
   bool ret              = false;
   struct slang_blob *vs = slang_ingest_stage(vertex, vertex_len,
         RSLC_STAGE_VERTEX);
   struct slang_blob *fs = vs ? slang_ingest_stage(fragment, fragment_len,
         RSLC_STAGE_FRAGMENT) : NULL;
   if (fs)
      ret = slang_reflect_blobs(vs, fs, reflection);
   slang_blob_unref(vs);
   slang_blob_unref(fs);
   return ret;
}
#else
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
#endif
