/* Last-pass target requirements and compiled feedback resources.
 *
 * The slang chains give the last pass of a preset a render target of
 * its own, copied to the backbuffer by a hidden pass, exactly when this
 * says so.  A last pass scaled to the viewport at 1.0 renders straight
 * to the backbuffer; any other scale, a render target format asked for
 * by the preset or by the shader, and any pass reading the last pass's
 * feedback keep the target. The setup check reads metadata through an
 * include cache; feedback_test.c compiles real stages for resource usage.
 *
 * The sources are served from memory in place of video_shader_parse.c,
 * so nothing here touches the file system. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>

#include "gfx/drivers_shader/glslang_util.h"

static unsigned failures = 0;
unsigned test_compiled_feedback(void);

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }

/* ---- the sources ---- */
struct source
{
   const char *name;
   const char *text;
   unsigned    reads;
};

static struct source sources[] =
{
   { "first.slang",
     "#version 450\n"
     "#include \"common.inc\"\n"
     "void main() {}\n", 0 },
   { "last.slang",
     "#version 450\n"
     "#include \"common.inc\"\n"
     "void main() {}\n", 0 },
   { "common.inc",
     "#pragma parameter STRENGTH \"Strength\" 1.0 0.0 2.0 0.1\n"
     "layout(set = 0, binding = 2) uniform sampler2D Source;\n", 0 },
   /* reads the feedback of pass 1, by index */
   { "reads_index.slang",
     "#version 450\n"
     "layout(set = 0, binding = 3) uniform sampler2D PassFeedback1;\n"
     "void main() {}\n", 0 },
   /* only the size of pass 1's feedback */
   { "reads_size.slang",
     "#version 450\n"
     "layout(push_constant) uniform Push { vec4 PassFeedbackSize1; } p;\n"
     "void main() {}\n", 0 },
   /* the feedback of passes 0, 10 and 12 - none of them pass 1 */
   { "reads_other.slang",
     "#version 450\n"
     "layout(set = 0, binding = 3) uniform sampler2D PassFeedback0;\n"
     "layout(set = 0, binding = 4) uniform sampler2D PassFeedback10;\n"
     "layout(set = 0, binding = 5) uniform sampler2D PassFeedback12;\n"
     "layout(set = 0, binding = 6) uniform sampler2D FinalFeedbackX;\n"
     "void main() {}\n", 0 },
   /* the feedback of the pass named Final */
   { "reads_name.slang",
     "#version 450\n"
     "layout(set = 0, binding = 3) uniform sampler2D FinalFeedback;\n"
     "void main() {}\n", 0 },
   /* the last pass, named by its own pragma */
   { "named_last.slang",
     "#version 450\n"
     "#pragma name Final\n"
     "void main() {}\n", 0 },
   /* the last pass, reading its own feedback */
   { "self_feedback.slang",
     "#version 450\n"
     "layout(set = 0, binding = 3) uniform sampler2D PassFeedback1;\n"
     "void main() {}\n", 0 },
   /* the last pass, asking for a format */
   { "format_last.slang",
     "#version 450\n"
     "#pragma format R16G16B16A16_SFLOAT\n"
     "void main() {}\n", 0 },
   /* the last pass, asking for a format from an include */
   { "format_inc_last.slang",
     "#version 450\n"
     "#include \"format.inc\"\n"
     "void main() {}\n", 0 },
   { "format.inc",
     "#pragma format R8G8B8A8_SRGB\n", 0 }
};

static struct source *source_find(const char *name)
{
   unsigned i;
   for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
      if (!strcmp(sources[i].name, name))
         return &sources[i];
   return NULL;
}

bool video_shader_source_read(const char *ident, char **buf, int64_t *len)
{
   struct source *src = ident ? source_find(ident) : NULL;
   size_t         n;
   if (!src || !buf)
      return false;
   n = strlen(src->text);
   if (!(*buf = (char*)malloc(n + 1)))
      return false;
   memcpy(*buf, src->text, n + 1);
   if (len)
      *len = (int64_t)n;
   src->reads++;
   return true;
}

bool video_shader_source_resolve(const char *parent, const char *name,
      char *s, size_t len)
{
   (void)parent;
   if (!name || !*name || !s || !len || strlen(name) >= len)
      return false;
   strcpy(s, name);
   return true;
}

const char *video_shader_source_ident_name(const char *ident)
{
   return (ident && *ident) ? ident : NULL;
}

bool video_shader_source_ident_is_slang(const char *ident)
{
   size_t n = ident ? strlen(ident) : 0;
   return n > 6 && !strcmp(ident + n - 6, ".slang");
}

/* ---- the presets ---- */

/* A two-pass preset, @first then @last; the last pass is given
 * scale_type = viewport at 1.0, as the parser leaves it. */
