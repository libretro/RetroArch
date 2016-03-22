/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2016 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Jason Fetters
 * Copyright (C) 2012-2015 - Michael Lelli
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/resource.h>

#include <pthread.h>

#ifdef ANDROID
#include <sys/system_properties.h>
#ifdef __arm__
#include <machine/cpu-features.h>
#endif
#endif

#include <boolean.h>
#include <retro_dirent.h>
#include <retro_inline.h>
#include <compat/strl.h>
#include <rhash.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../defaults.h"
#include "../../general.h"
#include "../../verbosity.h"
#include "platform_linux.h"

/* This small data type is used to represent a CPU list / mask,
 * as read from sysfs on Linux.
 *
 * See http://www.kernel.org/doc/Documentation/cputopology.txt
 *
 * For now, we don't expect more than 32 cores on mobile devices, 
 * so keep everything simple.
 */
typedef struct
{
    uint32_t mask;
} CpuList;

static bool                cpu_inited_once;
static enum cpu_family     g_cpuFamily;
static  uint64_t           g_cpuFeatures;
static  int                g_cpuCount;

#ifndef HAVE_DYNAMIC
static enum frontend_fork linux_fork_mode = FRONTEND_FORK_NONE;
#endif

#ifdef __arm__
#  define DEFAULT_CPU_FAMILY  CPU_FAMILY_ARM
#elif defined __i386__
#  define DEFAULT_CPU_FAMILY  CPU_FAMILY_X86
#else
#  define DEFAULT_CPU_FAMILY  CPU_FAMILY_UNKNOWN
#endif

#ifdef __i386__
void x86_cpuid(int func, int flags[4]);
#endif

#ifdef __ARM_ARCH__
/* Extract the content of a the first occurrence of a given field in
 * the content of /proc/cpuinfo and return it as a heap-allocated
 * string that must be freed by the caller.
 *
 * Return NULL if not found
 */
static char *extract_cpuinfo_field(char* buffer,
      ssize_t length, const char* field)
{
   int len;
   const char *q;
   int  fieldlen = strlen(field);
   char* bufend  = buffer + length;
   char* result  = NULL;
   /* Look for first field occurrence, 
    * and ensures it starts the line. */
   const char *p = buffer;

   for (;;)
   {
      p = memmem(p, bufend-p, field, fieldlen);
      if (p == NULL)
         return result;

      if (p == buffer || p[-1] == '\n')
         break;

      p += fieldlen;
   }

   /* Skip to the first column followed by a space */
   p += fieldlen;
   p  = memchr(p, ':', bufend-p);
   if (p == NULL || p[1] != ' ')
      return result;

   /* Find the end of the line */
   p += 2;
   q  = memchr(p, '\n', bufend-p);
   if (q == NULL)
      q = bufend;

   /* Copy the line into a heap-allocated buffer */
   len    = q-p;
   result = malloc(len+1);
   if (result == NULL)
      return result;

   memcpy(result, p, len);
   result[len] = '\0';

   return result;
}

/* Checks that a space-separated list of items 
 * contains one given 'item'.
 * Returns 1 if found, 0 otherwise.
 */
static int has_list_item(const char* list, const char* item)
{
    const char*  p = list;
    int    itemlen = strlen(item);

    if (list == NULL)
        return 0;

    while (*p)
    {
        const char*  q;

        /* skip spaces */
        while (*p == ' ' || *p == '\t')
            p++;

        /* find end of current list item */
        q = p;
        while (*q && *q != ' ' && *q != '\t')
            q++;

        if (itemlen == q-p && !memcmp(p, item, itemlen))
            return 1;

        /* skip to next item */
        p = q;
    }
    return 0;
}
#endif


/* Parse an decimal integer starting from 'input', but not going further
 * than 'limit'. Return the value into '*result'.
 *
 * NOTE: Does not skip over leading spaces, or deal with sign characters.
 * NOTE: Ignores overflows.
 *
 * The function returns NULL in case of error (bad format), or the new
 * position after the decimal number in case of success (which will always
 * be <= 'limit').
 */
static const char *parse_decimal(const char* input,
      const char* limit, int* result)
{
    const char* p = input;
    int       val = 0;

    while (p < limit)
    {
        int d = (*p - '0');
        if ((unsigned)d >= 10U)
            break;
        val = val*10 + d;
        p++;
    }
    if (p == input)
        return NULL;

    *result = val;
    return p;
}


/* Parse a textual list of cpus and store the result inside a CpuList object.
 * Input format is the following:
 * - comma-separated list of items (no spaces)
 * - each item is either a single decimal number (cpu index), or a range made
 *   of two numbers separated by a single dash (-). Ranges are inclusive.
 *
 * Examples:   0
 *             2,4-127,128-143
 *             0-1
 */
static void cpulist_parse(CpuList* list, char **buf, ssize_t length)
{
   const char* p   = (const char*)buf;
   const char* end = p + length;

   /* NOTE: the input line coming from sysfs typically contains a
    * trailing newline, so take care of it in the code below
    */
   while (p < end && *p != '\n')
   {
      int val, start_value, end_value;
      /* Find the end of current item, and put it into 'q' */
      const char *q = (const char*)memchr(p, ',', end-p);

      if (!q)
         q = end;

      /* Get first value */
      p = parse_decimal(p, q, &start_value);
      if (p == NULL)
         return;

      end_value = start_value;

      /* If we're not at the end of the item, expect a dash and
       * and integer; extract end value.
       */
      if (p < q && *p == '-')
      {
         p = parse_decimal(p+1, q, &end_value);
         if (p == NULL)
            return;
      }

      /* Set bits CPU list bits */
      for (val = start_value; val <= end_value; val++)
      {
         if ((unsigned)val < 32)
            list->mask |= (uint32_t)(1U << val);
      }

      /* Jump to next item */
      p = q;
      if (p < end)
         p++;
   }
}

/* Read a CPU list from one sysfs file */
static void cpulist_read_from(CpuList* list, const char* filename)
{
   ssize_t length;
   char *buf  = NULL;

   list->mask = 0;

   if (retro_read_file(filename, (void**)&buf, &length) != 1)
   {
      RARCH_ERR("Could not read %s: %s\n", filename, strerror(errno));
      return;
   }

   cpulist_parse(list, &buf, length);
   if (buf)
      free(buf);
   buf = NULL;
}

/* Return the number of cpus present on a given device.
 *
 * To handle all weird kernel configurations, we need to compute the
 * intersection of the 'present' and 'possible' CPU lists and count
 * the result.
 */
static int get_cpu_count(void)
{
   CpuList  cpus_present[1];
   CpuList cpus_possible[1];

   cpulist_read_from(cpus_present, "/sys/devices/system/cpu/present");
   cpulist_read_from(cpus_possible, "/sys/devices/system/cpu/possible");

   /* Compute the intersection of both sets to get the actual number of
    * CPU cores that can be used on this device by the kernel.
    */
   cpus_present->mask &= cpus_possible->mask;

   return __builtin_popcount(cpus_present->mask);
}

