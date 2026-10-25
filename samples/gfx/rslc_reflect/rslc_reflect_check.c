/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rslc_reflect_check.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Holds rslc's reflection section against SPIRV-Cross over a corpus
 * dump.  For every stage, the blob's tables must say what Cross says
 * of the very same module: block binding, declared size, each
 * member's name, offset and size, the set of members the stage
 * actually reads (Cross's active buffer ranges), and each sampler's
 * name, binding and whether the stage reads it (Cross's active
 * interface variables).  slang_process.c derives the filter chain's
 * reflection from exactly these Cross calls, so agreement here is
 * what lets the chain read the blob instead.
 *
 * With --ingest the module under test is glslang's .spv beside each
 * stage, read by rslc_compile_spirv rather than compiled from the
 * GLSL: the tables a cache hit yields must agree with Cross on that
 * module the same way. */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gfx/shaders/rslc.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <spirv_cross_c.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static unsigned diffs;
static int      ingest;
static unsigned refused;

static char *read_file(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   char *buf;
   long  n;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (char*)malloc((size_t)n + 1);
   if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(buf);
      return NULL;
   }
   fclose(f);
   buf[n] = '\0';
   *len   = (size_t)n;
   return buf;
}

static void mismatch(const char *stage, const char *what, const char *detail)
{
   printf("DIFF %s: %s %s\n", stage, what, detail ? detail : "");
   diffs++;
}

/* One block: the blob's table against Cross's view of resource res. */
static void check_block(const char *stage, spvc_compiler comp,
      const spvc_reflected_resource *res, const uint8_t *blob,
      const struct rslc_block *b, const char *label)
{
   const struct rslc_member *m;
   const spvc_buffer_range  *ranges = NULL;
   size_t num_ranges = 0, declared = 0;
   spvc_type type;
   unsigned  i, n;
   char      detail[256];

   if (!b->present)
   {
      mismatch(stage, label, "declared by Cross, absent from the blob");
      return;
   }
   type = spvc_compiler_get_type_handle(comp, res->base_type_id);
   n    = spvc_type_get_num_member_types(type);
   if (n != b->member_count)
   {
      snprintf(detail, sizeof(detail), "member count %u vs %u", n, b->member_count);
      mismatch(stage, label, detail);
      return;
   }
   if (strcmp(label, "push") != 0)
   {
      unsigned binding = spvc_compiler_get_decoration(comp, res->id, SpvDecorationBinding);
      unsigned set     = spvc_compiler_get_decoration(comp, res->id, SpvDecorationDescriptorSet);
      if (binding != b->binding)
      {
         snprintf(detail, sizeof(detail), "binding %u vs %u", binding, b->binding);
         mismatch(stage, label, detail);
      }
      /* an undeclared set is set 0 to the module */
      if (set != (b->set == RSLC_NO_BINDING ? 0 : b->set))
      {
         snprintf(detail, sizeof(detail), "set %u vs %u", set, b->set);
         mismatch(stage, label, detail);
      }
   }
   spvc_compiler_get_declared_struct_size(comp, type, &declared);
   if (declared != b->size)
   {
      snprintf(detail, sizeof(detail), "size %u vs %u", (unsigned)declared, b->size);
      mismatch(stage, label, detail);
   }
   m = (const struct rslc_member*)(blob + b->members);
   for (i = 0; i < n; i++)
   {
      const char *name = spvc_compiler_get_member_name(comp, res->base_type_id, i);
      unsigned    off  = 0;
      size_t      size = 0;
      spvc_compiler_type_struct_member_offset(comp, type, i, &off);
      spvc_compiler_get_declared_struct_member_size(comp, type, i, &size);
      if (strcmp(name, RSLC_STR(blob, m[i].name)) != 0)
      {
         snprintf(detail, sizeof(detail), "member %u name %s vs %s", i, name, RSLC_STR(blob, m[i].name));
         mismatch(stage, label, detail);
      }
      if (off != m[i].offset || size != m[i].size)
      {
         snprintf(detail, sizeof(detail), "member %s offset/size %u/%u vs %u/%u",
               name, off, (unsigned)size, m[i].offset, m[i].size);
         mismatch(stage, label, detail);
      }
   }
   /* active set */
   if (spvc_compiler_get_active_buffer_ranges(comp, res->id, &ranges, &num_ranges) != SPVC_SUCCESS)
   {
      mismatch(stage, label, "active ranges unavailable");
      return;
   }
   for (i = 0; i < n; i++)
   {
      size_t k;
      int cross_active = 0;
      for (k = 0; k < num_ranges; k++)
         if (ranges[k].index == i)
            cross_active = 1;
      if (cross_active != (m[i].active != 0))
      {
         snprintf(detail, sizeof(detail), "member %s active %d vs %d",
               RSLC_STR(blob, m[i].name), cross_active, m[i].active != 0);
         mismatch(stage, label, detail);
      }
   }
}