static void preset(struct video_shader *s, const char *first,
      const char *last)
{
   memset(s, 0, sizeof(*s));
   s->passes = 2;
   strcpy(s->pass[0].source.path, first);
   strcpy(s->pass[1].source.path, last);
   s->pass[1].fbo.flags   = FBO_SCALE_FLAG_VALID;
   s->pass[1].fbo.type_x  = RARCH_SCALE_VIEWPORT;
   s->pass[1].fbo.type_y  = RARCH_SCALE_VIEWPORT;
   s->pass[1].fbo.scale_x = 1.0f;
   s->pass[1].fbo.scale_y = 1.0f;
}

static void expect(const struct video_shader *s, bool needs,
      const char *tag)
{
   struct glslang_last_pass state;
   bool got;
   glslang_last_pass_init(s, NULL, &state);
   got = state.needs_target;
   CHECK(got == needs, tag);
   printf("      %-52s %s\n", tag, got ? "target" : "direct");
}

static void reset_reads(void)
{
   unsigned i;
   for (i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
      sources[i].reads = 0;
}

int main(void)
{
   struct video_shader *s = (struct video_shader*)calloc(1, sizeof(*s));

   if (!s)
   {
      printf("[fail] slang_last_pass_target_test: out of memory\n");
      return 1;
   }

   /* Scale */
   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.flags = 0;
   expect(s, false, "no scale on the last pass");

   /* An unscaled last pass may read its own feedback, which needs the
    * target that only the compiled pass can tell it about; it takes no
    * format, from the preset or the shader. */
   {
      struct glslang_last_pass state;
      preset(s, "first.slang", "last.slang");
      s->pass[1].fbo.flags = 0;
      glslang_last_pass_init(s, NULL, &state);
      CHECK(state.inspect_feedback && !state.needs_target,
            "an unscaled last pass waits on its compiled feedback");
      preset(s, "first.slang", "format_last.slang");
      s->pass[1].fbo.flags = FBO_SCALE_FLAG_FP_FBO;
      expect(s, false, "no scale: no format from the preset or shader");
   }

   preset(s, "first.slang", "last.slang");
   expect(s, false, "viewport at 1.0");

   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.scale_x = 2.0f;
   expect(s, true,  "viewport at 2.0 across");

   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.scale_y = 0.5f;
   expect(s, true,  "viewport at 0.5 down");

   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.type_x = RARCH_SCALE_INPUT;
   expect(s, true,  "source across, viewport down");

   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.type_y = RARCH_SCALE_ABSOLUTE;
   s->pass[1].fbo.abs_y  = 240;
   expect(s, true,  "viewport across, absolute down");

   /* Format, from the preset */
   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.flags |= FBO_SCALE_FLAG_FP_FBO;
   expect(s, true,  "float_framebuffer");

   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.flags |= FBO_SCALE_FLAG_SRGB_FBO;
   expect(s, true,  "srgb_framebuffer");

   preset(s, "first.slang", "last.slang");
   s->pass[1].fbo.flags |= FBO_SCALE_FLAG_RGB10_FBO;
   expect(s, true,  "rgb10_framebuffer");

   /* Format, from the shader */
   preset(s, "first.slang", "format_last.slang");
   expect(s, true,  "#pragma format in the last pass");

   preset(s, "first.slang", "format_inc_last.slang");
   expect(s, true,  "#pragma format in its include");

   preset(s, "format_last.slang", "last.slang");
   expect(s, false, "#pragma format in an earlier pass");

   /* Names are resolved before deferred compilation, without scanning GLSL. */
   {
      struct glslang_last_pass state;
      preset(s, "reads_name.slang", "named_last.slang");
      glslang_last_pass_init(s, NULL, &state);
      CHECK(state.inspect_feedback && !state.needs_target,
            "feedback is decided from compiled resources");
      CHECK(!strcmp(state.name, "Final"), "last pass pragma name");
      strcpy(s->pass[1].alias, "Other");
      glslang_last_pass_init(s, NULL, &state);
      CHECK(!strcmp(state.name, "Other"), "preset alias overrides pragma name");
   }

   /* A pass that cannot be read keeps the target; its compile reports. */
   preset(s, "first.slang", "missing.slang");
   expect(s, true,  "the last pass cannot be read");

   preset(s, "missing.slang", "last.slang");
   expect(s, false, "earlier sources are not scanned before compilation");

   /* One cache over the check reads each file once. */
   {
      void *cache = glslang_include_cache_new();
      CHECK(cache != NULL, "an include cache");
      if (cache)
      {
         preset(s, "first.slang", "last.slang");
         reset_reads();
         {
         struct glslang_last_pass state;
         glslang_last_pass_init(s, cache, &state);
         CHECK(!state.needs_target,
               "viewport at 1.0, through the caller's cache");
         CHECK(source_find("common.inc")->reads == 1,
               "the shared include is read once for the whole check");
         }
         glslang_include_cache_free(cache);
         printf("      %-52s done\n", "the caller's cache");
      }
   }

   failures += test_compiled_feedback();
   free(s);

   if (failures)
   {
      printf("[fail] slang_last_pass_target_test: %u check(s) failed\n",
            failures);
      return 1;
   }
   printf("[pass] slang_last_pass_target_test\n");
   return 0;
}