static void linux_cpu_init(void)
{
   ssize_t  length;
   void *buf = NULL;

   g_cpuFamily   = DEFAULT_CPU_FAMILY;
   g_cpuFeatures = 0;
   g_cpuCount    = 1;

   if (retro_read_file("/proc/cpuinfo", &buf, &length) != 1)
      return;

   /* Count the CPU cores, the value may be 0 for single-core CPUs */
   g_cpuCount = get_cpu_count();
   if (g_cpuCount == 0)
      g_cpuCount = 1;

   RARCH_LOG("found cpuCount = %d\n", g_cpuCount);

#ifdef __ARM_ARCH__
   /* Extract architecture from the "CPU Architecture" field.
    * The list is well-known, unlike the the output of
    * the 'Processor' field which can vary greatly.
    *
    * See the definition of the 'proc_arch' array in
    * $KERNEL/arch/arm/kernel/setup.c and the 'c_show' function in
    * same file.
    */
   char* cpu_arch = extract_cpuinfo_field(buf, length, "CPU architecture");

   if (cpu_arch)
   {
      char*  end;
      int    has_armv7 = 0;
      /* read the initial decimal number, ignore the rest */
      long   arch_number = strtol(cpu_arch, &end, 10);

      RARCH_LOG("Found CPU architecture = '%s'\n", cpu_arch);

      /* Here we assume that ARMv8 will be upwards compatible with v7
       * in the future. Unfortunately, there is no 'Features' field to
       * indicate that Thumb-2 is supported.
       */
      if (end > cpu_arch && arch_number >= 7)
         has_armv7 = 1;

      /* Unfortunately, it seems that certain ARMv6-based CPUs
       * report an incorrect architecture number of 7!
       *
       * See http://code.google.com/p/android/issues/detail?id=10812
       *
       * We try to correct this by looking at the 'elf_format'
       * field reported by the 'Processor' field, which is of the
       * form of "(v7l)" for an ARMv7-based CPU, and "(v6l)" for
       * an ARMv6-one.
       */
      if (has_armv7)
      {
         char *cpu_proc = extract_cpuinfo_field(buf, length,
               "Processor");

         if (cpu_proc != NULL)
         {
            RARCH_LOG("found cpu_proc = '%s'\n", cpu_proc);
            if (has_list_item(cpu_proc, "(v6l)"))
            {
               RARCH_ERR("CPU processor and architecture mismatch!!\n");
               has_armv7 = 0;
            }
            free(cpu_proc);
         }
      }

      if (has_armv7)
         g_cpuFeatures |= CPU_ARM_FEATURE_ARMv7;

      /* The LDREX / STREX instructions are available from ARMv6 */
      if (arch_number >= 6)
         g_cpuFeatures |= CPU_ARM_FEATURE_LDREX_STREX;

      free(cpu_arch);
   }

   /* Extract the list of CPU features from 'Features' field */
   char* cpu_features = extract_cpuinfo_field(buf, length, "Features");

   if (cpu_features)
   {
      RARCH_LOG("found cpu_features = '%s'\n", cpu_features);

      if (has_list_item(cpu_features, "vfpv3"))
         g_cpuFeatures |= CPU_ARM_FEATURE_VFPv3;

      else if (has_list_item(cpu_features, "vfpv3d16"))
         g_cpuFeatures |= CPU_ARM_FEATURE_VFPv3;

      /* Note: Certain kernels only report NEON but not VFPv3
       * in their features list. However, ARM mandates
       * that if NEON is implemented, so must be VFPv3
       * so always set the flag.
       */
      if (has_list_item(cpu_features, "neon"))
         g_cpuFeatures |= CPU_ARM_FEATURE_NEON | CPU_ARM_FEATURE_VFPv3;
      free(cpu_features);
   }
#endif /* __ARM_ARCH__ */

#ifdef __i386__
   g_cpuFamily = CPU_FAMILY_X86;
#elif defined(_MIPS_ARCH)
   g_cpuFamily = CPU_FAMILY_MIPS;
#endif

   if (buf)
      free(buf);
   buf = NULL;
}

enum cpu_family linux_get_cpu_platform(void)
{
    return g_cpuFamily;
}

uint64_t linux_get_cpu_features(void)
{
    return g_cpuFeatures;
}

int linux_get_cpu_count(void)
{
    return g_cpuCount;
}

int system_property_get(const char *command,
      const char *args, char *value)
{
   FILE *pipe;
   int length                   = 0;
   char buffer[PATH_MAX_LENGTH] = {0};
   char cmd[PATH_MAX_LENGTH]    = {0};
   char *curpos                 = NULL;

   snprintf(cmd, sizeof(cmd), "%s %s", command, args);

   pipe = popen(cmd, "r");

   if (!pipe)
      goto error;

   curpos = value;

   while (!feof(pipe))
   {
      if (fgets(buffer, 128, pipe) != NULL)
      {
         int curlen = strlen(buffer);

         memcpy(curpos, buffer, curlen);

         curpos    += curlen;
         length    += curlen;
      }
   }

   *curpos = '\0';

   pclose(pipe);

   return length;

error:
   RARCH_ERR("Could not create pipe.\n");
   return 0;
}

#ifdef ANDROID
#define SDCARD_ROOT_WRITABLE     1
#define SDCARD_EXT_DIR_WRITABLE  2
#define SDCARD_NOT_WRITABLE      3

struct android_app *g_android;
static pthread_key_t thread_key;

char screenshot_dir[PATH_MAX_LENGTH];
char downloads_dir[PATH_MAX_LENGTH];
char apk_path[PATH_MAX_LENGTH];
char sdcard_dir[PATH_MAX_LENGTH];
char app_dir[PATH_MAX_LENGTH];
char ext_dir[PATH_MAX_LENGTH];


/* forward declaration */
bool android_run_events(void *data);

void android_app_write_cmd(struct android_app *android_app, int8_t cmd)
{
   if (!android_app)
      return;

   if (write(android_app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd))
      RARCH_ERR("Failure writing android_app cmd: %s\n", strerror(errno));
}

static void android_app_set_input(struct android_app *android_app,
      AInputQueue* inputQueue)
{
   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   android_app->pendingInputQueue = inputQueue;
   android_app_write_cmd(android_app, APP_CMD_INPUT_CHANGED);

   while (android_app->inputQueue != android_app->pendingInputQueue)
      scond_wait(android_app->cond, android_app->mutex);

   slock_unlock(android_app->mutex);
}

static void android_app_set_window(struct android_app *android_app,
      ANativeWindow* window)
{
   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   if (android_app->pendingWindow)
      android_app_write_cmd(android_app, APP_CMD_TERM_WINDOW);

   android_app->pendingWindow = window;

   if (window)
      android_app_write_cmd(android_app, APP_CMD_INIT_WINDOW);

   while (android_app->window != android_app->pendingWindow)
      scond_wait(android_app->cond, android_app->mutex);

   slock_unlock(android_app->mutex);
}

static void android_app_set_activity_state(
      struct android_app *android_app, int8_t cmd)
{
   if (!android_app)
      return;

   slock_lock(android_app->mutex);
   android_app_write_cmd(android_app, cmd);
   while (android_app->activityState != cmd)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);
}

static void android_app_free(struct android_app* android_app)
{
   slock_lock(android_app->mutex);

   sthread_join(android_app->thread);
   RARCH_LOG("Joined with RetroArch native thread.\n");

   slock_unlock(android_app->mutex);

   close(android_app->msgread);
   close(android_app->msgwrite);
   scond_free(android_app->cond);
   slock_free(android_app->mutex);

   free(android_app);
}

static void onDestroy(ANativeActivity* activity)
{
   RARCH_LOG("onDestroy: %p\n", activity);
   android_app_free((struct android_app*)activity->instance);
}

