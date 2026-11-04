/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>

#include <boolean.h>
#include <compat/strl.h>
#include <dynamic/dylib.h>
#include <file/archive_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <queues/task_queue.h>
#include <retro_atomic.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "vulkan_custom_driver.h"

#include "../../frontend/drivers/platform_unix.h"
#include "../../verbosity.h"

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
   char prep[PATH_MAX_LENGTH];   /* the zip a pick's task was pushed for */
   bool mesa;                    /* of the last driver open() returned */
   bool pruned;
   bool shortcut;
} custom_driver_android_st;

/* dladdr target: a data address needs no function-to-object cast. */
static const char custom_driver_anchor = 0;

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
   if (!(zip = vulkan_custom_driver_glue_read(w->file, &len)))
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

/* Pick tasks running; prune leaves their .prep dirs alone */
static retro_atomic_int_t custom_driver_preps;

/* Once per process, before any driver is mapped: drops each extraction
 * whose zip is gone or changed, and any half-done one. No recursion. */
static void custom_driver_prune(struct custom_driver_work *w)
{
   const char *base  = vulkan_custom_driver_glue_get_ptr()->base;
   bool preps        = retro_atomic_load_acquire_int(&custom_driver_preps) > 0;
   struct RDIR *rdir = retro_opendir(base);
   if (!rdir)
      return;
   while (retro_readdir(rdir))
   {
      const char *name = retro_dirent_get_name(rdir);
      size_t len       = strlen(name);
      if (     !retro_dirent_is_dir(rdir, NULL)
            || !(     len == 8
                  || (len == 12 && string_is_equal(name + 8, ".tmp"))
                  || (len == 13 && string_is_equal(name + 8, ".prep")
                     && !preps))
            || strspn(name, "0123456789abcdef") != 8)
         continue;
      fill_pathname_join_special(w->dir, base, name, sizeof(w->dir));
      if (len == 8 && custom_driver_current_dir(w, name))
         continue;
      custom_driver_remove(w, w->dir);
   }
   retro_closedir(rdir);
}

/* True when w->dir holds a finished extraction; fills *meta. */
static bool custom_driver_extracted(struct custom_driver_work *w,
      int api_level, struct vulkan_custom_driver_meta *meta)
{
   int64_t len = 0;
   char *buf;
   bool ok     = false;
   fill_pathname_join_special(w->file, w->dir, "meta.json", sizeof(w->file));
   if (!(buf = vulkan_custom_driver_glue_read(w->file, &len)))
      return false;
   if (!vulkan_custom_driver_parse_meta(buf, (size_t)len, api_level, meta))
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

/* Checks the package at path and extracts it into w->dir, through
 * w->tmp. Returns NULL, or the reason it is not usable. Any thread: the
 * pick's task and a context init may extract one version at once, each
 * through a tmp of its own; the second to finish keeps the first's. */
static const char *custom_driver_extract(struct custom_driver_work *w,
      const char *path, int api_level,
      struct vulkan_custom_driver_meta *meta)
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
               (size_t)len, api_level, meta))
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
         struct vulkan_custom_driver_meta done;
         if (custom_driver_extracted(w, api_level, &done))
            custom_driver_remove(w, w->tmp);
         else
         {
            custom_driver_remove(w, w->dir);
            if (     filestream_rename(w->tmp, w->dir) != 0
                  && !custom_driver_extracted(w, api_level, &done))
               reason = "cannot write the driver dir";
         }
      }
   }
   if (reason)
      custom_driver_remove(w, w->tmp);
   return reason;
}

/* nativeLibraryDir/, where libvulkan_driver_hook.so is */
static bool custom_driver_hook_dir(struct custom_driver_work *w)
{
   Dl_info info;
   if (!dladdr(&custom_driver_anchor, &info) || !info.dli_fname)
      return false;
   fill_pathname_basedir(w->hook, info.dli_fname, sizeof(w->hook));
   return true;
}

/* Adds the "Safe start" launcher shortcut once per process, before the
 * first custom driver is loaded, so a black screen has a way back. */
