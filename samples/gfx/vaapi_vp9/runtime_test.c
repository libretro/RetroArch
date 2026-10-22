/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_RUNTIME_TEST_MAIN hevc_runtime_contract_suite
#include "../vaapi_hevc/runtime_test.c"
static VAStatus vp9_profiles(VADisplay d, VAProfile *p, int *n)
{
   check_display(d);
   assert(*n == 2);
   p[0] = fault == NO_PROFILE ? VAProfileH264High : VAProfileVP9Profile0;
   p[1] = fault == NO_PROFILE ? VAProfileH264High : VAProfileVP9Profile2;
   *n = fault == PROFILE_COUNT ? 257 : 2;
   return fault == PROFILE_QUERY ? VA_STATUS_ERROR_OPERATION_FAILED
                                 : VA_STATUS_SUCCESS;
}
static VAStatus vp9_entrypoints(
      VADisplay d, VAProfile p, VAEntrypoint *entries, int *n)
{
   check_display(d);
   assert(p == (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0));
   assert(*n == 2);
   entries[0] = fault == NO_ENTRY ? VAEntrypointEncSlice : VAEntrypointVLD;
   entries[1] = VAEntrypointEncSlice;
   *n = 2;
   return fault == ENTRY_QUERY ? VA_STATUS_ERROR_OPERATION_FAILED
                               : VA_STATUS_SUCCESS;
}
static VAStatus vp9_attributes(VADisplay d, VAProfile p, VAEntrypoint entry,
      VAConfigAttrib *attrs, int n)
{
   check_display(d);
   assert(p == (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0));
   assert(entry == VAEntrypointVLD && n == 3);
   assert(attrs[0].type == VAConfigAttribRTFormat);
   assert(attrs[1].type == VAConfigAttribMaxPictureWidth);
   assert(attrs[2].type == VAConfigAttribMaxPictureHeight);
   attrs[0].value = rt_mask;
   attrs[1].value = max_width;
   attrs[2].value = 2160;
   return fault == ATTRIBUTES ? VA_STATUS_ERROR_OPERATION_FAILED
                              : VA_STATUS_SUCCESS;
}
static VAStatus vp9_config(VADisplay d, VAProfile p, VAEntrypoint entry,
      VAConfigAttrib *attrs, int n, VAConfigID *out)
{
   check_display(d);
   assert(p == (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0));
   assert(entry == VAEntrypointVLD && n == 1);
   assert(attrs[0].type == VAConfigAttribRTFormat
         && attrs[0].value
               == (depth == 10 ? VA_RT_FORMAT_YUV420_10 : VA_RT_FORMAT_YUV420));
   assert(!live_config);
   configs++;
   if (fault == CONFIG_EMPTY_SUCCESS)
      return VA_STATUS_SUCCESS;
   live_config = 1;
   *out = 11;
   return fault == CONFIG ? VA_STATUS_ERROR_OPERATION_FAILED
                          : VA_STATUS_SUCCESS;
}

static void vp9_runtime_reset(void)
{
   runtime_reset();
   expected_slots = 10;
   exports.session.ops.profiles = vp9_profiles;
   exports.session.ops.entrypoints = vp9_entrypoints;
   exports.session.ops.attributes = vp9_attributes;
   exports.session.ops.create_config = vp9_config;
}
static bool vp9_runtime_open(void)
{
   return vaapi_vp9_runtime_open_with_io(
         &runtime, "/render-test", 226, 128, 64, 64, depth, 10, &io);
}
int main(void)
{
   unsigned i, before;
   int f;
   assert(!hevc_runtime_contract_suite());
   for (depth = 8; depth <= 10; depth += 2)
   {
      vp9_runtime_reset();
      assert(vp9_runtime_open() && runtime.ready && lookups == 22);
      assert(runtime.session.profile
            == (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0));
      assert(vaapi_vp9_runtime_close(&runtime) && fd_closes == 1);
      for (i = 1; i <= 22; i++)
      {
         vp9_runtime_reset();
         missing_symbol = i;
         assert(!vp9_runtime_open() && !display_calls);
         assert(vaapi_vp9_runtime_close(&runtime) && loads == unloads);
      }
      for (f = NONE; f < FAULT_END; f++)
      {
         vp9_runtime_reset();
         fault = (enum fault)f;
         assert(vp9_runtime_open() == (f == NONE));
         assert(vaapi_vp9_runtime_close(&runtime) && fd_closes == 1);
      }
      vp9_runtime_reset();
      assert(vp9_runtime_open());
      fail_context_drop = 1;
      term_failures = 2;
      assert(!vaapi_vp9_runtime_close(&runtime) && !unloads && fd_live);
      before = calls;
      assert(!vaapi_vp9_runtime_close(&runtime) && calls == before);
      assert(vaapi_vp9_runtime_close(&runtime) && calls == before);
      assert(term_calls == 3 && fd_closes == 1 && unloads == 3);
      vp9_runtime_reset();
      assert(!vaapi_vp9_runtime_open_with_io(
            &runtime, "/render-test", 226, 129, 64, 64, depth, 10, &io));
      assert(vaapi_vp9_runtime_close(&runtime) && !loads && !fd_closes);
      vp9_runtime_reset();
      assert(!vaapi_vp9_runtime_open_with_io(
                   &runtime, "/render-test", 226, 128, 64, 64, depth, 9, &io)
            && !runtime.active && !loads);
   }
   puts("PASS: VP9 VA/DRM runtime binding, profile admission, partial startup "
        "and termination retry; no device execution");
   return 0;
}
