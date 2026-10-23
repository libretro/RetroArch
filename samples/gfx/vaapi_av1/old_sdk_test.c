/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <va/va.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef VA_CHECK_VERSION
#define VA_CHECK_VERSION(major, minor, micro) 0
#define VAProfileAV1Profile0 RARCH_UNAVAILABLE_AV1_PROFILE
#define VAAPI_HEVC_RUNTIME_TEST
#define main hevc_old_sdk_contract
#include "../vaapi_hevc/session_test.c"
#undef main
#include "../../../gfx/common/vaapi_hevc_runtime.c"
static int node_open(const char *path, unsigned major_id, unsigned minor_id)
{
   (void)path;
   (void)major_id;
   (void)minor_id;
   assert(0);
   return -1;
}
static void node_close(int fd)
{
   (void)fd;
   assert(0);
}
static dylib_t load(const char *name)
{
   (void)name;
   assert(0);
   return NULL;
}
static function_t symbol(dylib_t library, const char *name)
{
   (void)library;
   (void)name;
   assert(0);
   return NULL;
}
static void unload(dylib_t library)
{
   (void)library;
   assert(0);
}
int main(void)
{
   vaapi_av1_session_t session, session_before;
   vaapi_av1_runtime_t runtime, runtime_before;
   static const vaapi_av1_runtime_io_t io
         = {node_open, node_close, load, symbol, unload};
   assert(!hevc_old_sdk_contract());
   memset(&session, 0, sizeof(session));
   memset(&runtime, 0, sizeof(runtime));
   session_before = session;
   runtime_before = runtime;
   assert(!vaapi_av1_session_open(
         &session, (VADisplay)(uintptr_t)1, NULL, 64, 64, 8, 9));
   assert(!memcmp(&session, &session_before, sizeof(session)));
   assert(!vaapi_av1_runtime_open_with_io(
         &runtime, "/render-test", 226, 128, 64, 64, 8, 9, &io));
   assert(!memcmp(&runtime, &runtime_before, sizeof(runtime)));
   puts("PASS: older VA SDK refuses AV1 before native ownership; AV1 profile "
        "token unavailable");
   return 0;
}