static void onStart(ANativeActivity* activity)
{
   RARCH_LOG("Start: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_START);
}

static void onResume(ANativeActivity* activity)
{
   RARCH_LOG("Resume: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_RESUME);
}

static void* onSaveInstanceState(
      ANativeActivity* activity, size_t* outLen)
{
   void* savedState = NULL;
   struct android_app* android_app = (struct android_app*)
      activity->instance;

   RARCH_LOG("SaveInstanceState: %p\n", activity);

   slock_lock(android_app->mutex);

   android_app->stateSaved = 0;
   android_app_write_cmd(android_app, APP_CMD_SAVE_STATE);

   while (!android_app->stateSaved)
      scond_wait(android_app->cond, android_app->mutex);

   if (android_app->savedState != NULL)
   {
      savedState = android_app->savedState;
      *outLen    = android_app->savedStateSize;
      android_app->savedState = NULL;
      android_app->savedStateSize = 0;
   }

   slock_unlock(android_app->mutex);

   return savedState;
}

static void onPause(ANativeActivity* activity)
{
   RARCH_LOG("Pause: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_PAUSE);
}

static void onStop(ANativeActivity* activity)
{
   RARCH_LOG("Stop: %p\n", activity);
   android_app_set_activity_state((struct android_app*)
         activity->instance, APP_CMD_STOP);
}

static void onConfigurationChanged(ANativeActivity *activity)
{
   RARCH_LOG("ConfigurationChanged: %p\n", activity);
   android_app_write_cmd((struct android_app*)
         activity->instance, APP_CMD_CONFIG_CHANGED);
}

static void onLowMemory(ANativeActivity* activity)
{
   RARCH_LOG("LowMemory: %p\n", activity);
   android_app_write_cmd((struct android_app*)
         activity->instance, APP_CMD_LOW_MEMORY);
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
   RARCH_LOG("WindowFocusChanged: %p -- %d\n", activity, focused);
   android_app_write_cmd((struct android_app*)activity->instance,
         focused ? APP_CMD_GAINED_FOCUS : APP_CMD_LOST_FOCUS);
}

static void onNativeWindowCreated(ANativeActivity* activity,
      ANativeWindow* window)
{
   RARCH_LOG("NativeWindowCreated: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, window);
}

static void onNativeWindowDestroyed(ANativeActivity* activity,
      ANativeWindow* window)
{
   RARCH_LOG("NativeWindowDestroyed: %p -- %p\n", activity, window);
   android_app_set_window((struct android_app*)activity->instance, NULL);
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
   RARCH_LOG("InputQueueCreated: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, queue);
}

static void onInputQueueDestroyed(ANativeActivity* activity,
      AInputQueue* queue)
{
   RARCH_LOG("InputQueueDestroyed: %p -- %p\n", activity, queue);
   android_app_set_input((struct android_app*)activity->instance, NULL);
}

JNIEnv *jni_thread_getenv(void)
{
   JNIEnv *env;
   struct android_app* android_app = (struct android_app*)g_android;
   int status = (*android_app->activity->vm)->
      AttachCurrentThread(android_app->activity->vm, &env, 0);

   if (status < 0)
   {
      RARCH_ERR("jni_thread_getenv: Failed to attach current thread.\n");
      return NULL;
   }
   pthread_setspecific(thread_key, (void*)env);

   return env;
}

static void jni_thread_destruct(void *value)
{
   JNIEnv *env = (JNIEnv*)value;
   struct android_app *android_app = (struct android_app*)g_android;
   RARCH_LOG("jni_thread_destruct()\n");

   if (!env)
      return;

   if (android_app)
      (*android_app->activity->vm)->
         DetachCurrentThread(android_app->activity->vm);
   pthread_setspecific(thread_key, NULL);
}

static void android_app_entry(void *data)
{
   char *argv[1];
   int argc = 0;
   int ret  = 0;

   if (rarch_main(argc, argv, data) != 0)
      goto end;
#ifndef HAVE_MAIN
   do
   {
      unsigned sleep_ms = 0;
      ret = runloop_iterate(&sleep_ms);

      if (ret == 1 && sleep_ms > 0)
         retro_sleep(sleep_ms);
      runloop_ctl(RUNLOOP_CTL_DATA_ITERATE, NULL);
   }while (ret != -1);

   main_exit(data);
#endif

end:
   exit(0);
}

static struct android_app* android_app_create(ANativeActivity* activity,
        void* savedState, size_t savedStateSize)
{
   int msgpipe[2];
   struct android_app *android_app = 
      (struct android_app*)calloc(1, sizeof(*android_app));

   if (!android_app)
   {
      RARCH_ERR("Failed to initialize android_app\n");
      return NULL;
   }
   android_app->activity = activity;

   android_app->mutex    = slock_new();
   android_app->cond     = scond_new();

   if (savedState != NULL)
   {
      android_app->savedState = malloc(savedStateSize);
      android_app->savedStateSize = savedStateSize;
      memcpy(android_app->savedState, savedState, savedStateSize);
   }

   if (pipe(msgpipe))
   {
      RARCH_ERR("could not create pipe: %s.\n", strerror(errno));
      if(android_app->savedState)
        free(android_app->savedState);
      free(android_app);
      return NULL;
   }

   android_app->msgread  = msgpipe[0];
   android_app->msgwrite = msgpipe[1];

   android_app->thread   = sthread_create(android_app_entry, android_app);

   /* Wait for thread to start. */
   slock_lock(android_app->mutex);
   while (!android_app->running)
      scond_wait(android_app->cond, android_app->mutex);
   slock_unlock(android_app->mutex);

   return android_app;
}

/*
 * Native activity interaction (called from main thread)
 **/

void ANativeActivity_onCreate(ANativeActivity* activity,
      void* savedState, size_t savedStateSize)
{
   RARCH_LOG("Creating Native Activity: %p\n", activity);
   activity->callbacks->onDestroy               = onDestroy;
   activity->callbacks->onStart                 = onStart;
   activity->callbacks->onResume                = onResume;
   activity->callbacks->onSaveInstanceState     = onSaveInstanceState;
   activity->callbacks->onPause                 = onPause;
   activity->callbacks->onStop                  = onStop;
   activity->callbacks->onConfigurationChanged  = onConfigurationChanged;
   activity->callbacks->onLowMemory             = onLowMemory;
   activity->callbacks->onWindowFocusChanged    = onWindowFocusChanged;
   activity->callbacks->onNativeWindowCreated   = onNativeWindowCreated;
   activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
   activity->callbacks->onInputQueueCreated     = onInputQueueCreated;
   activity->callbacks->onInputQueueDestroyed   = onInputQueueDestroyed;

   /* These are set only for the native activity,
    * and are reset when it ends. */
   ANativeActivity_setWindowFlags(activity, AWINDOW_FLAG_KEEP_SCREEN_ON
         | AWINDOW_FLAG_FULLSCREEN, 0);

   if (pthread_key_create(&thread_key, jni_thread_destruct))
      RARCH_ERR("Error initializing pthread_key\n");

   activity->instance = android_app_create(activity,
         savedState, savedStateSize);
}

static void frontend_android_get_name(char *s, size_t len)
{
   system_property_get("getprop", "ro.product.model", s);
}

static void frontend_android_get_version(int32_t *major,
      int32_t *minor, int32_t *rel)
{
   char os_version_str[PROP_VALUE_MAX] = {0};
   system_property_get("getprop", "ro.build.version.release",
         os_version_str);

   *major  = 0;
   *minor  = 0;
   *rel    = 0;

   /* Parse out the OS version numbers from the system properties. */
   if (os_version_str[0])
   {
      /* Try to parse out the version numbers from the string. */
      int num_read = sscanf(os_version_str, "%d.%d.%d", major, minor, rel);

      if (num_read > 0)
      {
         if (num_read < 2)
            *minor = 0;
         if (num_read < 3)
            *rel = 0;
         return;
      }
   }
}

static void frontend_android_get_version_sdk(int32_t *sdk)
{
   char os_version_str[PROP_VALUE_MAX] = {0};
   system_property_get("getprop", "ro.build.version.sdk", os_version_str);

   *sdk = 0;
   if (os_version_str[0])
   {
      int num_read = sscanf(os_version_str, "%d", sdk);
      (void) num_read;
   }
}

static bool device_is_xperia_play(const char *name)
{
   if (
         strstr(name, "R800x") ||
         strstr(name, "R800at") ||
         strstr(name, "R800i") ||
         strstr(name, "R800a") ||
         strstr(name, "SO-01D")
      )
      return true;

   return false;
}

static bool device_is_game_console(const char *name)
{
   if (
         strstr(name, "OUYA Console") ||
         device_is_xperia_play(name) ||
         strstr(name, "GAMEMID_BT") ||
         strstr(name, "S7800") ||
         strstr(name, "XD\n") ||
         strstr(name, "ARCHOS GAMEPAD") ||
         strstr(name, "SHIELD Android TV") ||
         strstr(name, "SHIELD\n")
      )
      return true;

   return false;
}

bool test_permissions(const char *path)
{
   char buf[PATH_MAX_LENGTH];
   bool ret;

   RARCH_LOG("Testing permissions for %s\n",path);

   fill_pathname_join(buf, path, ".retroarch", sizeof(buf));
   ret = path_mkdir(buf);

   RARCH_LOG("Create %s %s\n", buf, ret ? "true" : "false");

   if(ret)
      rmdir(buf);

   return ret;
}

static void frontend_android_shutdown(bool unused)
{
   (void)unused;
   /* Cleaner approaches don't work sadly. */
   exit(0);
}

#else
static const char *proc_apm_path             = "/proc/apm";
static const char *proc_acpi_battery_path    = "/proc/acpi/battery";
static const char *proc_acpi_sysfs_ac_adapter_path= "/sys/class/power_supply/ACAD";
static const char *proc_acpi_sysfs_battery_path= "/sys/class/power_supply";
static const char *proc_acpi_ac_adapter_path = "/proc/acpi/ac_adapter";


static bool make_proc_acpi_key_val(char **_ptr, char **_key, char **_val)
{
    char *ptr = *_ptr;

    while (*ptr == ' ')
        ptr++;  /* skip whitespace. */

    if (*ptr == '\0')
        return false;  /* EOF. */

    *_key = ptr;

    while ((*ptr != ':') && (*ptr != '\0'))
        ptr++;

    if (*ptr == '\0')
        return false;  /* (unexpected) EOF. */

    *(ptr++) = '\0';  /* terminate the key. */

    while ((*ptr == ' ') && (*ptr != '\0'))
        ptr++;  /* skip whitespace. */

    if (*ptr == '\0')
        return false;  /* (unexpected) EOF. */

    *_val = ptr;

    while ((*ptr != '\n') && (*ptr != '\0'))
        ptr++;

    if (*ptr != '\0')
        *(ptr++) = '\0';  /* terminate the value. */

    *_ptr = ptr;  /* store for next time. */
    return true;
}

#define ACPI_KEY_STATE                 0x10614a06U
#define ACPI_KEY_PRESENT               0xc28ac046U
#define ACPI_KEY_CHARGING_STATE        0x5ba13e29U
#define ACPI_KEY_REMAINING_CAPACITY    0xf36952edU
#define ACPI_KEY_DESIGN_CAPACITY       0x05e6488dU

#define ACPI_VAL_CHARGING_DISCHARGING  0xf268327aU
#define ACPI_VAL_CHARGING              0x095ee228U
#define ACPI_VAL_YES                   0x0b88c316U
#define ACPI_VAL_ONLINE                0x6842bf17U

static void check_proc_acpi_battery(const char * node, bool * have_battery,
      bool * charging, int *seconds, int *percent)
{
   const char *base  = proc_acpi_battery_path;
   char path[1024];
   ssize_t length    = 0;
   char         *ptr = NULL;
   char  *buf        = NULL;
   char  *buf_info   = NULL;
   char         *key = NULL;
   char         *val = NULL;
   bool       charge = false;
   bool       choose = false;
   int       maximum = -1;
   int     remaining = -1;
   int          secs = -1;
   int           pct = -1;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "state");

   if (!retro_read_file(path, (void**)&buf, &length))
      goto end;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "info");
   if (!retro_read_file(path, (void**)&buf_info, &length))
      goto end;

   ptr = &buf[0];

   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      uint32_t key_hash = djb2_calculate(key);
      uint32_t val_hash = djb2_calculate(val);

      switch (key_hash)
      {
         case ACPI_KEY_PRESENT:
            if (val_hash == ACPI_VAL_YES)
               *have_battery = true;
            break;
         case ACPI_KEY_CHARGING_STATE:
            switch (val_hash)
            {
               case ACPI_VAL_CHARGING_DISCHARGING:
               case ACPI_VAL_CHARGING:
                  charge = true;
                  break;
            }
            break;
         case ACPI_KEY_REMAINING_CAPACITY:
            {
               char  *endptr = NULL;
               const int cvt = (int)strtol(val, &endptr, 10);

               if (*endptr == ' ')
                  remaining = cvt;
            }
            break;
      }
   }

   ptr = &buf_info[0];

   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      uint32_t key_hash = djb2_calculate(key);

      switch (key_hash)
      {
         case ACPI_KEY_DESIGN_CAPACITY:
            {
               char  *endptr = NULL;
               const int cvt = (int)strtol(val, &endptr, 10);

               if (*endptr == ' ')
                  maximum = cvt;
            }
            break;
      }
   }

   if ((maximum >= 0) && (remaining >= 0))
   {
      pct = (int) ((((float) remaining) / ((float) maximum)) * 100.0f);
      if (pct < 0)
         pct = 0;
      if (pct > 100)
         pct = 100;
   }

   /* !!! FIXME: calculate (secs). */

   /*
    * We pick the battery that claims to have the most minutes left.
    *  (failing a report of minutes, we'll take the highest percent.)
    */
   if ((secs < 0) && (*seconds < 0))
   {
      if ((pct < 0) && (*percent < 0))
         choose = true;  /* at least we know there's a battery. */
      if (pct > *percent)
         choose = true;
   }
   else if (secs > *seconds)
      choose = true;

   if (choose)
   {
      *seconds = secs;
      *percent = pct;
      *charging = charge;
   }

end:
   if (buf_info)
      free(buf_info);
   if (buf)
      free(buf);
   buf      = NULL;
   buf_info = NULL;
}

static void check_proc_acpi_sysfs_battery(const char *node,
      bool *have_battery, bool *charging,
      int *seconds, int *percent)
{
   unsigned capacity;
   char path[1024], info[1024];
   const char *base  = proc_acpi_sysfs_battery_path;
   char        *buf  = NULL;
   char         *ptr = NULL;
   char         *key = NULL;
   char         *val = NULL;
   bool       charge = false;
   bool       choose = false;
   ssize_t length    = 0;
   int       maximum = -1;
   int     remaining = -1;
   int          secs = -1;
   int           pct = -1;

   if (!strstr(node, "BAT"))
      return;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "status");
   if (retro_read_file(path, (void**)&buf, &length) != 1)
      return;

   if (strstr((char*)buf, "Discharging"))
      *have_battery = true;
   else if (strstr((char*)buf, "Full"))
      *have_battery = true;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "capacity");
   if (retro_read_file(path, (void**)&buf, &length) != 1)
      goto end;

   capacity = atoi(buf);

   *percent = capacity;

