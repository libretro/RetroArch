/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (slang_texture_index_bounds_test.c).
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

/* Regression test for the texture-semantic index-bounds fix in
 * gfx/drivers_shader/slang_process.cpp::
 *   validate_texture_semantic_index().
 *
 * Pre-fix only the SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT semantic was
 * bounded against reflection->pass_number; the other arrayed
 * semantics (ORIGINAL_HISTORY, PASS_FEEDBACK, USER) had no upper
 * bound on their array index.  The index suffix in arrayed
 * semantic names like `OriginalHistory42` / `PassFeedback9` /
 * `User7` is parsed via strtoul in
 * slang_name_to_texture_semantic_array() and propagates into the
 * downstream resize_minimum() at slang_process.cpp:1116 (UBO
 * uniform path) and the direct-binding loop in slang_reflect()
 * (sampler binding path).  A malicious slang shader declaring
 *
 *    layout(set = 0, binding = 1) uniform sampler2D
 *       OriginalHistory4294967294;
 *
 * makes resize_minimum() call std::vector::resize(4294967295).
 * With slang_texture_semantic_meta at ~32 bytes, that is ~128 GiB
 * which std::vector throws std::bad_alloc on.  The exception
 * propagates out of slang_reflect_spirv() up to
 * vulkan_filter_chain_create_from_preset() with no try/catch, and
 * the unhandled C++ exception terminates RetroArch.
 *
 * Reachability: any malicious slang shader preset.  Slang preset
 * packs are downloaded from the Online Updater and shipped third-
 * party; this is the same threat surface as the .bsv replay file,
 * .bps soft-patch, and image-decoder bugs in 7335b37.
 *
 * Fix: extend the existing PASS_OUTPUT bound check to cover all
 * arrayed semantics, with their natural caps:
 *   PASS_OUTPUT     -> reflection->pass_number   (unchanged)
 *   PASS_FEEDBACK   -> GFX_MAX_SHADERS           (64)
 *   ORIGINAL_HISTORY-> GFX_MAX_FRAME_HISTORY     (128)
 *   USER            -> GFX_MAX_TEXTURES          (64)
 * Non-arrayed semantics (Original, Source) carry index 0 by
 * construction and are accepted unconditionally.
 *
 * IMPORTANT: this test keeps a verbatim copy of
 * validate_texture_semantic_index() from slang_process.cpp.  If
 * the production function amends the cap table or the dispatch
 * structure, the copy below must follow.  Convention used by the
 * security regression tests in samples/tasks/ and the v3 vulkan/
 * tests in this directory.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Mirror the production semantic enum.  Order matters because
 * the production texture_semantic_names[] array is indexed by
 * enum value and the message uses it.  If
 * gfx/drivers_shader/glslang.hpp amends the enum, this mirror
 * must follow. */
enum slang_texture_semantic
{
   SLANG_TEXTURE_SEMANTIC_ORIGINAL = 0,
   SLANG_TEXTURE_SEMANTIC_SOURCE,
   SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY,
   SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT,
   SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK,
   SLANG_TEXTURE_SEMANTIC_USER,
   SLANG_INVALID_TEXTURE_SEMANTIC = -1
};

/* Mirror the production caps from gfx/video_shader_parse.h.
 * If those macros change, this mirror must follow. */
#define GFX_MAX_SHADERS         64
#define GFX_MAX_TEXTURES        64
#define GFX_MAX_FRAME_HISTORY   128

/* Minimal stand-in for slang_reflection.  Production has a much
 * larger struct; the validator only consults pass_number. */
struct mock_reflection
{
   unsigned pass_number;
};

/* Mirror of the production texture_semantic_names array, used by
 * the validator's error log.  If glslang_util.c amends the order
 * or contents, this mirror must follow. */
static const char *texture_semantic_names[] = {
   "Original",
   "Source",
   "OriginalHistory",
   "PassOutput",
   "PassFeedback",
   "User",
   NULL
};

