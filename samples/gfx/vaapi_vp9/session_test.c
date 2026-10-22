/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define main hevc_session_contract_suite
#include "../vaapi_hevc/session_test.c"
#undef main
#include "../../../gfx/common/vaapi_vp9_session.h"
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
int main(void)
{
   vaapi_vp9_session_ops_t native;
   int f;
   assert(!hevc_session_contract_suite());
   native = ops;
   native.profiles = vp9_profiles;
   native.entrypoints = vp9_entrypoints;
   native.attributes = vp9_attributes;
   native.create_config = vp9_config;
   for (depth = 8; depth <= 10; depth += 2)
   {
      for (f = NONE; f < FAULT_END; f++)
      {
         reset();
         expected_slots = 10;
         fault = (enum fault)f;
         assert(vaapi_vp9_session_open(&session, (VADisplay)(uintptr_t)1,
                      &native, 64, 64, depth, 10)
               == (f == NONE));
         if (f == NONE)
            assert(session.profile
                  == (depth == 10 ? VAProfileVP9Profile2
                                  : VAProfileVP9Profile0));
         assert(vaapi_vp9_session_close(&session));
         assert(vaapi_vp9_session_close(&session));
      }
      reset();
      expected_slots = 10;
      assert(!vaapi_vp9_session_open(
                   &session, (VADisplay)(uintptr_t)1, &native, 64, 64, depth, 9)
            && !calls);
      max_width = 32;
      assert(!vaapi_vp9_session_open(&session, (VADisplay)(uintptr_t)1, &native,
                   64, 64, depth, 10)
            && !configs);
      assert(vaapi_vp9_session_close(&session));
      reset();
      expected_slots = 10;
      allocation_fail = 1;
      assert(!vaapi_vp9_session_open(&session, (VADisplay)(uintptr_t)1, &native,
                   64, 64, depth, 10)
            && !calls);
      assert(vaapi_vp9_session_close(&session));
      reset();
      expected_slots = 10;
      assert(vaapi_vp9_session_open(
            &session, (VADisplay)(uintptr_t)1, &native, 64, 64, depth, 10));
      fail_context_drop = 1;
      assert(!vaapi_vp9_session_close(&session) && session.initialized);
      assert(vaapi_vp9_session_close(&session));
   }
   puts("PASS: VA VP9 profile/depth admission, partial startup and retirement");
   return 0;
}