end:
   if (buf)
      free(buf);
   buf = NULL;
}

static void check_proc_acpi_ac_adapter(const char * node, bool *have_ac)
{
   char path[1024];
   const char *base = proc_acpi_ac_adapter_path;
   char       *buf  = NULL;
   char        *ptr = NULL;
   char        *key = NULL;
   char        *val = NULL;
   ssize_t length   = 0;

   snprintf(path, sizeof(path), "%s/%s/%s", base, node, "state");
   if (retro_read_file(path, (void**)&buf, &length) != 1)
      return;

   ptr = &buf[0];
   while (make_proc_acpi_key_val(&ptr, &key, &val))
   {
      uint32_t key_hash = djb2_calculate(key);
      uint32_t val_hash = djb2_calculate(val);

      if (key_hash == ACPI_KEY_STATE &&
            val_hash == ACPI_VAL_ONLINE)
         *have_ac = true;
   }

   if (buf)
      free(buf);
   buf = NULL;
}

static void check_proc_acpi_sysfs_ac_adapter(const char * node, bool *have_ac)
{
   char  path[1024];
   ssize_t length   = 0;
   char     *buf    = NULL;
   const char *base = proc_acpi_sysfs_ac_adapter_path;

   snprintf(path, sizeof(path), "%s/%s", base, "online");
   if (retro_read_file(path, (void**)&buf, &length) != 1)
      return;

   if (strstr((char*)buf, "1"))
      *have_ac = true;

   if (buf)
      free(buf);
   buf = NULL;
}

