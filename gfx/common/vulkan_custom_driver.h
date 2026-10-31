/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_CUSTOM_DRIVER_H
#define VULKAN_CUSTOM_DRIVER_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#ifdef HAVE_ADRENOTOOLS
#include <dynamic/dylib.h>
#endif

RETRO_BEGIN_DECLS

#define VULKAN_CUSTOM_DRIVER_VENDOR      "Mesa"
#define VULKAN_CUSTOM_DRIVER_META_MAX    4096
#define VULKAN_CUSTOM_DRIVER_SO_MAX      (64L * 1024 * 1024)
#define VULKAN_CUSTOM_DRIVER_ELF_HEAD    20
#define VULKAN_CUSTOM_DRIVER_LIBRARY_MAX 64
#define VULKAN_CUSTOM_DRIVER_TEXT_MAX    128

struct vulkan_custom_driver_meta
{
   char library_name[VULKAN_CUSTOM_DRIVER_LIBRARY_MAX];
   char vendor[VULKAN_CUSTOM_DRIVER_TEXT_MAX];
   char name[VULKAN_CUSTOM_DRIVER_TEXT_MAX];    /* optional */
   char version[VULKAN_CUSTOM_DRIVER_TEXT_MAX]; /* driverVersion, optional */
   bool mesa;                                   /* vendor is Mesa, any case */
};

/* Package checks. Each returns NULL when the package passes, else the
 * reason, for the log. */
const char *vulkan_custom_driver_parse_meta(const char *buf, size_t len,
      int api_level, struct vulkan_custom_driver_meta *meta);
const char *vulkan_custom_driver_check_members(
      const struct string_list *members, const char *library_name);
const char *vulkan_custom_driver_check_elf(const unsigned char *head,
      size_t len, int64_t size);

/* Names the extraction dir, as "%08x". A new zip version gets a new key. */
uint32_t vulkan_custom_driver_key(const char *path,
      int64_t size, int64_t mtime);

#define VULKAN_CUSTOM_DRIVER_SLOTS       4
#define VULKAN_CUSTOM_DRIVER_PATHS       8
#define VULKAN_CUSTOM_DRIVER_STATE_MAX   8192

enum vulkan_custom_driver_msg
{
   VULKAN_CUSTOM_DRIVER_MSG_NONE = 0,
   VULKAN_CUSTOM_DRIVER_MSG_FAILED,
   VULKAN_CUSTOM_DRIVER_MSG_CRASHED,
   VULKAN_CUSTOM_DRIVER_MSG_INVALID,
   VULKAN_CUSTOM_DRIVER_MSG_UNSUPPORTED,
   VULKAN_CUSTOM_DRIVER_MSG_RESTART
};

/* Crash state: one "<core>\t<zip path>\n" line per driver that took the
 * process down in init with that core; the menu's core is "". The crash
 * marker is one such line. A NULL core is the menu's. */
bool vulkan_custom_driver_crash_find(const char *state, size_t len,
      const char *path, const char *core);
/* Returns the new length. The oldest lines go when cap is reached. */
size_t vulkan_custom_driver_crash_add(char *state, size_t len, size_t cap,
      const char *path, const char *core);
/* Drops path's lines for every core. Returns the new length. */
size_t vulkan_custom_driver_crash_clear(char *state, size_t len,
      const char *path);

/* Loads the driver at path into the extraction dir named by key. Returns
 * its loader with *meta filled in, or NULL with *msg set to
 * VULKAN_CUSTOM_DRIVER_MSG_INVALID (bad package) or _FAILED. */
typedef void *(*vulkan_custom_driver_load_t)(void *data, const char *path,
      uint32_t key, struct vulkan_custom_driver_meta *meta,
      enum vulkan_custom_driver_msg *msg);

struct vulkan_custom_driver_request
{
   int64_t size;      /* of the zip; < 0 when it is missing */
   int64_t mtime;
   const char *path;  /* the setting; empty: the system driver */
   const char *core;  /* the running core; "" or NULL: the menu */
   bool supported;    /* Adreno and API >= 28 */
   bool safe_mode;    /* started with the system driver forced */
};