static void custom_driver_shortcut(void)
{
   JNIEnv *env;
   if (custom_driver_android_st.shortcut)
      return;
   custom_driver_android_st.shortcut = true;
   if (g_android->addSafeStartShortcut && (env = jni_thread_getenv()))
      CALL_VOID_METHOD(env, g_android->activity->clazz,
            g_android->addSafeStartShortcut);
}

/* vulkan_custom_driver_load_t: runs on a loader cache miss only. */
void *vulkan_custom_driver_platform_load(void *data, const char *path,
      uint32_t key, struct vulkan_custom_driver_meta *meta,
      enum vulkan_custom_driver_msg *msg)
{
   struct vulkan_custom_driver_glue *glue =
      vulkan_custom_driver_glue_get_ptr();
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
   if (!custom_driver_android_st.pruned)
   {
      custom_driver_android_st.pruned = true;
      custom_driver_prune(w);
   }
   snprintf(w->other, sizeof(w->other), "%08x", (unsigned)key);
   fill_pathname_join_special(w->dir, glue->base, w->other,
         sizeof(w->dir));
   strlcpy(w->tmp, w->dir, sizeof(w->tmp));
   strlcat(w->tmp, ".tmp", sizeof(w->tmp));

   if (     !custom_driver_extracted(w, g_android->activity->sdkVersion,
               meta)
         && (reason = custom_driver_extract(w, path,
               g_android->activity->sdkVersion, meta)))
      *msg = VULKAN_CUSTOM_DRIVER_MSG_INVALID;
   else
   {
      custom_driver_shortcut();
      vulkan_custom_driver_glue_mark(path, glue->core);
      if (!custom_driver_hook_dir(w))
         reason = "the library directory is unknown";
      else
      {
         fill_pathname_join_special(w->file, glue->base, "tmp",
               sizeof(w->file));
         fill_pathname_slash(w->file, sizeof(w->file));
         path_mkdir(w->file);
         strlcpy(w->other, w->dir, sizeof(w->other));
         fill_pathname_slash(w->other, sizeof(w->other));
         handle = vulkan_custom_driver_linker_open(w->hook, w->other,
               meta->library_name, w->file, RTLD_NOW, &reason);
      }
      if (!handle)
      {
         filestream_delete(glue->marker);
         *msg = VULKAN_CUSTOM_DRIVER_MSG_FAILED;
      }
   }
   if (reason)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: %s.\n",
            path, reason);
   free(w);
   return handle;
}

bool vulkan_custom_driver_platform_dir(char *s, size_t len)
{
   if (     !g_android
         || !g_android->activity
         || string_is_empty(g_android->activity->internalDataPath))
      return false;
   strlcpy(s, g_android->activity->internalDataPath, len);
   return true;
}

dylib_t vulkan_custom_driver_open(void)
{
   struct vulkan_custom_driver_request req;
   const struct vulkan_custom_driver_meta *meta;
   enum vulkan_custom_driver_msg msg;
   void *handle = vulkan_custom_driver_glue_open(&req, &meta, &msg);

   custom_driver_android_st.mesa = handle && meta->mesa;
   /* The load callback warns for the INVALID it returns. */
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_INVALID && req.size < 0)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: zip missing or not readable.\n",
            req.path);
   if (handle)
   {
      RARCH_LOG("[Vulkan] Custom driver: \"%s\" (%s), version \"%s\", core \"%s\".\n",
            meta->name, meta->library_name, meta->version, req.core);
      if (!custom_driver_android_st.mesa)
         RARCH_WARN("[Vulkan] Custom driver vendor \"%s\" is untested.\n",
               meta->vendor);
   }
   return (dylib_t)handle;
}

void vulkan_custom_driver_failed(void)
{
   vulkan_custom_driver_glue_failed();
}

bool vulkan_custom_driver_mesa(void)
{
   return custom_driver_android_st.mesa;
}

/* 0 until asked, then 1 or 2. Asked once: the menu asks each time it
 * builds the Video settings, and the context on every init. */