static bool next_string(char **_ptr, char **_str)
{
   char *ptr = *_ptr;
   char *str = *_str;

   while (*ptr == ' ')       /* skip any spaces... */
      ptr++;

   if (*ptr == '\0')
      return false;

   str = ptr;
   while ((*ptr != ' ') && (*ptr != '\n') && (*ptr != '\0'))
      ptr++;

   if (*ptr != '\0')
      *(ptr++) = '\0';

   *_str = str;
   *_ptr = ptr;
   return true;
}

static bool int_string(char *str, int *val)
{
   char *endptr = NULL;
   *val = (int) strtol(str, &endptr, 0);
   return ((*str != '\0') && (*endptr == '\0'));
}

static bool frontend_linux_powerstate_check_apm(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   char *ptr;
   int ac_status       = 0;
   int battery_status  = 0;
   int battery_flag    = 0;
   int battery_percent = 0;
   int battery_time    = 0;
   ssize_t length      = 0;
   char  *buf          = NULL;
   char *str           = NULL;

   if (retro_read_file(proc_apm_path, (void**)&buf, &length) != 1)
      goto error;

   ptr                 = &buf[0];

   if (!next_string(&ptr, &str))     /* driver version */
      goto error;
   if (!next_string(&ptr, &str))     /* BIOS version */
      goto error;
   if (!next_string(&ptr, &str))     /* APM flags */
      goto error;

   if (!next_string(&ptr, &str))     /* AC line status */
      goto error;
   else if (!int_string(str, &ac_status))
      goto error;

   if (!next_string(&ptr, &str))     /* battery status */
      goto error;
   else if (!int_string(str, &battery_status))
      goto error;

   if (!next_string(&ptr, &str))     /* battery flag */
      goto error;
   else if (!int_string(str, &battery_flag))
      goto error;
   if (!next_string(&ptr, &str))    /* remaining battery life percent */
      goto error;
   if (str[strlen(str) - 1] == '%')
      str[strlen(str) - 1] = '\0';
   if (!int_string(str, &battery_percent))
      goto error;

   if (!next_string(&ptr, &str))     /* remaining battery life time */
      goto error;
   else if (!int_string(str, &battery_time))
      goto error;

   if (!next_string(&ptr, &str))     /* remaining battery life time units */
      goto error;
   else if (string_is_equal(str, "min"))
      battery_time *= 60;

   if (battery_flag == 0xFF) /* unknown state */
      *state = FRONTEND_POWERSTATE_NONE;
   else if (battery_flag & (1 << 7))       /* no battery */
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (battery_flag & (1 << 3))   /* charging */
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (ac_status == 1)
      *state = FRONTEND_POWERSTATE_CHARGED;        /* on AC, not charging. */
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   if (battery_percent >= 0)         /* -1 == unknown */
      *percent = (battery_percent > 100) ? 100 : battery_percent; /* clamp between 0%, 100% */
   if (battery_time >= 0)            /* -1 == unknown */
      *seconds = battery_time;

   if (buf)
      free(buf);
   buf = NULL;

   return true;

error:
   if (buf)
      free(buf);
   buf = NULL;

   return false;
}

static bool frontend_linux_powerstate_check_acpi(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   bool ret            = false;
   struct RDIR *entry  = NULL;
   bool have_battery   = false;
   bool have_ac        = false;
   bool charging       = false;

   *state = FRONTEND_POWERSTATE_NONE;

   entry = retro_opendir(proc_acpi_battery_path);
   if (!entry)
      goto end;

   if (retro_dirent_error(entry))
      goto end;

   while (retro_readdir(entry))
      check_proc_acpi_battery(retro_dirent_get_name(entry),
            &have_battery, &charging, seconds, percent);

   retro_closedir(entry);

   entry = retro_opendir(proc_acpi_ac_adapter_path);
   if (!entry)
      goto end;

   while (retro_readdir(entry))
      check_proc_acpi_ac_adapter(
            retro_dirent_get_name(entry), &have_ac);

   if (!have_battery)
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   else if (charging)
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      *state = FRONTEND_POWERSTATE_CHARGED;
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   ret = true;

end:
   if (entry)
      retro_closedir(entry);

   return ret;
}

static bool frontend_linux_powerstate_check_acpi_sysfs(
      enum frontend_powerstate *state,
      int *seconds, int *percent)
{
   bool ret            = false;
   struct RDIR *entry  = NULL;
   bool have_battery   = false;
   bool have_ac        = false;
   bool charging       = false;

   *state = FRONTEND_POWERSTATE_NONE;

   entry = retro_opendir(proc_acpi_sysfs_battery_path);
   if (!entry)
      goto error;

   if (retro_dirent_error(entry))
      goto error;

   while (retro_readdir(entry))
      check_proc_acpi_sysfs_battery(retro_dirent_get_name(entry),
            &have_battery, &charging, seconds, percent);

   retro_closedir(entry);

   entry = retro_opendir(proc_acpi_sysfs_ac_adapter_path);
   if (!entry)
      goto error;

   check_proc_acpi_sysfs_ac_adapter(retro_dirent_get_name(entry), &have_ac);

   if (!have_battery)
   {
      *state = FRONTEND_POWERSTATE_NO_SOURCE;
   }
   else if (charging)
      *state = FRONTEND_POWERSTATE_CHARGING;
   else if (have_ac)
      *state = FRONTEND_POWERSTATE_CHARGED;
   else
      *state = FRONTEND_POWERSTATE_ON_POWER_SOURCE;

   return true;

error:
   if (entry)
      retro_closedir(entry);

   return false;
}
#endif

static int frontend_linux_get_rating(void)
{
#ifdef ANDROID
   char device_model[PROP_VALUE_MAX] = {0};
   frontend_android_get_name(device_model, sizeof(device_model));

   RARCH_LOG("ro.product.model: (%s).\n", device_model);

   if (device_is_xperia_play(device_model))
      return 6;
   else if (strstr(device_model, "GT-I9505"))
      return 12;
   else if (strstr(device_model, "SHIELD"))
      return 13;
#endif
   return -1;
}

static enum frontend_powerstate frontend_linux_get_powerstate(
      int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;

#ifndef ANDROID
   if (frontend_linux_powerstate_check_acpi_sysfs(&ret, seconds, percent))
      return ret;

   if (frontend_linux_powerstate_check_acpi(&ret, seconds, percent))
      return ret;

   if (frontend_linux_powerstate_check_apm(&ret, seconds, percent))
      return ret;
#endif

   return ret;
}

