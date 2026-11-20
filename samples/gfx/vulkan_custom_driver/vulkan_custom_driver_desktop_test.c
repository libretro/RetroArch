/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */

/* The desktop custom Vulkan driver glue, vulkan_custom_driver_desktop.c
 * and vulkan_custom_driver_glue.c, with real library loads: ICD .json
 * manifests in a scratch directory name stand-in drivers (fake_icd.c),
 * and the device check is handed the address of a function in each.
 * Covers the manifest checks against the files, the crash marker, which
 * module a device runs on (the custom driver, a system driver, a layer
 * in front), the failover when the device runs on the system driver, the
 * failed driver's library staying known while something keeps it
 * mapped, the loader probe and safe start. The frontend is stubbed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <direct.h>
#define FAKE_GETCWD _getcwd
#define FAKE_LIB(name) name ".dll"
#else
#include <unistd.h>
#define FAKE_GETCWD getcwd
#define FAKE_LIB(name) "lib" name ".so"
#endif

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <vulkan/vulkan_symbol_wrapper.h>

#include "../../../configuration.h"
#include "../../../msg_hash.h"
#include "../../../paths.h"
#include "../../../runloop.h"
#include "../../../gfx/common/vulkan_custom_driver.h"

static int failures;
static int checks;

#define CHECK(x) do { \
   checks++; \
   if (!(x)) \
   { \
      failures++; \
      fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #x); \
   } \
} while (0)

/* ---- The frontend, stubbed ---- */

static settings_t settings_st;
static runloop_state_t runloop_st;
static char config_path[PATH_MAX_LENGTH];
static int msgs[16];
static unsigned msg_count;
static char text_buf[64];

settings_t *config_get_ptr(void) { return &settings_st; }
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }

const char *path_get(enum rarch_path_type type)
{
   return (type == RARCH_PATH_CONFIG) ? config_path : "";
}

/* The enum's number, so a pushed notice says which message it was */
const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   snprintf(text_buf, sizeof(text_buf), "%d", (int)msg);
   return text_buf;
}

void runloop_msg_queue_push(const char *msg, size_t len, unsigned prio,
      unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)len; (void)prio; (void)duration; (void)flush; (void)title;
   (void)icon; (void)category;
   if (msg_count < sizeof(msgs) / sizeof(msgs[0]))
      msgs[msg_count++] = atoi(msg);
}

static void log_line(const char *fmt, va_list ap)
{
   if (getenv("VCD_VERBOSE"))
      vfprintf(stderr, fmt, ap);
}
void RARCH_LOG(const char *fmt, ...) { va_list a; va_start(a, fmt); log_line(fmt, a); va_end(a); }
void RARCH_WARN(const char *fmt, ...) { va_list a; va_start(a, fmt); log_line(fmt, a); va_end(a); }
void RARCH_ERR(const char *fmt, ...) { va_list a; va_start(a, fmt); log_line(fmt, a); va_end(a); }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

/* ---- The system loader's instance extensions ---- */

static const char *loader_ext;
static unsigned enumerations;

static VKAPI_ATTR VkResult VKAPI_CALL fake_enumerate(const char *layer,
      uint32_t *count, VkExtensionProperties *props)
{
   (void)layer;
   enumerations++;
   if (!props)
   {
      *count = 2;
      return VK_SUCCESS;
   }
   if (*count < 2)
      return VK_INCOMPLETE;
   memset(props, 0, 2 * sizeof(*props));
   strlcpy(props[0].extensionName, "VK_KHR_surface",
         sizeof(props[0].extensionName));
   strlcpy(props[1].extensionName, loader_ext,
         sizeof(props[1].extensionName));
   *count = 2;
   return VK_SUCCESS;
}

PFN_vkEnumerateInstanceExtensionProperties
   vulkan_symbol_wrapper_vkEnumerateInstanceExtensionProperties;

/* ---- Scratch files ---- */

static char dir[PATH_MAX_LENGTH];
static char cwd[PATH_MAX_LENGTH];

static void scratch_path(char *s, size_t len, const char *name)
{
   fill_pathname_join_special(s, dir, name, len);
}

