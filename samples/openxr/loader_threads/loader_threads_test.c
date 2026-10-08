/* The vendored OpenXR loader under concurrent use, against a stand-in
 * runtime. The loader shares a runtime, an active instance, a logger
 * recorder table, a messenger table and a property set between threads
 * without a lock; run under -fsanitize=thread or address, a race or a
 * use-after-free in any of them fails the run.
 *
 *   enumerators  xrEnumerateInstanceExtensionProperties in a loop: each
 *                call loads or finds the runtime and dwells inside it,
 *                and every call logs through the recorder table.
 *   rival        xrCreateInstance in a loop: refused while the instance
 *                is claimed, otherwise a full create and destroy, which
 *                unloads the runtime under the enumerators.
 *   reinit       xrInitializeLoaderKHR with a property set naming the
 *                runtime manifest: replaces the properties the
 *                enumerators read, and unloads the runtime when it can
 *                take the instance claim.
 *   main         create, a debug messenger made and destroyed (adding
 *                and removing a recorder and a messenger entry), a
 *                message submitted through it, destroy. */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openxr/openxr.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#define ENUMERATORS 3
#define ROUNDS      150

static retro_atomic_int_t stop_threads;
static retro_atomic_int_t failures;
static retro_atomic_int_t enumerations;
static retro_atomic_int_t rival_creates;
static retro_atomic_int_t reinits;
static retro_atomic_int_t messages;
static char manifest_path[1024];

static void fail(const char *fmt, ...)
{
   va_list ap;
   fprintf(stderr, "FAIL loader_threads_test: ");
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   fprintf(stderr, "\n");
   retro_atomic_fetch_add_int(&failures, 1);
}

static bool running(void)
{
   return !retro_atomic_load_acquire_int(&stop_threads);
}

static void fill_create_info(XrInstanceCreateInfo *info,
      const char *const *exts)
{
   memset(info, 0, sizeof(*info));
   info->type                            = XR_TYPE_INSTANCE_CREATE_INFO;
   strcpy(info->applicationInfo.applicationName, "loader_threads_test");
   info->applicationInfo.apiVersion      = XR_MAKE_VERSION(1, 0, 0);
   info->enabledExtensionCount           = 1;
   info->enabledExtensionNames           = exts;
}

static void enumerator(void *data)
{
   XrExtensionProperties props[16];
   (void)data;
   while (running())
   {
      uint32_t i;
      uint32_t count     = 0;
      bool has_stub      = false;
      bool has_debug     = false;
      XrResult res       = xrEnumerateInstanceExtensionProperties(
            NULL, 0, &count, NULL);
      if (res != XR_SUCCESS || count < 2 || count > 16)
      {
         fail("enumerate count: result %d, %u extensions", (int)res,
               (unsigned)count);
         return;
      }
      for (i = 0; i < count; i++)
      {
         memset(&props[i], 0, sizeof(props[i]));
         props[i].type = XR_TYPE_EXTENSION_PROPERTIES;
      }
      if ((res = xrEnumerateInstanceExtensionProperties(
                  NULL, count, &count, props)) != XR_SUCCESS)
      {
         fail("enumerate: result %d", (int)res);
         return;
      }
      for (i = 0; i < count; i++)
      {
         if (!strcmp(props[i].extensionName, "XR_STUB_loader_threads"))
            has_stub  = true;
         if (!strcmp(props[i].extensionName, XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
            has_debug = true;
      }
      if (!has_stub || !has_debug)
      {
         fail("enumerate: the runtime's or the loader's extension is missing");
         return;
      }
      retro_atomic_fetch_add_int(&enumerations, 1);
   }
}

static void rival(void *data)
{
   const char *exts[1];
   XrInstanceCreateInfo info;
   (void)data;
   exts[0] = XR_EXT_DEBUG_UTILS_EXTENSION_NAME;
   fill_create_info(&info, exts);
   while (running())
   {
      XrInstance instance = XR_NULL_HANDLE;
      XrResult res        = xrCreateInstance(&info, &instance);
      if (res == XR_SUCCESS)
      {
         if (xrDestroyInstance(instance) != XR_SUCCESS)
            fail("rival: destroy failed");
         retro_atomic_fetch_add_int(&rival_creates, 1);
      }
      else if (res != XR_ERROR_LIMIT_REACHED)
      {
         fail("rival: create returned %d", (int)res);
         return;
      }
   }
}

static void reinit(void *data)
{
   PFN_xrInitializeLoaderKHR init = NULL;
   XrLoaderInitPropertyValueEXT values[2];
   XrLoaderInitInfoPropertiesEXT props;
   (void)data;
   if (     xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
               (PFN_xrVoidFunction*)&init) != XR_SUCCESS
         || !init)
   {
      fail("reinit: no xrInitializeLoaderKHR");
      return;
   }
   values[0].name  = "XR_RUNTIME_JSON";
   values[0].value = manifest_path;
   values[1].name  = "XR_LOADER_THREADS_ROUND";
   memset(&props, 0, sizeof(props));
   props.type               = XR_TYPE_LOADER_INIT_INFO_PROPERTIES_EXT;
   props.propertyValueCount = 2;
   props.propertyValues     = values;
   while (running())
   {
      XrResult res;
      values[1].value = (retro_atomic_load_acquire_int(&reinits) & 1)
            ? "odd" : "even";
      res = init((const XrLoaderInitInfoBaseHeaderKHR*)&props);
      if (res == XR_SUCCESS)
         retro_atomic_fetch_add_int(&reinits, 1);
      else if (res != XR_ERROR_INITIALIZATION_FAILED)
      {
         fail("reinit: result %d", (int)res);
         return;
      }
   }
}

static XrBool32 XRAPI_CALL on_message(
      XrDebugUtilsMessageSeverityFlagsEXT severity,
      XrDebugUtilsMessageTypeFlagsEXT types,
      const XrDebugUtilsMessengerCallbackDataEXT *data, void *user)
{
   (void)severity;
   (void)types;
   (void)user;
   if (data && data->message && !strcmp(data->message, "loader_threads"))
      retro_atomic_fetch_add_int(&messages, 1);
   return XR_FALSE;
}

static void one_round(const XrInstanceCreateInfo *info)
{
   XrInstance instance                                = XR_NULL_HANDLE;
   XrDebugUtilsMessengerEXT messenger                 = XR_NULL_HANDLE;
   PFN_xrCreateDebugUtilsMessengerEXT create_messenger   = NULL;
   PFN_xrDestroyDebugUtilsMessengerEXT destroy_messenger = NULL;
   PFN_xrSubmitDebugUtilsMessageEXT submit            = NULL;
   XrDebugUtilsMessengerCreateInfoEXT mci;
   XrDebugUtilsMessengerCallbackDataEXT cbd;
   XrResult res;

   while ((res = xrCreateInstance(info, &instance)) == XR_ERROR_LIMIT_REACHED)
      ;
   if (res != XR_SUCCESS)
   {
      fail("create returned %d", (int)res);
      return;
   }

   xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
         (PFN_xrVoidFunction*)&create_messenger);
   xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT",
         (PFN_xrVoidFunction*)&destroy_messenger);
   xrGetInstanceProcAddr(instance, "xrSubmitDebugUtilsMessageEXT",
         (PFN_xrVoidFunction*)&submit);
   if (!create_messenger || !destroy_messenger || !submit)
      fail("debug utils entry points missing");
   else
   {
      memset(&mci, 0, sizeof(mci));
      mci.type              = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
      mci.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
      mci.messageTypes      = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
      mci.userCallback      = on_message;
      if ((res = create_messenger(instance, &mci, &messenger)) != XR_SUCCESS)
         fail("messenger create returned %d", (int)res);
      else
      {
         memset(&cbd, 0, sizeof(cbd));
         cbd.type         = XR_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
         cbd.messageId    = "loader_threads";
         cbd.functionName = "one_round";
         cbd.message      = "loader_threads";
         submit(instance, XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
               XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &cbd);
         if ((res = destroy_messenger(messenger)) != XR_SUCCESS)
            fail("messenger destroy returned %d", (int)res);
      }
   }

   if ((res = xrDestroyInstance(instance)) != XR_SUCCESS)
      fail("destroy returned %d", (int)res);
}