#define LINUX_ARCH_X86_64     0x23dea434U
#define LINUX_ARCH_X86        0x0b88b8cbU
#define LINUX_ARCH_ARM        0x0b885ea5U
#define LINUX_ARCH_PPC64      0x1028cf52U
#define LINUX_ARCH_MIPS       0x7c9aa25eU
#define LINUX_ARCH_TILE       0x7c9e7873U

#define ANDROID_ARCH_ARMV7    0x26257a91U
#define ANDROID_ARCH_ARM      0x406a3516U

static enum frontend_architecture frontend_linux_get_architecture(void)
{
   uint32_t buffer_hash;
   const char *val;
#ifdef ANDROID
   char abi[PROP_VALUE_MAX] = {0};
   system_property_get("getprop", "ro.product.cpu.abi", abi);
   val         = abi;
#else
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return FRONTEND_ARCH_NONE;

   val         = buffer.machine;
#endif
   buffer_hash = djb2_calculate(val);

   switch (buffer_hash)
   {
#ifdef ANDROID
      case ANDROID_ARCH_ARMV7:
         return FRONTEND_ARCH_ARM;
      case ANDROID_ARCH_ARM:
         return FRONTEND_ARCH_ARM;
#endif
      case LINUX_ARCH_X86_64:
         return FRONTEND_ARCH_X86_64;
      case LINUX_ARCH_X86:
         return FRONTEND_ARCH_X86;
      case LINUX_ARCH_ARM:
         return FRONTEND_ARCH_ARM;
      case LINUX_ARCH_PPC64:
         return FRONTEND_ARCH_PPC;
      case LINUX_ARCH_MIPS:
         return FRONTEND_ARCH_MIPS;
      case LINUX_ARCH_TILE:
         return FRONTEND_ARCH_TILE;
   }

   return FRONTEND_ARCH_NONE;
}

static void frontend_linux_get_os(char *s,
      size_t len, int *major, int *minor)
{
#ifdef ANDROID
   int rel;
   frontend_android_get_version(major, minor, &rel);

   strlcpy(s, "Android", len);
#else
   unsigned krel;
   struct utsname buffer;

   if (uname(&buffer) != 0)
      return;

   sscanf(buffer.release, "%d.%d.%u", major, minor, &krel);
   strlcpy(s, "Linux", len);
#endif
}

