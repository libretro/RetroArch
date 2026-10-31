/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include <adrenotools/priv.h>

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "vulkan_custom_driver.h"

#include "../../configuration.h"
#include "../../frontend/drivers/platform_unix.h"
#include "../../msg_hash.h"
#include "../../runloop.h"
#include "../../verbosity.h"

typedef void *(*custom_driver_open_libvulkan_t)(int dlopen_mode,
      int feature_flags, const char *tmp_dir, const char *hook_dir,
      const char *driver_dir, const char *driver_name,
      const char *redirect_dir, void **mapping);

/* One load's paths, on the heap: frames stay under 4 KiB. */
struct custom_driver_work
{
   char dir[PATH_MAX_LENGTH];    /* <base><key> */
   char tmp[PATH_MAX_LENGTH];    /* <base><key>.tmp */
   char file[PATH_MAX_LENGTH];
   char other[PATH_MAX_LENGTH];
   char member[PATH_MAX_LENGTH]; /* <zip>#<name> */
   char hook[PATH_MAX_LENGTH];   /* nativeLibraryDir/ */
};

static struct
{
   vulkan_custom_driver_session_t session;
   char base[PATH_MAX_LENGTH];   /* <internalDataPath>/vulkan_drivers/ */
   char marker[PATH_MAX_LENGTH];
   char state[PATH_MAX_LENGTH];
   const char *core;             /* of the open() in progress */
   custom_driver_open_libvulkan_t open_libvulkan;
   bool mesa;                    /* of the last driver open() returned */
   bool ready;
   bool pruned;
   bool safe_mode;
   bool safe_mode_msg;
   bool shortcut;
} custom_driver_st;

/* dladdr target: a data address needs no function-to-object cast. */
static const char custom_driver_anchor = 0;