/* A manifest naming library_path, relative to its own directory */
static void write_icd(const char *name, const char *library_path)
{
   char path[PATH_MAX_LENGTH];
   char buf[512];
   int n = snprintf(buf, sizeof(buf),
         "{ \"file_format_version\": \"1.0.0\", \"ICD\": "
         "{ \"library_path\": \"%s\", \"api_version\": \"1.3.0\" } }",
         library_path);
   scratch_path(path, sizeof(path), name);
   CHECK(filestream_write_file(path, buf, n));
}

/* A copy of a built library in the scratch directory */
static void copy_lib(const char *from, const char *to)
{
   char path[PATH_MAX_LENGTH];
   void *buf   = NULL;
   int64_t len = 0;
   CHECK(filestream_read_file(from, &buf, &len));
   scratch_path(path, sizeof(path), to);
   CHECK(buf && filestream_write_file(path, buf, len));
   free(buf);
}

static void set_driver(const char *name)
{
   if (name)
      scratch_path(settings_st.paths.path_vulkan_driver,
            sizeof(settings_st.paths.path_vulkan_driver), name);
   else
      settings_st.paths.path_vulkan_driver[0] = '\0';
}

static bool marker_exists(void)
{
   char path[PATH_MAX_LENGTH];
   fill_pathname_join_special(path, dir, "vulkan_drivers/pending",
         sizeof(path));
   return path_is_valid(path);
}

static bool msg_shown(int msg)
{
   unsigned i;
   for (i = 0; i < msg_count; i++)
      if (msgs[i] == msg)
         return true;
   return false;
}

/* A function in the library at name, opened with a reference of the
 * test's own, as a library the system loader or a layer holds */
static function_t submit_in(const char *name, dylib_t *lib)
{
   char path[PATH_MAX_LENGTH];
   scratch_path(path, sizeof(path), name);
   *lib = dylib_load(path);
   CHECK(*lib != NULL);
   return *lib ? dylib_proc(*lib, "fake_queue_submit") : NULL;
}

static void remove_scratch(void)
{
   static const char *const names[] = {
      "vulkan_drivers/pending", "vulkan_drivers/state",
      "custom.json", "custom_b.json", "missing.json", "layer.json",
      "bare.json", "safe.json", FAKE_LIB("custom_icd"),
      FAKE_LIB("custom_icd_b"), FAKE_LIB("system_icd"),
      FAKE_LIB("layer_icd")
   };
   char path[PATH_MAX_LENGTH];
   unsigned i;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      scratch_path(path, sizeof(path), names[i]);
      filestream_delete(path);
   }
   scratch_path(path, sizeof(path), "vulkan_drivers");
   path_rmdir(path);
   path_rmdir(dir);
}