static void frontend_linux_get_env(int *argc,
      char *argv[], void *data, void *params_data)
{
#ifdef ANDROID
   int32_t major, minor, rel;
   char device_model[PROP_VALUE_MAX] = {0};
   char device_id[PROP_VALUE_MAX]    = {0};
   struct rarch_main_wrap      *args = NULL;
   JNIEnv                       *env = NULL;
   jobject                       obj = NULL;
   jstring                      jstr = NULL;
   struct android_app   *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   env = jni_thread_getenv();
   if (!env)
      return;

   args = (struct rarch_main_wrap*)params_data;

   if (args)
   {
      args->touched    = true;
      args->no_content = false;
      args->verbose    = false;
      args->sram_path  = NULL;
      args->state_path = NULL;
   }

   frontend_android_get_version(&major, &minor, &rel);

   RARCH_LOG("Android OS version (major : %d, minor : %d, rel : %d)\n",
         major, minor, rel);

   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         android_app->getIntent);
   RARCH_LOG("Checking arguments passed from intent ...\n");

   /* Config file. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "CONFIGFILE"));

   if (android_app->getStringExtra && jstr)
   {
      static char config_path[PATH_MAX_LENGTH] = {0};
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      if (argv && *argv)
         strlcpy(config_path, argv, sizeof(config_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Config file: [%s].\n", config_path);
      if (args && *config_path)
         args->config_path = config_path;
   }

   /* Current IME. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "IME"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      strlcpy(android_app->current_ime, argv,
            sizeof(android_app->current_ime));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Current IME: [%s].\n", android_app->current_ime);
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "USED"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);
      bool used = (string_is_equal(argv, "false")) ? false : true;

      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("USED: [%s].\n", used ? "true" : "false");
   }

   /* LIBRETRO. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "LIBRETRO"));

   if (android_app->getStringExtra && jstr)
   {
      static char core_path[PATH_MAX_LENGTH];
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *core_path = '\0';
      if (argv && *argv)
         strlcpy(core_path, argv, sizeof(core_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      RARCH_LOG("Libretro path: [%s]\n", core_path);
      if (args && *core_path)
         args->libretro_path = core_path;
   }

   /* Content. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "ROM"));

   if (android_app->getStringExtra && jstr)
   {
      static char path[PATH_MAX_LENGTH];
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *path = '\0';

      if (argv && *argv)
         strlcpy(path, argv, sizeof(path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*path)
      {
         RARCH_LOG("Auto-start game %s.\n", path);
         if (args && *path)
            args->content_path = path;
      }
   }

   /* External Storage */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "SDCARD"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *sdcard_dir = '\0';

      if (argv && *argv)
         strlcpy(sdcard_dir, argv, sizeof(sdcard_dir));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*sdcard_dir)
      {
         RARCH_LOG("External storage location [%s]\n", sdcard_dir);
         /* TODO base dir handler */
      }
   }

   /* Screenshots */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "SCREENSHOTS"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *screenshot_dir = '\0';

      if (argv && *argv)
         strlcpy(screenshot_dir, argv, sizeof(screenshot_dir));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*screenshot_dir)
      {
         RARCH_LOG("Picture folder location [%s]\n", screenshot_dir);
         /* TODO: screenshot handler */
      }
   }

   /* Downloads */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "DOWNLOADS"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *downloads_dir = '\0';

      if (argv && *argv)
         strlcpy(downloads_dir, argv, sizeof(downloads_dir));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*downloads_dir)
      {
         RARCH_LOG("Download folder location [%s].\n", downloads_dir);
         /* TODO: downloads handler */
      }
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "APK"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *apk_path = '\0';

      if (argv && *argv)
         strlcpy(apk_path, argv, sizeof(apk_path));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*apk_path)
      {
         RARCH_LOG("APK location [%s].\n", apk_path);
      }
   }

   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "EXTERNAL"));

   if (android_app->getStringExtra && jstr)
   {
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *ext_dir = '\0';

      if (argv && *argv)
         strlcpy(ext_dir, argv, sizeof(ext_dir));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      if (*ext_dir)
      {
         RARCH_LOG("External files location [%s]\n", ext_dir);
      }
   }

   /* Content. */
   CALL_OBJ_METHOD_PARAM(env, jstr, obj, android_app->getStringExtra,
         (*env)->NewStringUTF(env, "DATADIR"));

   if (android_app->getStringExtra && jstr)
   {
      int perms = 0;
      const char *argv = (*env)->GetStringUTFChars(env, jstr, 0);

      *app_dir = '\0';

      if (argv && *argv)
         strlcpy(app_dir, argv, sizeof(app_dir));
      (*env)->ReleaseStringUTFChars(env, jstr, argv);

      //set paths depending on the ability to write to sdcard_dir

      if(*sdcard_dir)
      {
         if(test_permissions(sdcard_dir))
            perms = SDCARD_ROOT_WRITABLE;
      }
      else if(*ext_dir)
      {
         if(test_permissions(ext_dir))
            perms = SDCARD_EXT_DIR_WRITABLE;
      }
      else
         perms = SDCARD_NOT_WRITABLE;

      RARCH_LOG("SD permissions: %d",perms);

      if (*app_dir)
      {
         RARCH_LOG("Application location: [%s].\n", app_dir);
         if (args && *app_dir)
         {
            char buf[PATH_MAX_LENGTH];

            fill_pathname_join(g_defaults.dir.assets, app_dir,
                  "assets", sizeof(g_defaults.dir.assets));
            fill_pathname_join(g_defaults.dir.cache, app_dir,
                  "tmp", sizeof(g_defaults.dir.cache));
            fill_pathname_join(g_defaults.dir.shader, app_dir,
                  "shaders", sizeof(g_defaults.dir.shader));
            fill_pathname_join(g_defaults.dir.overlay, app_dir,
                  "overlays", sizeof(g_defaults.dir.overlay));
            fill_pathname_join(g_defaults.dir.osk_overlay, app_dir,
                  "overlays", sizeof(g_defaults.dir.osk_overlay));
            fill_pathname_join(g_defaults.dir.core, app_dir,
                  "cores", sizeof(g_defaults.dir.core));
            fill_pathname_join(g_defaults.dir.core_info,
                  app_dir, "info", sizeof(g_defaults.dir.core_info));
            fill_pathname_join(g_defaults.dir.autoconfig,
                  app_dir, "autoconfig", sizeof(g_defaults.dir.autoconfig));
            fill_pathname_join(g_defaults.dir.audio_filter,
                  app_dir, "filters/audio", sizeof(g_defaults.dir.audio_filter));
            fill_pathname_join(g_defaults.dir.video_filter,
                  app_dir, "filters/video", sizeof(g_defaults.dir.video_filter));
            strlcpy(g_defaults.dir.content_history,
                  app_dir, sizeof(g_defaults.dir.content_history));
            fill_pathname_join(g_defaults.dir.database,
                  app_dir, "database/rdb", sizeof(g_defaults.dir.database));
            fill_pathname_join(g_defaults.dir.cursor,
                  app_dir, "database/cursors", sizeof(g_defaults.dir.cursor));
            fill_pathname_join(g_defaults.dir.cheats,
                  app_dir, "cheats", sizeof(g_defaults.dir.cheats));
            fill_pathname_join(g_defaults.dir.playlist,
                  app_dir, "playlists", sizeof(g_defaults.dir.playlist));
            fill_pathname_join(g_defaults.dir.remap,
                  app_dir, "remaps", sizeof(g_defaults.dir.remap));
            fill_pathname_join(g_defaults.dir.wallpapers,
                  app_dir, "wallpapers", sizeof(g_defaults.dir.wallpapers));
            if(*downloads_dir && test_permissions(downloads_dir))
            {
               fill_pathname_join(g_defaults.dir.core_assets,
                     downloads_dir, "", sizeof(g_defaults.dir.core_assets));
            }
            else
            {
               fill_pathname_join(g_defaults.dir.core_assets,
                     app_dir, "downloads", sizeof(g_defaults.dir.core_assets));
               path_mkdir(g_defaults.dir.core_assets);
            }

            RARCH_LOG("Default download folder: [%s]", g_defaults.dir.core_assets);

            if(*screenshot_dir && test_permissions(screenshot_dir))
            {
               fill_pathname_join(g_defaults.dir.screenshot,
                     screenshot_dir, "", sizeof(g_defaults.dir.screenshot));
            }
            else
            {
               fill_pathname_join(g_defaults.dir.screenshot,
                     app_dir, "screenshots", sizeof(g_defaults.dir.screenshot));
               path_mkdir(g_defaults.dir.screenshot);
            }

            RARCH_LOG("Default screenshot folder: [%s]", g_defaults.dir.screenshot);

            switch (perms)
            {
               case SDCARD_EXT_DIR_WRITABLE:
                  fill_pathname_join(g_defaults.dir.sram,
                        ext_dir, "saves", sizeof(g_defaults.dir.sram));
                  path_mkdir(g_defaults.dir.sram);

                  fill_pathname_join(g_defaults.dir.savestate,
                        ext_dir, "states", sizeof(g_defaults.dir.savestate));
                  path_mkdir(g_defaults.dir.savestate);

                  fill_pathname_join(g_defaults.dir.system,
                        ext_dir, "system", sizeof(g_defaults.dir.system));
                  path_mkdir(g_defaults.dir.system);

                  fill_pathname_join(g_defaults.dir.menu_config,
                        ext_dir, "config", sizeof(g_defaults.dir.menu_config));
                  path_mkdir(g_defaults.dir.menu_config);

                  break;
               case SDCARD_NOT_WRITABLE:
                  fill_pathname_join(g_defaults.dir.sram,
                        app_dir, "saves", sizeof(g_defaults.dir.sram));
                  path_mkdir(g_defaults.dir.sram);
                  fill_pathname_join(g_defaults.dir.savestate,
                        app_dir, "states", sizeof(g_defaults.dir.savestate));
                  path_mkdir(g_defaults.dir.savestate);
                  fill_pathname_join(g_defaults.dir.system,
                        app_dir, "system", sizeof(g_defaults.dir.system));
                  path_mkdir(g_defaults.dir.system);

                  fill_pathname_join(g_defaults.dir.menu_config,
                        app_dir, "config", sizeof(g_defaults.dir.menu_config));
                  path_mkdir(g_defaults.dir.menu_config);
                  break;
               case SDCARD_ROOT_WRITABLE:
               default:
                  break;
            }

            /* create save and system directories in the internal dir too */
            fill_pathname_join(buf,
                  app_dir, "saves", sizeof(buf));
            path_mkdir(buf);

            fill_pathname_join(buf,
                  app_dir, "states", sizeof(buf));
            path_mkdir(buf);

            fill_pathname_join(buf,
                  app_dir, "system", sizeof(buf));
            path_mkdir(buf);

            /* create save and system directories in the internal sd too */
            fill_pathname_join(buf,
                  ext_dir, "saves", sizeof(buf));
            path_mkdir(buf);

            fill_pathname_join(buf,
                  ext_dir, "states", sizeof(buf));
            path_mkdir(buf);

            fill_pathname_join(buf,
                  ext_dir, "system", sizeof(buf));
            path_mkdir(buf);

            RARCH_LOG("Default savefile folder: [%s]",   g_defaults.dir.sram);
            RARCH_LOG("Default savestate folder: [%s]",  g_defaults.dir.savestate);
            RARCH_LOG("Default system folder: [%s]",     g_defaults.dir.system);
         }
      }
   }

   frontend_android_get_name(device_model, sizeof(device_model));
   system_property_get("getprop", "ro.product.id", device_id);

   g_defaults.settings.video_threaded_enable = true;

   /* Set automatic default values per device */
   if (device_is_xperia_play(device_model))
   {
      g_defaults.settings.out_latency = 128;
      g_defaults.settings.video_refresh_rate = 59.19132938771038;
      g_defaults.settings.video_threaded_enable = false;
   }
   else if (strstr(device_model, "GAMEMID_BT"))
      g_defaults.settings.out_latency = 160;
   else if (strstr(device_model, "SHIELD"))
      g_defaults.settings.video_refresh_rate = 60.0;
   else if (strstr(device_model, "JSS15J"))
      g_defaults.settings.video_refresh_rate = 59.65;


   /* Explicitly disable input overlay by default
    * for gamepad-like/console devices. */

   if (device_is_game_console(device_model))
   {
      snprintf(g_defaults.settings.menu, sizeof(g_defaults.settings.menu), "xmb");
   }

