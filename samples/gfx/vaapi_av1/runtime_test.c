/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_RUNTIME_TEST_MAIN hevc_runtime_contract_suite
#include "../vaapi_hevc/runtime_test.c"
static VAStatus av1_profiles(VADisplay d, VAProfile *p, int *n)
{
   check_display(d);
   assert(*n == 2);
   p[0] = fault == NO_PROFILE ? VAProfileH264High : VAProfileAV1Profile0;
   p[1] = VAProfileH264High;
   *n = fault == PROFILE_COUNT ? 257 : 2;
   return fault == PROFILE_QUERY ? VA_STATUS_ERROR_OPERATION_FAILED
                                 : VA_STATUS_SUCCESS;
}
static VAStatus av1_entrypoints(
      VADisplay d, VAProfile p, VAEntrypoint *entries, int *n)
{
   check_display(d);
   assert(p == VAProfileAV1Profile0);
   assert(*n == 2);
   entries[0] = fault == NO_ENTRY ? VAEntrypointEncSlice : VAEntrypointVLD;
   entries[1] = VAEntrypointEncSlice;
   *n = 2;
   return fault == ENTRY_QUERY ? VA_STATUS_ERROR_OPERATION_FAILED
                               : VA_STATUS_SUCCESS;
}
static VAStatus av1_attributes(VADisplay d, VAProfile p, VAEntrypoint entry,
      VAConfigAttrib *attrs, int n)
{
   check_display(d);
   assert(p == VAProfileAV1Profile0);
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
static VAStatus av1_config(VADisplay d, VAProfile p, VAEntrypoint entry,
      VAConfigAttrib *attrs, int n, VAConfigID *out)
{
   check_display(d);
   assert(p == VAProfileAV1Profile0);
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

static void av1_runtime_reset(void)
{
   runtime_reset();
   expected_slots = 9;
   exports.session.ops.profiles = av1_profiles;
   exports.session.ops.entrypoints = av1_entrypoints;
   exports.session.ops.attributes = av1_attributes;
   exports.session.ops.create_config = av1_config;
}
static bool av1_runtime_open(void)
{
   return vaapi_av1_runtime_open_with_io(
         &runtime, "/render-test", 226, 128, 64, 64, depth, 9, &io);
}
int main(void)
{
   unsigned i, before;
   int f;
   assert(!hevc_runtime_contract_suite());
   for (depth = 8; depth <= 10; depth += 2)
   {
      av1_runtime_reset();
      assert(av1_runtime_open() && runtime.ready && lookups == 22);
      assert(runtime.session.profile == VAProfileAV1Profile0);
      assert(vaapi_av1_runtime_close(&runtime) && fd_closes == 1);
      for (i = 1; i <= 22; i++)
      {
         av1_runtime_reset();
         missing_symbol = i;
         assert(!av1_runtime_open() && !display_calls);
         assert(vaapi_av1_runtime_close(&runtime) && loads == unloads);
      }
      for (f = NONE; f < FAULT_END; f++)
      {
         av1_runtime_reset();
         fault = (enum fault)f;
         assert(av1_runtime_open() == (f == NONE));
         assert(vaapi_av1_runtime_close(&runtime) && fd_closes == 1);
      }
      av1_runtime_reset();
      assert(av1_runtime_open());
      fail_context_drop = 1;
      term_failures = 2;
      assert(!vaapi_av1_runtime_close(&runtime) && !unloads && fd_live);
      before = calls;
      assert(!vaapi_av1_runtime_close(&runtime) && calls == before);
      assert(vaapi_av1_runtime_close(&runtime) && calls == before);
      assert(term_calls == 3 && fd_closes == 1 && unloads == 3);
      av1_runtime_reset();
      assert(!vaapi_av1_runtime_open_with_io(
            &runtime, "/render-test", 226, 129, 64, 64, depth, 9, &io));
      assert(vaapi_av1_runtime_close(&runtime) && !loads && !fd_closes);
      av1_runtime_reset();
      assert(!vaapi_av1_runtime_open_with_io(
                   &runtime, "/render-test", 226, 128, 64, 64, depth, 8, &io)
            && !runtime.active && !loads);
   }
   depth = 8;
   av1_runtime_reset();
   expected_slots = 32;
   assert(vaapi_av1_runtime_open_with_io(
         &runtime, "/render-test", 226, 128, 64, 64, depth, 32, &io));
   assert(vaapi_av1_runtime_close(&runtime));
   av1_runtime_reset();
   assert(!vaapi_av1_runtime_open_with_io(
                &runtime, "/render-test", 226, 128, 64, 64, 12, 9, &io)
         && !loads && !runtime.active);
   puts("PASS: AV1 VA/DRM runtime binding, profile admission, partial startup "
        "and termination retry; no device execution");
   return 0;
}