/* Returns 1 when the stage was checked. */
static int check_stage(const char *dir, const char *file)
{
   char   path[1024];
   char  *src;
   size_t len;
   struct rslc_opts   opts;
   struct rslc_result res;
   char   err[256];
   spvc_context   ctx  = NULL;
   spvc_parsed_ir ir   = NULL;
   spvc_compiler  comp = NULL;
   spvc_resources rs   = NULL;
   const spvc_reflected_resource *list;
   size_t n, i;
   const uint8_t *blob;
   const struct rslc_reflect *rf;
   enum rslc_stage stage = strstr(file, ".vert.") ? RSLC_STAGE_VERTEX : RSLC_STAGE_FRAGMENT;
   char detail[256];
   const uint32_t *module;
   size_t nwords;

   /* a stage glslang rejected has no .spv beside it and is not a gate */
   snprintf(path, sizeof(path), "%s/%.*s.spv", dir, (int)(strlen(file) - 5), file);
   {
      FILE *f = fopen(path, "rb");
      if (!f)
         return 0;
      fclose(f);
   }
   memset(&opts, 0, sizeof(opts));
   opts.dst     = RSLC_DST_SPIRV;
   opts.err     = err;
   opts.err_cap = sizeof(err);
   err[0]       = '\0';
   if (ingest)
   {
      /* glslang's module through the ingest; Cross reads the same words */
      src = read_file(path, &len);
      if (!src || (len & 3))
      {
         free(src);
         return 0;
      }
      if (!rslc_compile_spirv((const uint32_t*)src, len / 4, stage, &opts, &res))
      {
         printf("REFUSED %s: %s\n", file, err);
         refused++;
         free(src);
         return 1;
      }
      module = (const uint32_t*)src;
      nwords = len / 4;
   }
   else
   {
      snprintf(path, sizeof(path), "%s/%s", dir, file);
      src = read_file(path, &len);
      if (!src)
         return 0;
      if (!rslc_compile(src, len, stage, &opts, &res))
      {
         printf("FAIL %s: %s\n", file, err);
         diffs++;
         free(src);
         return 1;
      }
      free(src);
      src    = NULL;
      module = res.spirv;
      nwords = res.spirv_len;
   }
   blob = (const uint8_t*)res.blob;
   rf   = res.refl;

   if (spvc_context_create(&ctx) != SPVC_SUCCESS
         || spvc_context_parse_spirv(ctx, (const SpvId*)module, nwords, &ir) != SPVC_SUCCESS
         || spvc_context_create_compiler(ctx, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &comp) != SPVC_SUCCESS
         || spvc_compiler_create_shader_resources(comp, &rs) != SPVC_SUCCESS)
   {
      printf("FAIL %s: SPIRV-Cross: %s\n", file, ctx ? spvc_context_get_last_error_string(ctx) : "context");
      diffs++;
      goto out;
   }

   /* blocks */
   spvc_resources_get_resource_list_for_type(rs, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &n);
   if (n > 1)
      mismatch(file, "ubo", "more than one");
   if (n == 1)
      check_block(file, comp, &list[0], blob, &rf->ubo, "ubo");
   else if (rf->ubo.present)
      mismatch(file, "ubo", "in the blob, not in the module");
   spvc_resources_get_resource_list_for_type(rs, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &list, &n);
   if (n > 1)
      mismatch(file, "push", "more than one");
   if (n == 1)
      check_block(file, comp, &list[0], blob, &rf->push, "push");
   else if (rf->push.present)
      mismatch(file, "push", "in the blob, not in the module");

   /* samplers, by name */
   spvc_resources_get_resource_list_for_type(rs, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &list, &n);
   if (n != rf->sampler_count)
   {
      snprintf(detail, sizeof(detail), "%u vs %u", (unsigned)n, rf->sampler_count);
      mismatch(file, "sampler count", detail);
   }
   else
   {
      const struct rslc_sampler *s = (const struct rslc_sampler*)(blob + rf->samplers);
      spvc_set active_set = NULL;
      spvc_resources active_rs = NULL;
      const spvc_reflected_resource *active = NULL;
      size_t nactive = 0;
      if (     spvc_compiler_get_active_interface_variables(comp, &active_set) != SPVC_SUCCESS
            || spvc_compiler_create_shader_resources_for_active_variables(comp, &active_rs, active_set) != SPVC_SUCCESS
            || spvc_resources_get_resource_list_for_type(active_rs, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &active, &nactive) != SPVC_SUCCESS)
         mismatch(file, "sampler", "active set unavailable");
      for (i = 0; i < n; i++)
      {
         unsigned binding = spvc_compiler_get_decoration(comp, list[i].id, SpvDecorationBinding);
         size_t k;
         int found = 0;
         for (k = 0; k < n; k++)
            if (!strcmp(list[i].name, RSLC_STR(blob, s[k].name)))
            {
               found = 1;
               if (s[k].binding != binding)
               {
                  snprintf(detail, sizeof(detail), "%s binding %u vs %u", list[i].name, binding, s[k].binding);
                  mismatch(file, "sampler", detail);
               }
               {
                  size_t a;
                  unsigned cross_active = 0;
                  for (a = 0; a < nactive; a++)
                     if (active[a].id == list[i].id)
                        cross_active = 1;
                  if (cross_active != (s[k].active != 0))
                  {
                     snprintf(detail, sizeof(detail), "%s active %u vs %u", list[i].name, cross_active, s[k].active);
                     mismatch(file, "sampler", detail);
                  }
               }
               if (spvc_compiler_get_decoration(comp, list[i].id, SpvDecorationDescriptorSet)
                     != (s[k].set == RSLC_NO_BINDING ? 0 : s[k].set))
               {
                  snprintf(detail, sizeof(detail), "%s set %u", list[i].name, s[k].set);
                  mismatch(file, "sampler", detail);
               }
            }
         if (!found)
            mismatch(file, "sampler missing from blob", list[i].name);
      }
   }

   /* stage inputs / outputs, by name and location */
   spvc_resources_get_resource_list_for_type(rs, SPVC_RESOURCE_TYPE_STAGE_INPUT, &list, &n);
   if (n != rf->input_count)
   {
      snprintf(detail, sizeof(detail), "%u vs %u", (unsigned)n, rf->input_count);
      mismatch(file, "input count", detail);
   }
   spvc_resources_get_resource_list_for_type(rs, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &list, &n);
   if (n != rf->output_count)
   {
      snprintf(detail, sizeof(detail), "%u vs %u", (unsigned)n, rf->output_count);
      mismatch(file, "output count", detail);
   }
   else
   {
      const struct rslc_io *io = (const struct rslc_io*)(blob + rf->outputs);
      for (i = 0; i < n; i++)
      {
         unsigned loc = spvc_compiler_get_decoration(comp, list[i].id, SpvDecorationLocation);
         size_t k;
         for (k = 0; k < n; k++)
            if (!strcmp(list[i].name, RSLC_STR(blob, io[k].name)) && io[k].location != loc)
            {
               snprintf(detail, sizeof(detail), "%s location %u vs %u", list[i].name, loc, io[k].location);
               mismatch(file, "output", detail);
            }
      }
   }

out:
   if (ctx)
      spvc_context_destroy(ctx);
   rslc_result_free(&res, &opts);
   free(src);
   return 1;
}

int main(int argc, char **argv)
{
   DIR *d;
   struct dirent *ent;
   unsigned stages = 0;
   const char *dir;
   if (argc == 3 && !strcmp(argv[1], "--ingest"))
      ingest = 1;
   else if (argc != 2)
   {
      fprintf(stderr, "usage: rslc_reflect_check [--ingest] <dumpdir>\n");
      return 2;
   }
   dir = argv[argc - 1];
   d   = opendir(dir);
   if (!d)
   {
      fprintf(stderr, "cannot open %s\n", dir);
      return 2;
   }
   while ((ent = readdir(d)))
   {
      size_t n = strlen(ent->d_name);
      if (n > 5 && !strcmp(ent->d_name + n - 5, ".glsl"))
         stages += check_stage(dir, ent->d_name);
   }
   closedir(d);
   if (ingest)
      printf("%u stages, %u refused, %u mismatches\n", stages, refused, diffs);
   else
      printf("%u stages, %u mismatches\n", stages, diffs);
   return (diffs || refused) ? 1 : 0;
}