#else
   char base_path[PATH_MAX];
   const char *xdg  = getenv("XDG_CONFIG_HOME");
   const char *home = getenv("HOME");

   if (xdg)
      snprintf(base_path, sizeof(base_path),
            "%s/retroarch", xdg);
   else if (home)
      snprintf(base_path, sizeof(base_path),
            "%s/.config/retroarch", home);
   else
      snprintf(base_path, sizeof(base_path), "retroarch");

   fill_pathname_join(g_defaults.dir.core, base_path,
         "cores", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.core_info, base_path,
         "cores", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.autoconfig, base_path,
         "autoconf", sizeof(g_defaults.dir.autoconfig));
   fill_pathname_join(g_defaults.dir.assets, base_path,
         "assets", sizeof(g_defaults.dir.assets));
   fill_pathname_join(g_defaults.dir.remap, base_path,
         "remap", sizeof(g_defaults.dir.remap));
   fill_pathname_join(g_defaults.dir.playlist, base_path,
         "playlists", sizeof(g_defaults.dir.playlist));
   fill_pathname_join(g_defaults.dir.cursor, base_path,
         "database/cursors", sizeof(g_defaults.dir.cursor));
   fill_pathname_join(g_defaults.dir.database, base_path,
         "database/rdb", sizeof(g_defaults.dir.database));
   fill_pathname_join(g_defaults.dir.shader, base_path,
         "shaders", sizeof(g_defaults.dir.shader));
   fill_pathname_join(g_defaults.dir.cheats, base_path,
         "cheats", sizeof(g_defaults.dir.cheats));
   fill_pathname_join(g_defaults.dir.overlay, base_path,
         "overlay", sizeof(g_defaults.dir.overlay));
   fill_pathname_join(g_defaults.dir.osk_overlay, base_path,
         "overlay", sizeof(g_defaults.dir.osk_overlay));
   fill_pathname_join(g_defaults.dir.core_assets, base_path,
         "downloads", sizeof(g_defaults.dir.core_assets));
   fill_pathname_join(g_defaults.dir.screenshot, base_path,
         "screenshots", sizeof(g_defaults.dir.screenshot));
#endif
}

#ifdef ANDROID
static void free_saved_state(struct android_app* android_app)
{
    slock_lock(android_app->mutex);

    if (android_app->savedState != NULL)
    {
        free(android_app->savedState);
        android_app->savedState = NULL;
        android_app->savedStateSize = 0;
    }

    slock_unlock(android_app->mutex);
}

static void android_app_destroy(struct android_app *android_app)
{
   JNIEnv *env = NULL;

   RARCH_LOG("android_app_destroy\n");
   free_saved_state(android_app);

   slock_lock(android_app->mutex);

   env = jni_thread_getenv();

   if (env && android_app->onRetroArchExit)
      CALL_VOID_METHOD(env, android_app->activity->clazz,
            android_app->onRetroArchExit);

   if (android_app->inputQueue)
      AInputQueue_detachLooper(android_app->inputQueue);

   AConfiguration_delete(android_app->config);
   android_app->destroyed = 1;
   scond_broadcast(android_app->cond);
   slock_unlock(android_app->mutex);
   /* Can't touch android_app object after this. */
}
#endif

static void frontend_linux_deinit(void *data)
{
#ifdef ANDROID
   struct android_app *android_app = (struct android_app*)data;

   if (!android_app)
      return;

   android_app_destroy(android_app);
#endif
}

static void frontend_linux_init(void *data)
{
#ifdef ANDROID
   JNIEnv                     *env = NULL;
   ALooper                 *looper = NULL;
   jclass                    class = NULL;
   jobject                     obj = NULL;
   struct android_app* android_app = (struct android_app*)data;

   if (!android_app)
      return;

   android_app->config             = AConfiguration_new();
   AConfiguration_fromAssetManager(android_app->config,
         android_app->activity->assetManager);

   looper = (ALooper*)ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
   ALooper_addFd(looper, android_app->msgread, LOOPER_ID_MAIN,
         ALOOPER_EVENT_INPUT, NULL, NULL);
   android_app->looper = looper;

   slock_lock(android_app->mutex);
   android_app->running = 1;
   scond_broadcast(android_app->cond);
   slock_unlock(android_app->mutex);

   memset(&g_android, 0, sizeof(g_android));
   g_android = (struct android_app*)android_app;

   RARCH_LOG("Waiting for Android Native Window to be initialized ...\n");

   while (!android_app->window)
   {
      if (!android_run_events(android_app))
      {
         frontend_linux_deinit(android_app);
         frontend_android_shutdown(android_app);
         return;
      }
   }

   RARCH_LOG("Android Native Window initialized.\n");

   env = jni_thread_getenv();
   if (!env)
      return;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   GET_METHOD_ID(env, android_app->getIntent, class,
         "getIntent", "()Landroid/content/Intent;");
   GET_METHOD_ID(env, android_app->onRetroArchExit, class,
         "onRetroArchExit", "()V");
   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz,
         android_app->getIntent);

   GET_OBJECT_CLASS(env, class, obj);
   GET_METHOD_ID(env, android_app->getStringExtra, class,
         "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
#endif

   if (!cpu_inited_once)
   {
      linux_cpu_init();
      cpu_inited_once = true;
   }
}

#ifdef ANDROID
static int frontend_android_parse_drive_list(void *data)
{
   file_list_t *list = (file_list_t*)data;

   // MENU_FILE_DIRECTORY is not working with labels, placeholders for now
   menu_entries_push(list,
         app_dir, "Application Dir", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         ext_dir, "External Application Dir", MENU_FILE_DIRECTORY, 0, 0);
   menu_entries_push(list,
         sdcard_dir, "Internal Memory", MENU_FILE_DIRECTORY, 0, 0);

   menu_entries_push(list, "/", "",
         MENU_FILE_DIRECTORY, 0, 0);

   return 0;
}
#endif

#ifndef HAVE_DYNAMIC
#include "../../retroarch.h"

static bool frontend_linux_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         linux_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         linux_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         linux_fork_mode  = FRONTEND_FORK_CORE;

         {
            char executable_path[PATH_MAX_LENGTH];
            settings_t *settings = config_get_ptr();

            fill_pathname_application_path(executable_path, sizeof(executable_path));
            strlcpy(settings->libretro, executable_path, sizeof(settings->libretro));
         }
         rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}

static void frontend_linux_exec(const char *path, bool should_load_game)
{
   char *newargv[]    = { NULL, NULL };
   size_t len         = strlen(path);

   newargv[0] = malloc(len);

   strlcpy(newargv[0], path, len);
}

static void frontend_linux_exitspawn(char *core_path, size_t core_path_size)
{
   bool should_load_game = false;

   if (linux_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (linux_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      case FRONTEND_FORK_NONE:
      default:
         break;
   }

   frontend_linux_exec(core_path, should_load_game);
}
#endif

frontend_ctx_driver_t frontend_ctx_linux = {
   frontend_linux_get_env,       /* environment_get */
   frontend_linux_init,          /* init */
   frontend_linux_deinit,        /* deinit */
#ifdef HAVE_DYNAMIC
   NULL,                         /* exitspawn */
#else
   frontend_linux_exitspawn,     /* exitspawn */
#endif
   NULL,                         /* process_args */
#ifdef HAVE_DYNAMIC
   NULL,                         /* exec */
   NULL,                         /* set_fork */
#else
   frontend_linux_exec,          /* exec */
   frontend_linux_set_fork,      /* set_fork */
#endif
#ifdef ANDROID
   frontend_android_shutdown,    /* shutdown */
   frontend_android_get_name,    /* get_name */
#else
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
#endif
   frontend_linux_get_os,
   frontend_linux_get_rating,    /* get_rating */
   NULL,                         /* load_content */
   frontend_linux_get_architecture,
   frontend_linux_get_powerstate,
#ifdef ANDROID
   frontend_android_parse_drive_list, /* parse_drive_list */
   "android",
#else
   NULL,                         /* parse_drive_list */
   "linux",
#endif
};