static void custom_driver_msg(enum vulkan_custom_driver_msg msg)
{
   const char *s;
   enum msg_hash_enums id;
   switch (msg)
   {
      case VULKAN_CUSTOM_DRIVER_MSG_FAILED:
         id = MSG_VULKAN_CUSTOM_DRIVER_FAILED;
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_CRASHED:
         id = MSG_VULKAN_CUSTOM_DRIVER_CRASHED;
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_INVALID:
         id = MSG_VULKAN_CUSTOM_DRIVER_INVALID;
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_UNSUPPORTED:
         id = MSG_VULKAN_CUSTOM_DRIVER_UNSUPPORTED;
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_RESTART:
         id = MSG_VULKAN_CUSTOM_DRIVER_RESTART;
         break;
      default:
         return;
   }
   s = msg_hash_to_str(id);
   runloop_msg_queue_push(s, strlen(s), 2, 360, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
}

/* A whole small file, NUL-terminated; NULL with *len 0 when missing. */
static char *custom_driver_read(const char *path, int64_t *len)
{
   void *buf = NULL;
   if (!filestream_read_file(path, &buf, len))
   {
      *len = 0;
      return NULL;
   }
   return (char*)buf;
}

/* The crash state's key for the running core; "" for the menu. */
static const char *custom_driver_core(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   if (     runloop_st->current_core_type == CORE_TYPE_DUMMY
         || string_is_empty(runloop_st->system.info.library_name))
      return "";
   return runloop_st->system.info.library_name;
}

static bool custom_driver_save_state(void)
{
   return filestream_write_file_atomic(custom_driver_st.state,
         custom_driver_st.session.state,
         (int64_t)custom_driver_st.session.state_len);
}

/* Written before driver code runs, deleted once it is up or failed. */
static void custom_driver_mark(const char *path, const char *core)
{
   size_t cap = strlen(path) + strlen(core) + 2;
   char *line = (char*)malloc(cap);
   size_t len;
   if (!line)
      return;
   len = vulkan_custom_driver_crash_add(line, 0, cap, path, core);
   filestream_write_file(custom_driver_st.marker, line, (int64_t)len);
   free(line);
}

/* Deletes what an extraction leaves in dir, file by file, then dir.
 * Reads no meta.json, so a package this build rejects still goes. */
static void custom_driver_remove(struct custom_driver_work *w,
      const char *dir)
{
   struct RDIR *rdir = retro_opendir(dir);
   if (rdir)
   {
      while (retro_readdir(rdir))
      {
         if (retro_dirent_is_dir(rdir, NULL))
            continue;
         fill_pathname_join_special(w->file, dir,
               retro_dirent_get_name(rdir), sizeof(w->file));
         filestream_delete(w->file);
      }
      retro_closedir(rdir);
   }
   path_rmdir(dir);
}

/* True when the dir named key still matches its source zip. */
static bool custom_driver_current_dir(struct custom_driver_work *w,
      const char *key)
{
   int64_t len   = 0;
   int64_t mtime = 0;
   int64_t size;
   char *zip;
   bool keep     = false;
   fill_pathname_join_special(w->file, w->dir, "source", sizeof(w->file));
   if (!(zip = custom_driver_read(w->file, &len)))
      return false;
   if (     (size = path_get_size(zip)) >= 0
         && path_get_mtime(zip, &mtime))
   {
      snprintf(w->other, sizeof(w->other), "%08x",
            (unsigned)vulkan_custom_driver_key(zip, size, mtime));
      keep = string_is_equal(w->other, key);
   }
   free(zip);
   return keep;
}

/* Once per process, before any driver is mapped: drops each extraction
 * whose zip is gone or changed, and any half-done one. No recursion. */
static void custom_driver_prune(struct custom_driver_work *w)
{
   struct RDIR *rdir = retro_opendir(custom_driver_st.base);
   if (!rdir)
      return;
   while (retro_readdir(rdir))
   {
      const char *name = retro_dirent_get_name(rdir);
      size_t len       = strlen(name);
      if (     !retro_dirent_is_dir(rdir, NULL)
            || !(len == 8 || (len == 12 && string_is_equal(name + 8, ".tmp")))
            || strspn(name, "0123456789abcdef") != 8)
         continue;
      fill_pathname_join_special(w->dir, custom_driver_st.base, name,
            sizeof(w->dir));
      if (len == 8 && custom_driver_current_dir(w, name))
         continue;
      custom_driver_remove(w, w->dir);
   }
   retro_closedir(rdir);
}

/* True when w->dir holds a finished extraction; fills *meta. */
static bool custom_driver_extracted(struct custom_driver_work *w,
      struct vulkan_custom_driver_meta *meta)
{
   int64_t len = 0;
   char *buf;
   bool ok     = false;
   fill_pathname_join_special(w->file, w->dir, "meta.json", sizeof(w->file));
   if (!(buf = custom_driver_read(w->file, &len)))
      return false;
   if (!vulkan_custom_driver_parse_meta(buf, (size_t)len,
            g_android->activity->sdkVersion, meta))
   {
      fill_pathname_join_special(w->file, w->dir, meta->library_name,
            sizeof(w->file));
      ok = path_is_valid(w->file);
   }
   free(buf);
   return ok;
}

/* Extraction matches the member name ignoring case, so only one member
 * may match, or it could write one other than the one checked. */
static bool custom_driver_one_member(const struct string_list *members,
      const char *name)
{
   size_t i;
   unsigned n = 0;
   for (i = 0; i < members->size; i++)
      if (string_is_equal_noncase(members->elems[i].data, name))
         n++;
   return n == 1;
}

/* Checks the package at path and extracts it into w->dir. Returns NULL,
 * or the reason it is not usable. */
static const char *custom_driver_extract(struct custom_driver_work *w,
      const char *path, struct vulkan_custom_driver_meta *meta)
{
   unsigned char head[VULKAN_CUSTOM_DRIVER_ELF_HEAD];
   uint64_t size              = 0;
   int64_t len                = 0;
   int64_t got                = 0;
   void *buf                  = NULL;
   RFILE *file                = NULL;
   const char *reason         = NULL;
   struct string_list *members;

   if (!(members = file_archive_get_file_list(path, NULL)))
      return "not a readable zip";
   snprintf(w->member, sizeof(w->member), "%s#meta.json", path);
   file_archive_get_file_crc32_and_size(w->member, &size);
   if (size > VULKAN_CUSTOM_DRIVER_META_MAX)
      reason = "meta.json over 4 KiB";
   else if (!file_archive_compressed_read(w->member, &buf, NULL, &len))
      reason = "no readable meta.json";
   else if (!(reason = vulkan_custom_driver_parse_meta((const char*)buf,
               (size_t)len, g_android->activity->sdkVersion, meta))
         && !(reason = vulkan_custom_driver_check_members(members,
               meta->library_name))
         && !custom_driver_one_member(members, meta->library_name))
      reason = "libraryName matches more than one member";
   string_list_free(members);

   if (!reason)
   {
      snprintf(w->member, sizeof(w->member), "%s#%s", path,
            meta->library_name);
      file_archive_get_file_crc32_and_size(w->member, &size);
      if (size > (uint64_t)VULKAN_CUSTOM_DRIVER_SO_MAX)
         reason = "driver library over 64 MiB";
   }
   if (!reason)
   {
      custom_driver_remove(w, w->tmp);
      fill_pathname_join_special(w->other, w->tmp, "meta.json",
            sizeof(w->other));
      if (     !path_mkdir(w->tmp)
            || !filestream_write_file(w->other, buf, len))
         reason = "cannot write the driver dir";
   }
   free(buf);
   if (reason)
      return reason;

   fill_pathname_join_special(w->other, w->tmp, "source", sizeof(w->other));
   if (!filestream_write_file(w->other, path, (int64_t)strlen(path)))
      reason = "cannot write the driver dir";
   else if (!file_archive_extract_file(w->member, "so", w->tmp,
            w->other, sizeof(w->other)))
      reason = "cannot extract the driver library";
   else
   {
      fill_pathname_join_special(w->other, w->tmp, meta->library_name,
            sizeof(w->other));
      if ((file = filestream_open(w->other, RETRO_VFS_FILE_ACCESS_READ,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      {
         got = filestream_read(file, head, sizeof(head));
         filestream_close(file);
      }
      if (!(reason = vulkan_custom_driver_check_elf(head,
                  got > 0 ? (size_t)got : 0, path_get_size(w->other))))
      {
         custom_driver_remove(w, w->dir);
         if (filestream_rename(w->tmp, w->dir) != 0)
            reason = "cannot write the driver dir";
      }
   }
   if (reason)
      custom_driver_remove(w, w->tmp);
   return reason;
}

/* Loads libadrenotools.so on first use: its linkernsbypass constructor
 * remaps dlopen's code page, so it must not run in every start. */
static bool custom_driver_adrenotools(struct custom_driver_work *w)
{
   Dl_info info;
   dylib_t lib;
   if (!dladdr(&custom_driver_anchor, &info) || !info.dli_fname)
      return false;
   fill_pathname_basedir(w->hook, info.dli_fname, sizeof(w->hook));
   if (custom_driver_st.open_libvulkan)
      return true;
   fill_pathname_join_special(w->file, w->hook, "libadrenotools.so",
         sizeof(w->file));
   if (!(lib = dylib_load(w->file)))
      return false;
   custom_driver_st.open_libvulkan = (custom_driver_open_libvulkan_t)
      dylib_proc(lib, "adrenotools_open_libvulkan");
   return custom_driver_st.open_libvulkan != NULL;
}

/* Adds the "Safe start" launcher shortcut once per process, before the
 * first custom driver is loaded, so a black screen has a way back. */
static void custom_driver_shortcut(void)
{
   JNIEnv *env;
   if (custom_driver_st.shortcut)
      return;
   custom_driver_st.shortcut = true;
   if (g_android->addSafeStartShortcut && (env = jni_thread_getenv()))
      CALL_VOID_METHOD(env, g_android->activity->clazz,
            g_android->addSafeStartShortcut);
}

/* vulkan_custom_driver_load_t: runs on a loader cache miss only. */
static void *custom_driver_load(void *data, const char *path, uint32_t key,
      struct vulkan_custom_driver_meta *meta,
      enum vulkan_custom_driver_msg *msg)
{
   void *handle                 = NULL;
   const char *reason           = NULL;
   struct custom_driver_work *w = (struct custom_driver_work*)
      calloc(1, sizeof(*w));

   if (!w)
   {
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: out of memory.\n",
            path);
      *msg = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
      return NULL;
   }
   if (!custom_driver_st.pruned)
   {
      custom_driver_st.pruned = true;
      custom_driver_prune(w);
   }
   snprintf(w->other, sizeof(w->other), "%08x", (unsigned)key);
   fill_pathname_join_special(w->dir, custom_driver_st.base, w->other,
         sizeof(w->dir));
   strlcpy(w->tmp, w->dir, sizeof(w->tmp));
   strlcat(w->tmp, ".tmp", sizeof(w->tmp));

   if (     !custom_driver_extracted(w, meta)
         && (reason = custom_driver_extract(w, path, meta)))
      *msg = VULKAN_CUSTOM_DRIVER_MSG_INVALID;
   else
   {
      custom_driver_shortcut();
      custom_driver_mark(path, custom_driver_st.core);
      if (!custom_driver_adrenotools(w))
         reason = "libadrenotools.so did not load";
      else
      {
         fill_pathname_join_special(w->file, custom_driver_st.base, "tmp",
               sizeof(w->file));
         fill_pathname_slash(w->file, sizeof(w->file));
         path_mkdir(w->file);
         strlcpy(w->other, w->dir, sizeof(w->other));
         fill_pathname_slash(w->other, sizeof(w->other));
         if (!(handle = custom_driver_st.open_libvulkan(RTLD_NOW,
                     ADRENOTOOLS_DRIVER_CUSTOM, w->file, w->hook, w->other,
                     meta->library_name, NULL, NULL)))
            reason = "adrenotools_open_libvulkan failed";
      }
      if (!handle)
      {
         filestream_delete(custom_driver_st.marker);
         *msg = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
      }
   }
   if (reason)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: %s.\n",
            path, reason);
   free(w);
   return handle;
}

/* Reads the crash state and the marker the last process left, once. */
static bool custom_driver_init(void)
{
   int64_t state_len  = 0;
   int64_t marker_len = 0;
   char *state;
   char *marker;

   if (custom_driver_st.ready)
      return true;
   if (     !g_android
         || !g_android->activity
         || string_is_empty(g_android->activity->internalDataPath))
      return false;
   custom_driver_st.ready = true;
   fill_pathname_join_special(custom_driver_st.base,
         g_android->activity->internalDataPath, "vulkan_drivers",
         sizeof(custom_driver_st.base));
   path_mkdir(custom_driver_st.base);
   fill_pathname_join_special(custom_driver_st.marker, custom_driver_st.base,
         "pending", sizeof(custom_driver_st.marker));
   fill_pathname_join_special(custom_driver_st.state, custom_driver_st.base,
         "state", sizeof(custom_driver_st.state));
   fill_pathname_slash(custom_driver_st.base, sizeof(custom_driver_st.base));

   state  = custom_driver_read(custom_driver_st.state, &state_len);
   marker = custom_driver_read(custom_driver_st.marker, &marker_len);
   /* A failed save keeps the marker for the next start, until a custom
    * driver opens and writes its own. */
   if (     !vulkan_custom_driver_session_init(&custom_driver_st.session,
               custom_driver_load, NULL, state, (size_t)state_len,
               marker, (size_t)marker_len)
         || custom_driver_save_state())
      filestream_delete(custom_driver_st.marker);
   free(state);
   free(marker);
   return true;
}

dylib_t vulkan_custom_driver_open(void)
{
   struct vulkan_custom_driver_request req;
   const struct vulkan_custom_driver_meta *meta = NULL;
   enum vulkan_custom_driver_msg msg            = VULKAN_CUSTOM_DRIVER_MSG_NONE;
   settings_t *settings                         = config_get_ptr();
   int64_t mtime                                = 0;
   void *handle;

   if (!custom_driver_init())
      return NULL;
   if (vulkan_custom_driver_session_started(&custom_driver_st.session))
      filestream_delete(custom_driver_st.marker);
   if (custom_driver_st.safe_mode_msg)
   {
      const char *s = msg_hash_to_str(MSG_VULKAN_CUSTOM_DRIVER_SAFE_MODE);
      custom_driver_st.safe_mode_msg = false;
      runloop_msg_queue_push(s, strlen(s), 2, 360, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   req.path      = settings->paths.path_vulkan_driver;
   req.core      = custom_driver_core();
   req.safe_mode = custom_driver_st.safe_mode;
   req.supported = false;
   req.size      = -1;
   req.mtime     = 0;
   if (!string_is_empty(req.path) && !req.safe_mode)
   {
      req.supported = vulkan_custom_driver_supported();
      req.size      = path_get_size(req.path);
      if (path_get_mtime(req.path, &mtime))
         req.mtime  = mtime;
   }
   custom_driver_st.core = req.core;
   handle = vulkan_custom_driver_session_open(&custom_driver_st.session,
         &req, &meta, &msg);
   custom_driver_msg(msg);
   custom_driver_st.mesa = handle && meta->mesa;
   /* The load callback warns for the INVALID it returns. */
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID && req.size < 0)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: zip missing or not readable.\n",
            req.path);
   if (custom_driver_st.session.full)
      RARCH_LOG("[Vulkan] Custom driver \"%s\" not used: too many drivers tried this session.\n",
            req.path);
   if (handle)
   {
      custom_driver_mark(req.path, req.core);
      RARCH_LOG("[Vulkan] Custom driver: \"%s\" (%s), version \"%s\", core \"%s\".\n",
            meta->name, meta->library_name, meta->version, req.core);
      if (!custom_driver_st.mesa)
         RARCH_WARN("[Vulkan] Custom driver vendor \"%s\" is untested.\n",
               meta->vendor);
   }
   return (dylib_t)handle;
}

void vulkan_custom_driver_failed(void)
{
   const struct vulkan_custom_driver_path *p;
   enum vulkan_custom_driver_msg msg;
   if (!custom_driver_st.ready)
      return;
   p   = custom_driver_st.session.current;
   msg = vulkan_custom_driver_session_failed(&custom_driver_st.session);
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_FAILED)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: failed to start.\n",
            p->path);
   custom_driver_msg(msg);
   filestream_delete(custom_driver_st.marker);
}

