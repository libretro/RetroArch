/* Compile real stages before deciding whether feedback needs a target. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/drivers_shader/slang_process.h"
#include "gfx/drivers_shader/slang_blob.h"

static unsigned failures;
static const char vertex[] =
   "#version 450\nvoid main() { gl_Position = vec4(0.0); }\n";

/* Compiles one stage with rslc and hands the blob to the output, as
 * slang_process.c does. */
static bool compile_into(glslang_output *output, bool fragment, const char *src)
{
   struct slang_blob *blob = NULL;
   char              *text = NULL;
   if (!slang_compile_builtin(src, fragment, NULL, &blob, &text))
      return false;
   glslang_output_set_blob(output, fragment, blob);
   return fragment ? output->fragment != NULL : output->vertex != NULL;
}

static void probe(const char *label, const char *declarations,
      const char *expression, bool needs)
{
   char source[2048];
   struct glslang_last_pass state;
   glslang_output output;
   bool ok;
   memset(&state, 0, sizeof(state));
   memset(&output, 0, sizeof(output));
   state.index = 1;
   state.inspect_feedback = true;
   strcpy(state.name, "Final");
   snprintf(source, sizeof(source),
         "#version 450\n%s\nlayout(location = 0) out vec4 Color;\n"
         "void main() { Color = %s; }\n", declarations, expression);
   ok = compile_into(&output, false, vertex)
      && compile_into(&output, true, source)
      && slang_last_pass_feedback(&state, &output);
   if (!ok || state.needs_target != needs)
   {
      printf("FAIL compiled feedback: %s (valid %u, target %u, expected %u)\n",
            label, (unsigned)ok, (unsigned)state.needs_target, (unsigned)needs);
      failures++;
   }
   else
      printf("      compiled %-43s %s\n", label, needs ? "target" : "direct");
   glslang_output_free(&output);
}

unsigned test_compiled_feedback(void)
{
   probe("line comment", "// FinalFeedback is unused.", "vec4(1.0)", false);
   probe("block comment", "/* FinalFeedback\n PassFeedback1 */", "vec4(1.0)", false);
   probe("disabled branch", "#if 0\n#define X FinalFeedback\n#endif", "vec4(1.0)", false);
   probe("undefined conditional", "#define USE_FEEDBACK\n#undef USE_FEEDBACK\n"
         "#ifdef USE_FEEDBACK\n#define LAST FinalFeedback\n#else\n#define LAST Source\n#endif",
         "vec4(1.0)", false);
   probe("unused declaration", "layout(set=0,binding=1) uniform sampler2D FinalFeedback;",
         "vec4(1.0)", false);
   probe("uncalled function", "layout(set=0,binding=1) uniform sampler2D FinalFeedback;"
         "vec4 unused_function() { return texture(FinalFeedback, vec2(0.5)); }",
         "vec4(1.0)", false);
   probe("sampler handed to a called function", "layout(set=0,binding=1) uniform sampler2D FinalFeedback;"
         "vec4 at(sampler2D s) { return texture(s, vec2(0.5)); }",
         "at(FinalFeedback)", true);
   probe("unused macro", "#define UNUSED FinalFeedback", "vec4(1.0)", false);
   probe("other index", "layout(set=0,binding=1) uniform sampler2D PassFeedback0;",
         "texture(PassFeedback0, vec2(0.5))", false);
   probe("longer name", "layout(set=0,binding=1) uniform sampler2D FinalFeedbackX;",
         "texture(FinalFeedbackX, vec2(0.5))", false);
   probe("active canonical sampler", "layout(set=0,binding=1) uniform sampler2D PassFeedback1;",
         "texture(PassFeedback1, vec2(0.5))", true);
   probe("active alias sampler", "layout(set=0,binding=1) uniform sampler2D FinalFeedback;",
         "texture(FinalFeedback, vec2(0.5))", true);
   probe("token-pasted alias", "#define JOIN(a,b) a ## b\n"
         "layout(set=0,binding=1) uniform sampler2D JOIN(Final, Feedback);",
         "texture(JOIN(Final, Feedback), vec2(0.5))", true);
   probe("active UBO size", "layout(set=0,binding=0) uniform Params { vec4 PassFeedbackSize1; } p;",
         "p.PassFeedbackSize1", true);
   probe("active push size alias", "layout(push_constant) uniform Params { vec4 FinalFeedbackSize; } p;",
         "p.FinalFeedbackSize", true);
   probe("unused member of active UBO", "layout(set=0,binding=0) uniform Params "
         "{ vec4 FinalFeedbackSize; vec4 Other; } p;", "p.Other", false);
   probe("unused push member", "layout(push_constant) uniform Params "
         "{ vec4 FinalFeedbackSize; vec4 Other; } p;", "p.Other", false);
   {
      struct glslang_last_pass state;
      glslang_output output;
      uint32_t *bare;
      bool ok;
      memset(&state, 0, sizeof(state));
      memset(&output, 0, sizeof(output));
      state.index = 1;
      state.inspect_feedback = true;
      strcpy(state.name, "Final");
      ok = compile_into(&output, false, "#version 450\n"
            "layout(push_constant) uniform Params { vec4 FinalFeedbackSize; } p;"
            "void main() { gl_Position = p.FinalFeedbackSize; }")
         && compile_into(&output, true, "#version 450\n"
            "layout(location=0) out vec4 Color; void main() { Color=vec4(1.0); }")
         && slang_last_pass_feedback(&state, &output);
      if (!ok || !state.needs_target)
      {
         printf("FAIL vertex-stage feedback size\n");
         failures++;
      }
      state.needs_target = false;
      /* a vertex stage without tables in place of the real one: nothing
       * says what it reads, so it keeps the target */
      if ((bare = (uint32_t*)calloc(1, sizeof(*bare))))
         glslang_output_set_blob(&output, false, slang_blob_adopt_words(bare, 1));
      if (!bare || !slang_last_pass_feedback(&state, &output)
            || !state.needs_target)
      {
         printf("FAIL a stage without tables must keep the target\n");
         failures++;
      }
      glslang_output_free(&output);
   }
   return failures;
}