static retro_atomic_int_t custom_driver_supported_st;

bool vulkan_custom_driver_supported(void)
{
   int known = retro_atomic_load_acquire_int(&custom_driver_supported_st);
   if (!known)
   {
      known = (     g_android
                &&  g_android->activity
                &&  g_android->activity->sdkVersion >= 28
                &&  path_is_valid("/dev/kgsl-3d0")) ? 1 : 2;
      retro_atomic_store_release_int(&custom_driver_supported_st, known);
   }
   return known == 1;
}

/* A pick's task: the package checked and extracted ahead of the
 * context init that loads it, which then finds it done. Its state is
 * all it reads. */
struct custom_driver_prep
{
   struct custom_driver_work w;
   struct vulkan_custom_driver_meta meta;
   char path[PATH_MAX_LENGTH];
   char base[PATH_MAX_LENGTH];
   int api_level;
};

static void custom_driver_prep_handler(retro_task_t *task)
{
   struct custom_driver_prep *p = (struct custom_driver_prep*)task->state;
   int64_t mtime                = 0;
   int64_t size;

   if (     !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
         && (size = path_get_size(p->path)) >= 0
         && path_get_mtime(p->path, &mtime))
   {
      const char *reason;
      snprintf(p->w.other, sizeof(p->w.other), "%08x",
            (unsigned)vulkan_custom_driver_key(p->path, size, mtime));
      fill_pathname_join_special(p->w.dir, p->base, p->w.other,
            sizeof(p->w.dir));
      strlcpy(p->w.tmp, p->w.dir, sizeof(p->w.tmp));
      strlcat(p->w.tmp, ".prep", sizeof(p->w.tmp));
      if (     !custom_driver_extracted(&p->w, p->api_level, &p->meta)
            && (reason = custom_driver_extract(&p->w, p->path,
                  p->api_level, &p->meta)))
         RARCH_WARN("[Vulkan] Custom driver \"%s\" not usable: %s.\n",
               p->path, reason);
   }
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void custom_driver_prep_free(retro_task_t *task)
{
   free(task->state);
   retro_atomic_fetch_add_int(&custom_driver_preps, -1);
}

/* Main thread, at a pick: extracts path ahead of its first start. */
static void custom_driver_prepare(const char *path)
{
   struct vulkan_custom_driver_glue *glue =
      vulkan_custom_driver_glue_get_ptr();
   struct custom_driver_prep *p;
   retro_task_t *task;

   /* One task per zip at a time: two write the same .prep */
   if (     glue->safe_mode
         || !vulkan_custom_driver_supported()
         || (     retro_atomic_load_acquire_int(&custom_driver_preps) > 0
               && string_is_equal(custom_driver_android_st.prep, path))
         || !(p = (struct custom_driver_prep*)calloc(1, sizeof(*p))))
      return;
   if (!(task = task_init()))
   {
      free(p);
      return;
   }
   strlcpy(custom_driver_android_st.prep, path,
         sizeof(custom_driver_android_st.prep));
   strlcpy(p->path, path, sizeof(p->path));
   strlcpy(p->base, glue->base, sizeof(p->base));
   p->api_level    = g_android->activity->sdkVersion;
   task->handler   = custom_driver_prep_handler;
   task->state     = p;
   task->cleanup   = custom_driver_prep_free;
   retro_atomic_fetch_add_int(&custom_driver_preps, 1);
   task_queue_push(task);
}

void vulkan_custom_driver_platform_picked(const char *path)
{
   custom_driver_prepare(path);
}

void vulkan_custom_driver_set_safe_mode(bool safe_mode)
{
   struct vulkan_custom_driver_glue *glue =
      vulkan_custom_driver_glue_get_ptr();
   /* Latched: a recreate may re-read an intent without the extra.
    * Re-armed per session: a recreate frees the old session's message. */
   if (!safe_mode && !glue->safe_mode)
      return;
   glue->safe_mode     = true;
   glue->safe_mode_msg = true;
}
