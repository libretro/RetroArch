#if defined(__SWITCH2__) && defined(HAVE_DYNAMIC)

#include <dynamic/dylib.h>
#include <switch2/runtime/module.h>

dylib_t dylib_load(const char *path)
{
   ModuleLoadOptions options;
   options.required_symbol = "retro_api_version";
#ifdef HAVE_LIBOUNCE_MODULE_TRACE
   options.trace_path = "sdmc:/retroarch/module-loader.log";
#else
   options.trace_path = NULL;
#endif
   return moduleLoad(path, &options);
}

function_t dylib_proc(dylib_t handle, const char *name)
{
   return moduleGetFunction((Module*)handle, name);
}

void dylib_close(dylib_t handle)
{
   moduleClose((Module*)handle);
}

char *dylib_error(void)
{
   return moduleGetError();
}
#endif
