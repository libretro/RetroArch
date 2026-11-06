/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "vulkan_custom_driver.h"

#include "../../configuration.h"
#include "../../msg_hash.h"
#include "../../runloop.h"
#include "../../verbosity.h"

static struct vulkan_custom_driver_glue custom_driver_st;

struct vulkan_custom_driver_glue *vulkan_custom_driver_glue_get_ptr(void)
{
   return &custom_driver_st;
}

void vulkan_custom_driver_glue_msg(enum vulkan_custom_driver_msg msg)
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
#ifdef ANDROID
         id = MSG_VULKAN_CUSTOM_DRIVER_INVALID;
#else
         id = MSG_VULKAN_CUSTOM_DRIVER_INVALID_ICD;
#endif
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_UNSUPPORTED:
#ifdef ANDROID
         id = MSG_VULKAN_CUSTOM_DRIVER_UNSUPPORTED;
#else
         id = MSG_VULKAN_CUSTOM_DRIVER_UNSUPPORTED_LOADER;
#endif
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_RESTART:
         id = MSG_VULKAN_CUSTOM_DRIVER_RESTART;
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED:
         id = MSG_VULKAN_CUSTOM_DRIVER_XR_UNSUPPORTED;
         break;
      case VULKAN_CUSTOM_DRIVER_MSG_MISSING_FEATURES:
         id = MSG_VULKAN_CUSTOM_DRIVER_MISSING_FEATURES;
         break;
      default:
         return;
   }
   s = msg_hash_to_str(id);
   runloop_msg_queue_push(s, strlen(s), 2, 360, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
}

char *vulkan_custom_driver_glue_read(const char *path, int64_t *len)
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

void vulkan_custom_driver_glue_mark(const char *path, const char *core)
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

/* Reads the crash state and the marker the last process left, once. */
static bool custom_driver_init(void)
{
   int64_t state_len  = 0;
   int64_t marker_len = 0;
   char *state;
   char *marker;

   if (custom_driver_st.ready)
      return true;
   /* state holds the platform's dir until its own path is set */
   if (!vulkan_custom_driver_platform_dir(custom_driver_st.state,
            sizeof(custom_driver_st.state)))
      return false;
   custom_driver_st.ready = true;
   fill_pathname_join_special(custom_driver_st.base, custom_driver_st.state,
         "vulkan_drivers", sizeof(custom_driver_st.base));
   path_mkdir(custom_driver_st.base);
   fill_pathname_join_special(custom_driver_st.marker, custom_driver_st.base,
         "pending", sizeof(custom_driver_st.marker));
   fill_pathname_join_special(custom_driver_st.state, custom_driver_st.base,
         "state", sizeof(custom_driver_st.state));
   fill_pathname_slash(custom_driver_st.base, sizeof(custom_driver_st.base));

   state  = vulkan_custom_driver_glue_read(custom_driver_st.state, &state_len);
   marker = vulkan_custom_driver_glue_read(custom_driver_st.marker,
         &marker_len);
   /* A failed save keeps the marker for the next start, until a custom
    * driver opens and writes its own. */
   if (     !vulkan_custom_driver_session_init(&custom_driver_st.session,
               vulkan_custom_driver_platform_load, NULL,
               state, (size_t)state_len, marker, (size_t)marker_len)
         || custom_driver_save_state())
      filestream_delete(custom_driver_st.marker);
   free(state);
   free(marker);
   return true;
}

void *vulkan_custom_driver_glue_open(struct vulkan_custom_driver_request *req,
      const struct vulkan_custom_driver_meta **meta,
      enum vulkan_custom_driver_msg *msg)
{
   settings_t *settings = config_get_ptr();
   int64_t mtime        = 0;
   void *handle;

   *meta = NULL;
   *msg  = VULKAN_CUSTOM_DRIVER_MSG_NONE;
#ifndef ANDROID
   if (custom_driver_st.safe_mode_msg)
   {
      RARCH_LOG("[Vulkan] Started with the system driver (safe start).\n");
      /* Nothing was skipped: no notice, nor a late one once a driver
       * is picked. */
      if (string_is_empty(settings->paths.path_vulkan_driver))
         custom_driver_st.safe_mode_msg = false;
   }
   /* Nothing next to the config until a driver is set; a crash marker
    * left there waits until then. */
   if (     !custom_driver_st.ready
         && string_is_empty(settings->paths.path_vulkan_driver))
      return NULL;
#endif
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

   req->path      = settings->paths.path_vulkan_driver;
   req->core      = custom_driver_core();
   req->safe_mode = custom_driver_st.safe_mode;
   req->supported = false;
   req->size      = -1;
   req->mtime     = 0;
   if (!string_is_empty(req->path) && !req->safe_mode)
   {
      req->supported = vulkan_custom_driver_supported();
      req->size      = path_get_size(req->path);
      if (path_get_mtime(req->path, &mtime))
         req->mtime  = mtime;
   }
   custom_driver_st.core = req->core;
   handle = vulkan_custom_driver_session_open(&custom_driver_st.session,
         req, meta, msg);
   vulkan_custom_driver_glue_msg(*msg);
   if (custom_driver_st.session.full)
      RARCH_LOG("[Vulkan] Custom driver \"%s\" not used: too many drivers tried this session.\n",
            req->path);
   if (handle)
      vulkan_custom_driver_glue_mark(req->path, req->core);
   return handle;
}

bool vulkan_custom_driver_glue_failed(void)
{
   const struct vulkan_custom_driver_path *p;
   enum vulkan_custom_driver_msg msg;
   if (!custom_driver_st.ready)
      return false;
   p   = custom_driver_st.session.current;
   msg = vulkan_custom_driver_session_failed(&custom_driver_st.session);
   if (msg == VULKAN_CUSTOM_DRIVER_MSG_FAILED)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: failed to start.\n",
            p->path);
   else if (msg == VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: the headset runtime cannot use it.\n",
            p->path);
   else if (msg == VULKAN_CUSTOM_DRIVER_MSG_MISSING_FEATURES)
      RARCH_WARN("[Vulkan] Custom driver \"%s\" not used: it lacks features RetroArch needs.\n",
            p->path);
   vulkan_custom_driver_glue_msg(msg);
   filestream_delete(custom_driver_st.marker);
   return p != NULL;
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
#ifndef ANDROID
   /* The notice is for a driver set at start, not one picked since. */
   if (custom_driver_st.safe_mode_msg)
   {
      RARCH_LOG("[Vulkan] Started with the system driver (safe start).\n");
      custom_driver_st.safe_mode_msg = false;
   }
#endif
   if (string_is_empty(path) || !custom_driver_init())
      return;
   len = custom_driver_st.session.state_len;
   vulkan_custom_driver_glue_msg(vulkan_custom_driver_session_picked(
            &custom_driver_st.session, path));
   if (custom_driver_st.session.state_len != len)
      custom_driver_save_state();
   vulkan_custom_driver_platform_picked(path);
}

bool vulkan_custom_driver_safe_mode(void)
{
   return custom_driver_st.safe_mode;
}