int main(void)
{
   function_t custom_submit, custom_b_submit, system_submit, layer_submit;
   dylib_t custom_lib, custom_b_lib, system_lib, layer_lib;
   dylib_t handle;
   char why[VULKAN_CUSTOM_DRIVER_WHY_MAX];

   CHECK(FAKE_GETCWD(cwd, sizeof(cwd)) != NULL);
   fill_pathname_join_special(dir, cwd, "obj/desktop_scratch_test", sizeof(dir));
   remove_scratch();
   CHECK(path_mkdir(dir));
   scratch_path(config_path, sizeof(config_path), "retroarch.cfg");
   runloop_st.current_core_type = CORE_TYPE_DUMMY;
   loader_ext = "VK_LUNARG_direct_driver_loading";
   vulkan_symbol_wrapper_vkEnumerateInstanceExtensionProperties =
      fake_enumerate;

   copy_lib(FAKE_LIB("fake_icd"),       FAKE_LIB("custom_icd"));
   copy_lib(FAKE_LIB("fake_icd"),       FAKE_LIB("custom_icd_b"));
   copy_lib(FAKE_LIB("fake_icd"),       FAKE_LIB("system_icd"));
   copy_lib(FAKE_LIB("fake_icd_layer"), FAKE_LIB("layer_icd"));
   write_icd("custom.json",   "./" FAKE_LIB("custom_icd"));
   write_icd("custom_b.json", "./" FAKE_LIB("custom_icd_b"));
   write_icd("missing.json",  "./" FAKE_LIB("nothing_here"));
   write_icd("layer.json",    "./" FAKE_LIB("layer_icd"));
   write_icd("bare.json",     FAKE_LIB("system_icd"));
   write_icd("safe.json",     "./" FAKE_LIB("custom_icd"));

   /* No driver set: nothing next to the config, the system driver */
   set_driver(NULL);
   CHECK(!vulkan_custom_driver_open());
   CHECK(!vulkan_custom_driver_current());
   CHECK(enumerations == 0);

   /* Manifests that cannot load: off until picked again */
   set_driver("missing.json");
   CHECK(!vulkan_custom_driver_open());
   CHECK(msg_shown(MSG_VULKAN_CUSTOM_DRIVER_INVALID_ICD));
   CHECK(!marker_exists());
   set_driver("bare.json");
   CHECK(!vulkan_custom_driver_open());
   set_driver("layer.json");
   CHECK(!vulkan_custom_driver_open());
   CHECK(!marker_exists());

   /* The loader is asked once a process */
   CHECK(enumerations == 2);
   loader_ext = "VK_KHR_portability_enumeration";
   CHECK(vulkan_custom_driver_supported());
   CHECK(enumerations == 2);

   /* The custom driver opens; the marker stands until it is up */
   set_driver("custom.json");
   msg_count = 0;
   CHECK((handle = vulkan_custom_driver_open()) != NULL);
   CHECK(handle && dylib_proc(handle, "vk_icdGetInstanceProcAddr"));
   CHECK(vulkan_custom_driver_current());
   CHECK(marker_exists());
   CHECK(msg_count == 0);

   custom_submit   = submit_in(FAKE_LIB("custom_icd"),   &custom_lib);
   custom_b_submit = submit_in(FAKE_LIB("custom_icd_b"), &custom_b_lib);
   system_submit   = submit_in(FAKE_LIB("system_icd"),   &system_lib);
   layer_submit    = submit_in(FAKE_LIB("layer_icd"),    &layer_lib);
   CHECK(custom_submit && custom_b_submit && system_submit && layer_submit);

   /* Its device runs on it: confirmed, and up */
   CHECK(vulkan_custom_driver_device(custom_submit));
   vulkan_custom_driver_started();
   CHECK(!vulkan_custom_driver_current());
   CHECK(!marker_exists());

   /* A later init: a layer in front hides the driver, not held against it */
   CHECK(vulkan_custom_driver_open() == handle);
   CHECK(vulkan_custom_driver_current());
   CHECK(vulkan_custom_driver_device(layer_submit));
   vulkan_custom_driver_started();

   /* Another custom driver whose device lands on a system driver: the
    * init fails over and the driver is off for the session */
   set_driver("custom_b.json");
   CHECK(vulkan_custom_driver_open() != NULL);
   CHECK(vulkan_custom_driver_current());
   CHECK(!vulkan_custom_driver_device(system_submit));
   msg_count = 0;
   vulkan_custom_driver_failed();
   CHECK(!vulkan_custom_driver_current());
   CHECK(msg_shown(MSG_VULKAN_CUSTOM_DRIVER_FAILED));
   CHECK(!marker_exists());
   CHECK(!vulkan_custom_driver_open());

   /* The system init that follows runs on the failed driver's library,
    * still mapped by the test: restart, said once */
   msg_count = 0;
   CHECK(vulkan_custom_driver_device(custom_b_submit));
   CHECK(msg_shown(MSG_VULKAN_CUSTOM_DRIVER_RESTART));
   msg_count = 0;
   CHECK(vulkan_custom_driver_device(custom_b_submit));
   CHECK(vulkan_custom_driver_device(system_submit));
   CHECK(vulkan_custom_driver_device(layer_submit));
   CHECK(msg_count == 0);

   /* Off Android the reason is in the log, not the sublabel */
   CHECK(!vulkan_custom_driver_why(settings_st.paths.path_vulkan_driver,
            why, sizeof(why)));

   /* Safe start: the system driver, nothing opened */
   vulkan_custom_driver_set_safe_mode(true);
   CHECK(vulkan_custom_driver_safe_mode());
   set_driver("safe.json");
   msg_count = 0;
   CHECK(!vulkan_custom_driver_open());
   CHECK(msg_shown(MSG_VULKAN_CUSTOM_DRIVER_SAFE_MODE));
   CHECK(!marker_exists());

   dylib_close(custom_lib);
   dylib_close(custom_b_lib);
   dylib_close(system_lib);
   dylib_close(layer_lib);
   remove_scratch();

   printf("vulkan_custom_driver_desktop_test: %d checks, %d failures\n",
         checks, failures);
   return failures ? 1 : 0;
}
