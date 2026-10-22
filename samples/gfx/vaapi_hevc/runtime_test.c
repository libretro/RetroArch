/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main vaapi_session_reference_main
#include "session_test.c"
#undef main
#define main vaapi_submit_reference_main
#include "submit_test.c"
#undef main
#define VAAPI_HEVC_RUNTIME_TEST
#include "../../../gfx/common/vaapi_hevc_runtime.c"
static vaapi_hevc_runtime_t runtime,exports;
static unsigned loads,unloads,lookups,fd_closes,display_calls,init_calls,term_calls;
static unsigned missing_symbol,failed_load,term_failures;
static int fd_live,display_live,driver_live,node_kind,display_fail,init_fail,bad_version,fd_value;
static int node_open(const char *path,unsigned major_id,unsigned minor_id)
{
 assert(!fd_live && !strcmp(path,"/render-test"));
 if(major_id!=226 || minor_id!=128)return -1;
 fd_live=1;return fd_value;
}
static void node_close(int fd)
{assert(fd_live && fd==fd_value && !display_live && loads==unloads);fd_live=0;fd_closes++;}
static dylib_t load(const char *name)
{
 static const char *const names[]={"libva.so.2","libva-drm.so.2","libdrm.so.2"};
 assert(fd_live && loads<3 && !strcmp(name,names[loads]));
 if(failed_load==loads+1)return NULL;
 loads++;return (dylib_t)(uintptr_t)loads;
}
static void unload(dylib_t lib)
{
 assert(!display_live && !driver_live && !runtime.session.initialized);
 assert((unsigned)(uintptr_t)lib==loads-unloads);unloads++;
}
static VADisplay get_display(int fd)
{
 assert(fd_live && fd==fd_value && loads==3 && !unloads);display_calls++;
 if(display_fail)return NULL;
 display_live=1;return (VADisplay)(uintptr_t)1;
}
static int node_type(int fd)
{assert(fd_live && fd==fd_value);return node_kind;}
static VAStatus initialize(VADisplay display,int *major_out,int *minor_out)
{
 assert(display==(VADisplay)(uintptr_t)1 && display_live && !driver_live);
 init_calls++;*major_out=bad_version?VA_MAJOR_VERSION+1:VA_MAJOR_VERSION;*minor_out=0;
 if(init_fail)return VA_STATUS_ERROR_OPERATION_FAILED;
 driver_live=1;return VA_STATUS_SUCCESS;
}
static VAStatus terminate(VADisplay display)
{
 unsigned i;
 assert(display==(VADisplay)(uintptr_t)1 && display_live && fd_live && !unloads);
 assert(runtime.terminating && runtime.closing && !runtime.ready);term_calls++;
 /* libva can unload the driver even when its termination fails. */
 driver_live=live_config=live_context=0;for(i=0;i<32;i++)live_surfaces[i]=0;
 if(term_failures){term_failures--;return VA_STATUS_ERROR_OPERATION_FAILED;}
 display_live=0;return VA_STATUS_SUCCESS;
}
static function_t symbol_value(const void *value,size_t bytes)
{
 function_t result;
 assert(bytes==sizeof(result));memcpy(&result,value,bytes);return result;
}
static function_t symbol(dylib_t lib,const char *name)
{
 lookups++;if(missing_symbol==lookups)return NULL;
#define EXPORTED(name_string,member,library) if(!strcmp(name,name_string)) { \
 assert((uintptr_t)lib==library);return symbol_value(&exports.member,sizeof(exports.member)); }
 EXPORTED("vaCreateBuffer",submit.create,1)
 EXPORTED("vaDestroyBuffer",submit.destroy,1)
 EXPORTED("vaBeginPicture",submit.begin,1)
 EXPORTED("vaRenderPicture",submit.render,1)
 EXPORTED("vaEndPicture",submit.end,1)
 EXPORTED("vaSyncSurface2",submit.sync,1)
 EXPORTED("vaGetDisplayDRM",get_display,2)
 EXPORTED("vaInitialize",initialize,1)
 EXPORTED("vaTerminate",terminate,1)
 EXPORTED("drmGetNodeTypeFromFd",node_type,3)
 EXPORTED("vaMaxNumProfiles",session.ops.max_profiles,1)
 EXPORTED("vaQueryConfigProfiles",session.ops.profiles,1)
 EXPORTED("vaMaxNumEntrypoints",session.ops.max_entrypoints,1)
 EXPORTED("vaQueryConfigEntrypoints",session.ops.entrypoints,1)
 EXPORTED("vaGetConfigAttributes",session.ops.attributes,1)
 EXPORTED("vaCreateConfig",session.ops.create_config,1)
 EXPORTED("vaDestroyConfig",session.ops.destroy_config,1)
 EXPORTED("vaQuerySurfaceAttributes",session.ops.surface_attributes,1)
 EXPORTED("vaCreateSurfaces",session.ops.create_surfaces,1)
 EXPORTED("vaDestroySurfaces",session.ops.destroy_surfaces,1)
 EXPORTED("vaCreateContext",session.ops.create_context,1)
 EXPORTED("vaDestroyContext",session.ops.destroy_context,1)