struct vulkan_custom_driver_slot
{
   void *handle;      /* NULL: free */
   uint32_t key;
   struct vulkan_custom_driver_meta meta;
};

struct vulkan_custom_driver_path
{
   unsigned shown;    /* 1 << msg for each message shown */
   bool failed;       /* failed to start: the system driver all session */
   bool invalid;      /* bad package: the system driver until picked again */
   char path[PATH_MAX_LENGTH];
};

typedef struct vulkan_custom_driver_session
{
   vulkan_custom_driver_load_t load;
   void *load_data;
   struct vulkan_custom_driver_path *current;
   size_t state_len;
   struct vulkan_custom_driver_slot slots[VULKAN_CUSTOM_DRIVER_SLOTS];
   unsigned path_count;
   bool pending;      /* a custom driver opened and not started yet */
   bool full;         /* the path opened last found the table or slots full */
   bool full_shown;   /* RESTART shown for a path past a full table */
   struct vulkan_custom_driver_path paths[VULKAN_CUSTOM_DRIVER_PATHS];
   char state[VULKAN_CUSTOM_DRIVER_STATE_MAX];
} vulkan_custom_driver_session_t;

/* Starts a session from the crash state file and the crash marker the
 * last process left (either may be empty). Returns true when the marker
 * named a driver: it is now off for its core and the state must be
 * saved. */
bool vulkan_custom_driver_session_init(vulkan_custom_driver_session_t *s,
      vulkan_custom_driver_load_t load, void *load_data,
      const char *state, size_t state_len,
      const char *marker, size_t marker_len);
/* Picks the loader for one Vulkan context init: a custom one, with *meta
 * set, or NULL for the system one. *msg is the message to show, once per
 * path per session. */
void *vulkan_custom_driver_session_open(vulkan_custom_driver_session_t *s,
      const struct vulkan_custom_driver_request *req,
      const struct vulkan_custom_driver_meta **meta,
      enum vulkan_custom_driver_msg *msg);
/* The custom driver opened last failed to start: the system one is used
 * for its path all session. Returns the message to show. */
enum vulkan_custom_driver_msg vulkan_custom_driver_session_failed(
      vulkan_custom_driver_session_t *s);
/* The driver opened last is up, or no longer starting. Returns true when
 * the crash marker is to be deleted. */
bool vulkan_custom_driver_session_started(vulkan_custom_driver_session_t *s);
/* An init took a kept instance and skipped open: the driver opened last
 * is not this init's to drop. */
void vulkan_custom_driver_session_kept(vulkan_custom_driver_session_t *s);
/* True while the custom driver this init opened has not started. */
bool vulkan_custom_driver_session_current(
      const vulkan_custom_driver_session_t *s);
/* The user picked path again: its crash state goes for every core, and a
 * bad package is tried again. Returns RESTART when path failed to start
 * this session, as it may still be mapped, or has no room in the table. */
enum vulkan_custom_driver_msg vulkan_custom_driver_session_picked(
      vulkan_custom_driver_session_t *s, const char *path);

#ifdef HAVE_ADRENOTOOLS
/* Android glue, vulkan_custom_driver_android.c. Context init and surface
 * creation call open, kept, failed, started and current; the menu and the
 * frontend call the rest. */
dylib_t vulkan_custom_driver_open(void);                /* NULL: use the system loader */
void    vulkan_custom_driver_failed(void);              /* current custom driver failed */
void    vulkan_custom_driver_started(void);             /* first surface up */
void    vulkan_custom_driver_kept(void);                /* a kept instance is used, open skipped */
bool    vulkan_custom_driver_current(void);             /* this init opened a custom driver, not started yet */
bool    vulkan_custom_driver_mesa(void);                /* the driver open() returned is Mesa */
bool    vulkan_custom_driver_supported(void);           /* Adreno and API >= 28 */
bool    vulkan_custom_driver_crashed(const char *path); /* off for the running core after a crash */
void    vulkan_custom_driver_picked(const char *path);  /* clears path's crash state and INVALID mark */
void    vulkan_custom_driver_set_safe_mode(bool safe_mode); /* true: system driver until the process ends */
bool    vulkan_custom_driver_safe_mode(void);           /* Safe start: system driver for this process */
#endif

RETRO_END_DECLS

#endif
