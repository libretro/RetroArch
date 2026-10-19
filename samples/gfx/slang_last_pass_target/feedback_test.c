/* Compile real stages before deciding whether feedback needs a target. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx/drivers_shader/slang_process.h"

static unsigned failures;
static const char vertex[] =
   "#version 450\nvoid main() { gl_Position = vec4(0.0); }\n";

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
   ok = glslang_compile_spirv(vertex, GLSLANG_COMPILE_STAGE_VERTEX,
            &output.vertex, &output.vertex_len)
      && glslang_compile_spirv(source, GLSLANG_COMPILE_STAGE_FRAGMENT,
            &output.fragment, &output.fragment_len)
      && slang_last_pass_feedback(&state, &output);
   if (!ok || state.needs_target != needs)
   {
      printf("FAIL compiled feedback: %s (valid %u, target %u, expected %u)\n",
            label, (unsigned)ok, (unsigned)state.needs_target, (unsigned)needs);
      failures++;
   }
   else
      printf("      compiled %-43s %s\n", label, needs ? "target" : "direct");
   free(output.vertex);
   free(output.fragment);
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
      const uint32_t invalid[] = { 0 };
      bool ok;
      memset(&state, 0, sizeof(state));
      memset(&output, 0, sizeof(output));
      state.index = 1;
      state.inspect_feedback = true;
      strcpy(state.name, "Final");
      ok = glslang_compile_spirv("#version 450\n"
            "layout(push_constant) uniform Params { vec4 FinalFeedbackSize; } p;"
            "void main() { gl_Position = p.FinalFeedbackSize; }",
            GLSLANG_COMPILE_STAGE_VERTEX, &output.vertex, &output.vertex_len)
         && glslang_compile_spirv("#version 450\n"
            "layout(location=0) out vec4 Color; void main() { Color=vec4(1.0); }",
            GLSLANG_COMPILE_STAGE_FRAGMENT, &output.fragment, &output.fragment_len)
         && slang_last_pass_feedback(&state, &output);
      if (!ok || !state.needs_target)
      {
         printf("FAIL vertex-stage feedback size\n");
         failures++;
      }
      free(output.vertex);
      state.needs_target = false;
      output.vertex = (uint32_t*)invalid;
      output.vertex_len = 1;
      if (slang_last_pass_feedback(&state, &output))
      {
         printf("FAIL malformed SPIR-V must refuse analysis\n");
         failures++;
      }
      free(output.fragment);
   }
   return failures;
}
