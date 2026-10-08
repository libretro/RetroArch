/* A stand-in OpenXR runtime for loader_threads_test: enough of the
 * runtime interface for the loader to negotiate, enumerate, create and
 * destroy instances. Enumeration dwells a little, so a runtime unload
 * on another thread lands while a call is still inside this library. */

#include <stdint.h>
#include <string.h>

#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#if defined(_WIN32)
#define STUB_EXPORT __declspec(dllexport)
#else
#define STUB_EXPORT __attribute__((visibility("default")))
#endif

#define STUB_EXTENSION "XR_STUB_loader_threads"

static void stub_dwell(void)
{
   volatile unsigned spin = 0;
   while (spin < 20000)
      spin++;
}

static XRAPI_ATTR XrResult XRAPI_CALL stub_EnumerateInstanceExtensionProperties(
      const char *layer, uint32_t capacity, uint32_t *count,
      XrExtensionProperties *props)
{
   (void)layer;
   stub_dwell();
   *count = 1;
   if (!capacity)
      return XR_SUCCESS;
   if (!props)
      return XR_ERROR_VALIDATION_FAILURE;
   strcpy(props[0].extensionName, STUB_EXTENSION);
   props[0].extensionVersion = 1;
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL stub_CreateInstance(
      const XrInstanceCreateInfo *info, XrInstance *instance)
{
   static uintptr_t next = 0x1000;
   (void)info;
   next += 0x10;
   *instance = (XrInstance)next;
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL stub_DestroyInstance(XrInstance instance)
{
   (void)instance;
   return XR_SUCCESS;
}

static XRAPI_ATTR XrResult XRAPI_CALL stub_GetInstanceProcAddr(
      XrInstance instance, const char *name, PFN_xrVoidFunction *fn)
{
   (void)instance;
   if (!strcmp(name, "xrEnumerateInstanceExtensionProperties"))
      *fn = (PFN_xrVoidFunction)stub_EnumerateInstanceExtensionProperties;
   else if (!strcmp(name, "xrCreateInstance"))
      *fn = (PFN_xrVoidFunction)stub_CreateInstance;
   else if (!strcmp(name, "xrDestroyInstance"))
      *fn = (PFN_xrVoidFunction)stub_DestroyInstance;
   else if (!strcmp(name, "xrGetInstanceProcAddr"))
      *fn = (PFN_xrVoidFunction)stub_GetInstanceProcAddr;
   else
   {
      *fn = NULL;
      return XR_ERROR_FUNCTION_UNSUPPORTED;
   }
   return XR_SUCCESS;
}

STUB_EXPORT XRAPI_ATTR XrResult XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
      const XrNegotiateLoaderInfo *li, XrNegotiateRuntimeRequest *req)
{
   if (     !li || !req
         || li->structType  != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO
         || req->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST
         || li->minInterfaceVersion > XR_CURRENT_LOADER_RUNTIME_VERSION
         || li->maxInterfaceVersion < XR_CURRENT_LOADER_RUNTIME_VERSION)
      return XR_ERROR_INITIALIZATION_FAILED;
   req->runtimeInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
   req->runtimeApiVersion       = XR_MAKE_VERSION(1, 0, 0);
   req->getInstanceProcAddr     = stub_GetInstanceProcAddr;
   return XR_SUCCESS;
}