#undef EXPORTED
 assert(0);return NULL;
}
static const vaapi_hevc_runtime_io_t io={node_open,node_close,load,symbol,unload};
static void runtime_reset(void)
{
 assert(!runtime.active && !fd_live && !display_live && !driver_live);reset();
 memset(&runtime,0,sizeof(runtime));memset(&exports,0,sizeof(exports));
 exports.session.ops=ops;exports.submit=ss_ops;exports.get_display=get_display;exports.initialize=initialize;
 exports.terminate=terminate;exports.node_type=node_type;
 loads=unloads=lookups=fd_closes=display_calls=init_calls=term_calls=0;
 missing_symbol=failed_load=term_failures=0;
 node_kind=VAAPI_HEVC_RENDER_NODE;display_fail=init_fail=bad_version=0;fd_value=7;
}
static bool runtime_open(void)
{
 return vaapi_hevc_runtime_open_with_io(&runtime,"/render-test",226,128,64,64,
       depth,expected_slots,mode,&io);
}
static void runtime_cases(void)
{
 unsigned i,before;
 runtime_reset();assert(runtime_open() && runtime.ready && runtime.session.ready && lookups==22);
 assert(vaapi_hevc_runtime_close(&runtime) && term_calls==1 && fd_closes==1 && unloads==3);
 assert(vaapi_hevc_runtime_close(&runtime) && term_calls==1 && fd_closes==1);
 for(i=1;i<=22;i++) {
  runtime_reset();missing_symbol=i;assert(!runtime_open() && !display_calls);
  assert(vaapi_hevc_runtime_close(&runtime) && loads==unloads && fd_closes==1);
 }
 for(i=1;i<=3;i++) {
  runtime_reset();failed_load=i;assert(!runtime_open() && !display_calls);
  assert(vaapi_hevc_runtime_close(&runtime) && loads==unloads && fd_closes==1);
 }
 runtime_reset();node_kind=0;assert(!runtime_open() && !display_calls);
 assert(vaapi_hevc_runtime_close(&runtime) && !term_calls);
 runtime_reset();display_fail=1;assert(!runtime_open() && !init_calls);
 assert(vaapi_hevc_runtime_close(&runtime) && !term_calls);
 runtime_reset();init_fail=1;assert(!runtime_open() && display_live && !configs);
 assert(vaapi_hevc_runtime_close(&runtime) && term_calls==1 && fd_closes==1);
 runtime_reset();bad_version=1;assert(!runtime_open() && display_live && !configs);
 assert(vaapi_hevc_runtime_close(&runtime) && term_calls==1);
 runtime_reset();fault=CONTEXT;assert(!runtime_open() && runtime.session.initialized);
 assert(vaapi_hevc_runtime_close(&runtime) && term_calls==1 && context_drops==1);
 runtime_reset();allocation_fail=1;assert(!runtime_open());
 assert(vaapi_hevc_runtime_close(&runtime) && term_calls==1 && !configs);
 runtime_reset();assert(runtime_open());fail_context_drop=1;term_failures=2;
 assert(!vaapi_hevc_runtime_close(&runtime) && fd_live && !unloads && !driver_live);
 before=calls;
 assert(!vaapi_hevc_runtime_close(&runtime) && calls==before && fd_live && !unloads);
 assert(vaapi_hevc_runtime_close(&runtime) && calls==before && term_calls==3 && fd_closes==1);
 runtime_reset();fd_value=0;runtime.io=io;
 assert(vaapi_hevc_runtime_open_with_io(&runtime,"/render-test",226,128,64,64,depth,4,mode,&runtime.io));
 assert(vaapi_hevc_runtime_close(&runtime) && fd_closes==1);
 runtime_reset();assert(!vaapi_hevc_runtime_open_with_io(&runtime,"/render-test",226,129,64,64,depth,4,mode,&io));
 assert(vaapi_hevc_runtime_close(&runtime) && !loads && !fd_closes);
}
int main(void)
{
 for(depth=8;depth<=10;depth+=2)
  for(mode=VA_DEC_SLICE_MODE_NORMAL;mode<=VA_DEC_SLICE_MODE_BASE;mode++)runtime_cases();
 puts("PASS: VA/DRM runtime bindings, partial initialization, FD/library ownership and termination retry; no device execution");
 return 0;
}