void vulkan_custom_driver_started(void)
{
   if (     custom_driver_st.ready
         && vulkan_custom_driver_session_started(&custom_driver_st.session))
      filestream_delete(custom_driver_st.marker);
}

void vulkan_custom_driver_kept(void)
{
   if (custom_driver_st.ready)
      vulkan_custom_driver_session_kept(&custom_driver_st.session);
}

bool vulkan_custom_driver_current(void)
{
   return     custom_driver_st.ready
          &&  vulkan_custom_driver_session_current(&custom_driver_st.session);
}

bool vulkan_custom_driver_mesa(void)
{
   return custom_driver_st.mesa;
}

bool vulkan_custom_driver_supported(void)
{
   return     g_android
          &&  g_android->activity
          &&  g_android->activity->sdkVersion >= 28
          &&  path_is_valid("/dev/kgsl-3d0");
}

bool vulkan_custom_driver_crashed(const char *path)
{
   if (string_is_empty(path) || !custom_driver_init())
      return false;
   return vulkan_custom_driver_crash_find(custom_driver_st.session.state,
         custom_driver_st.session.state_len, path, custom_driver_core());
}

void vulkan_custom_driver_picked(const char *path)
{
   size_t len;
   if (string_is_empty(path) || !custom_driver_init())
      return;
   len = custom_driver_st.session.state_len;
   custom_driver_msg(vulkan_custom_driver_session_picked(
            &custom_driver_st.session, path));
   if (custom_driver_st.session.state_len != len)
      custom_driver_save_state();
}

void vulkan_custom_driver_set_safe_mode(bool safe_mode)
{
   /* Latched: a recreate may re-read an intent without the extra.
    * Re-armed per session: a recreate frees the old session's message. */
   if (!safe_mode && !custom_driver_st.safe_mode)
      return;
   custom_driver_st.safe_mode     = true;
   custom_driver_st.safe_mode_msg = true;
}

bool vulkan_custom_driver_safe_mode(void)
{
   return custom_driver_st.safe_mode;
}