/* Test-side stand-in for RARCH_ERR.  Production logs to the
 * verbosity layer; we route to stderr so the verbatim copy
 * still exercises its varargs (and the texture_semantic_names
 * pointer dereference is observed under ASan), while a flag
 * tracks whether a rejection log fired this call. */
static int last_log_was_rejection = 0;
#define RARCH_ERR(...) do {                          \
   last_log_was_rejection = 1;                       \
   fprintf(stderr, "  [log] ");                      \
   fprintf(stderr, __VA_ARGS__);                     \
} while (0)

/* === verbatim copy of the post-fix validate_texture_semantic_index
 *     from gfx/drivers_shader/slang_process.cpp.  If the production
 *     function amends the cap table or the dispatch structure, this
 *     copy must follow. === */
static bool validate_texture_semantic_index(struct mock_reflection *reflection,
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
/* === end verbatim copy === */

static int failures = 0;

/* Helper: run the predicate, optionally checking that the
 * rejection-log was tripped (meaning production would have
 * RARCH_ERR'd). */
static void run_case(const char *desc,
      enum slang_texture_semantic sem, unsigned pass_number,
      unsigned index, bool expect_accept)
{
   struct mock_reflection refl = { pass_number };
   bool rv;

   last_log_was_rejection = 0;
   rv = validate_texture_semantic_index(&refl, sem, index);

   if (rv != expect_accept)
   {
      printf("[ERROR] %s: got %s, expected %s\n",
            desc,
            rv ? "accept" : "reject",
            expect_accept ? "accept" : "reject");
      failures++;
      return;
   }
   if (!expect_accept && !last_log_was_rejection)
   {
      printf("[ERROR] %s: rejected but no log line emitted\n", desc);
      failures++;
      return;
   }
   if (expect_accept && last_log_was_rejection)
   {
      printf("[ERROR] %s: accepted but log line emitted\n", desc);
      failures++;
      return;
   }
   printf("[SUCCESS] %s: %s as expected\n",
         desc, expect_accept ? "accepted" : "rejected");
}

/* Probe: legitimate uses across all four arrayed semantics.
 * Each well within its respective cap. */
static void test_legitimate_in_bounds(void)
{
   /* Pass 5 in a 6-pass chain reads PassOutput0..4. */
   run_case("PASS_OUTPUT 0 in pass 5",
         SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, 5, 0, true);
   run_case("PASS_OUTPUT 4 in pass 5 (boundary minus one)",
         SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, 5, 4, true);

   /* History rings of size 7 (typical CRT shader). */
   run_case("ORIGINAL_HISTORY 6",
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, 6, true);
   run_case("ORIGINAL_HISTORY GFX_MAX_FRAME_HISTORY-1",
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0,
         GFX_MAX_FRAME_HISTORY - 1, true);

   /* Feedback from earlier pass. */
   run_case("PASS_FEEDBACK 3",
         SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, 0, 3, true);
   run_case("PASS_FEEDBACK GFX_MAX_SHADERS-1",
         SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, 0,
         GFX_MAX_SHADERS - 1, true);

   /* User LUT 0..N. */
   run_case("USER 0",
         SLANG_TEXTURE_SEMANTIC_USER, 0, 0, true);
   run_case("USER GFX_MAX_TEXTURES-1",
         SLANG_TEXTURE_SEMANTIC_USER, 0,
         GFX_MAX_TEXTURES - 1, true);
}

/* Probe: index exactly equal to the cap on each semantic --
 * boundary that should reject. */
static void test_at_boundary_rejected(void)
{
   run_case("PASS_OUTPUT 5 in pass 5 (== cap)",
         SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, 5, 5, false);
   run_case("ORIGINAL_HISTORY GFX_MAX_FRAME_HISTORY",
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0,
         GFX_MAX_FRAME_HISTORY, false);
   run_case("PASS_FEEDBACK GFX_MAX_SHADERS",
         SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, 0,
         GFX_MAX_SHADERS, false);
   run_case("USER GFX_MAX_TEXTURES",
         SLANG_TEXTURE_SEMANTIC_USER, 0,
         GFX_MAX_TEXTURES, false);
}

/* Probe: the smoking-gun cases.  Pre-fix these would have called
 * std::vector::resize(index+1) on the production reflection.
 * Index = 4_294_967_294 implies a 128 GiB allocation request. */
static void test_smoking_gun_uint32_max_minus_one(void)
{
   run_case("ORIGINAL_HISTORY UINT32_MAX-1 (smoking gun)",
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0,
         UINT32_MAX - 1u, false);
   run_case("PASS_FEEDBACK UINT32_MAX-1 (smoking gun)",
         SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, 0,
         UINT32_MAX - 1u, false);
   run_case("USER UINT32_MAX-1 (smoking gun)",
         SLANG_TEXTURE_SEMANTIC_USER, 0,
         UINT32_MAX - 1u, false);
   run_case("PASS_OUTPUT UINT32_MAX-1 (existing PASS_OUTPUT path)",
         SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, 5,
         UINT32_MAX - 1u, false);
}

/* Probe: PASS_OUTPUT keeps its dedicated `Non causal filter chain
 * detected` error message verbiage.  The other semantics use the
 * generic per-cap message.  Smoke-test that the dispatch fires
 * the right log path by checking the pre-existing PASS_OUTPUT
 * behaviour against an in-range index (accept) and an out-of-
 * range one (reject), then the same for ORIGINAL_HISTORY -- in
 * both cases the rejection-log flag should match the predicate
 * return value. */
static void test_log_dispatch_consistency(void)
{
   /* PASS_OUTPUT with pass_number=0: any index rejects. */
   run_case("PASS_OUTPUT 0 in pass 0 (cap=0)",
         SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, 0, 0, false);
   /* ORIGINAL_HISTORY at exactly the cap. */
   run_case("ORIGINAL_HISTORY at cap",
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0,
         GFX_MAX_FRAME_HISTORY, false);
}

/* Probe: non-arrayed semantics (Original, Source) accept any
 * index unconditionally.  Production sets index=0 for these by
 * construction in slang_name_to_texture_semantic_array, but the
 * validator must not reject if the caller mistakenly passes a
 * non-zero one. */
static void test_non_arrayed_semantics(void)
{
   run_case("ORIGINAL with index 0",
         SLANG_TEXTURE_SEMANTIC_ORIGINAL, 0, 0, true);
   run_case("ORIGINAL with index 42 (defensive)",
         SLANG_TEXTURE_SEMANTIC_ORIGINAL, 0, 42, true);
   run_case("SOURCE with index 0",
         SLANG_TEXTURE_SEMANTIC_SOURCE, 0, 0, true);
   run_case("SOURCE with index 99999 (defensive)",
         SLANG_TEXTURE_SEMANTIC_SOURCE, 0, 99999, true);
}

/* Probe: PASS_OUTPUT in the very first pass (pass_number = 0)
 * rejects every index, including 0.  This matches the pre-fix
 * predicate `index >= reflection->pass_number` for index=0,
 * pass_number=0: 0 >= 0 is true -> reject.  Confirms the v4
 * change preserved that semantics. */
static void test_pass_output_first_pass_rejects(void)
{
   run_case("PASS_OUTPUT 0 in pass 0 (the very first pass)",
         SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, 0, 0, false);
   run_case("PASS_OUTPUT 1 in pass 0",
         SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, 0, 1, false);
}

int main(void)
{
   test_legitimate_in_bounds();
   test_at_boundary_rejected();
   test_smoking_gun_uint32_max_minus_one();
   test_log_dispatch_consistency();
   test_non_arrayed_semantics();
   test_pass_output_first_pass_rejects();

   if (failures)
   {
      printf("\n%d slang_texture_index_bounds test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll slang_texture_index_bounds regression tests passed.\n");
   return 0;
}