static bool write_manifest(const char *dir)
{
   FILE *f;
   size_t len = strlen(dir);
   if (len + 64 > sizeof(manifest_path))
      return false;
   sprintf(manifest_path, "%s/stub_runtime.json", dir);
   if (!(f = fopen(manifest_path, "w")))
      return false;
   fprintf(f,
         "{\n"
         "   \"file_format_version\": \"1.0.0\",\n"
         "   \"runtime\": { \"library_path\": \"./%s\" }\n"
         "}\n",
#if defined(_WIN32)
         "stub_runtime.dll"
#else
         "libstub_runtime.so"
#endif
         );
   fclose(f);
   return true;
}

int main(int argc, char *argv[])
{
   unsigned i;
   const char *exts[1];
   XrInstanceCreateInfo info;
   sthread_t *threads[ENUMERATORS + 2];
   char var[1100];

   if (!write_manifest(argc > 1 ? argv[1] : "."))
   {
      fprintf(stderr, "cannot write the runtime manifest\n");
      return 1;
   }
   sprintf(var, "XR_RUNTIME_JSON=%s", manifest_path);
   putenv(var);

   for (i = 0; i < ENUMERATORS; i++)
      threads[i] = sthread_create(enumerator, NULL);
   threads[ENUMERATORS]     = sthread_create(rival, NULL);
   threads[ENUMERATORS + 1] = sthread_create(reinit, NULL);

   exts[0] = XR_EXT_DEBUG_UTILS_EXTENSION_NAME;
   fill_create_info(&info, exts);
   for (i = 0; i < ROUNDS; i++)
      one_round(&info);

   retro_atomic_store_release_int(&stop_threads, 1);
   for (i = 0; i < ENUMERATORS + 2; i++)
      if (threads[i])
         sthread_join(threads[i]);

   if (retro_atomic_load_acquire_int(&messages) != ROUNDS)
      fail("%d of %d submitted messages reached the messenger",
            (int)retro_atomic_load_acquire_int(&messages), ROUNDS);
   if (!retro_atomic_load_acquire_int(&enumerations))
      fail("no enumeration completed");

   if (retro_atomic_load_acquire_int(&failures))
   {
      fprintf(stderr, "%d failure(s)\n",
            (int)retro_atomic_load_acquire_int(&failures));
      return 1;
   }
   printf("PASS loader_threads_test: %d rounds, %d enumerations, "
         "%d rival creates, %d reinits\n", ROUNDS,
         (int)retro_atomic_load_acquire_int(&enumerations),
         (int)retro_atomic_load_acquire_int(&rival_creates),
         (int)retro_atomic_load_acquire_int(&reinits));
   return 0;
}
