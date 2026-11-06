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
#ifdef HAVE_VULKAN_CUSTOM_DRIVER
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

/* Desktop: an ICD manifest, the loader's .json for one driver. */
struct vulkan_custom_driver_icd
{
   char library_path[PATH_MAX_LENGTH];             /* as the .json has it */
   char api_version[VULKAN_CUSTOM_DRIVER_TEXT_MAX];
};

/* Desktop: checks an ICD manifest, at most 4 KiB. NULL when it passes,
 * else the reason, for the log. */
const char *vulkan_custom_driver_parse_icd(const char *buf, size_t len,
      struct vulkan_custom_driver_icd *icd);
/* Desktop: the library to open for library_path, from the manifest at
 * json_path: a relative path is the manifest's directory's. NULL when it
 * passes, else the reason; a bare file name means the system's library. */
const char *vulkan_custom_driver_icd_library(const char *json_path,
      const char *library_path, char *out, size_t size);
/* Desktop menu: where a .json came from, short enough for one line. Its
 * install prefix when it sits in share/vulkan/icd.d, else its directory;
 * the last two components after "...", or all of it when shorter. */
size_t vulkan_custom_driver_icd_folder(const char *json_path,
      char *out, size_t size);

/* A new zip or .json version gets a new key. On Android it names the
 * extraction dir, as "%08x". */
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
   VULKAN_CUSTOM_DRIVER_MSG_RESTART,
   VULKAN_CUSTOM_DRIVER_MSG_XR_UNSUPPORTED,  /* desktop FAILED: the headset runtime */
   VULKAN_CUSTOM_DRIVER_MSG_MISSING_FEATURES /* desktop FAILED: extensions, interface */
};

/* Crash state: one "<core>\t<zip or .json path>\n" line per driver that
 * took the process down in init with that core; the menu's core is "".
 * The crash marker is one such line. A NULL core is the menu's. */
bool vulkan_custom_driver_crash_find(const char *state, size_t len,
      const char *path, const char *core);
/* Returns the new length. The oldest lines go when cap is reached. */
size_t vulkan_custom_driver_crash_add(char *state, size_t len, size_t cap,
      const char *path, const char *core);
/* Drops path's lines for every core. Returns the new length. */
size_t vulkan_custom_driver_crash_clear(char *state, size_t len,
      const char *path);

/* Loads the driver at path, on Android into the extraction dir named by
 * key. Returns its handle with *meta filled in, or NULL with *msg set to
 * VULKAN_CUSTOM_DRIVER_MSG_INVALID (bad package) or _FAILED. */
typedef void *(*vulkan_custom_driver_load_t)(void *data, const char *path,
      uint32_t key, struct vulkan_custom_driver_meta *meta,
      enum vulkan_custom_driver_msg *msg);

struct vulkan_custom_driver_request
{
   int64_t size;      /* of the zip or .json; < 0 when it is missing */
   int64_t mtime;
   const char *path;  /* the setting; empty: the system driver */
   const char *core;  /* the running core; "" or NULL: the menu */
   bool supported;    /* vulkan_custom_driver_supported() */
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
   bool kept;         /* this init took a kept instance */
   bool restart_shown; /* RESTART shown for a system init on a custom library */
   enum vulkan_custom_driver_msg reason; /* this init's, for session_failed() */
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
/* Desktop: the message session_failed() returns in place of FAILED for
 * the custom driver this init opened, while it starts. The first one
 * stands; open() clears it. */
void vulkan_custom_driver_session_reason(vulkan_custom_driver_session_t *s,
      enum vulkan_custom_driver_msg msg);
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

/* Desktop: the module a new device's vkQueueSubmit is in. */
enum vulkan_custom_driver_module
{
   VULKAN_CUSTOM_DRIVER_MODULE_UNKNOWN = 0, /* a layer in front, or none */
   VULKAN_CUSTOM_DRIVER_MODULE_SYSTEM,      /* a driver not opened as custom */
   VULKAN_CUSTOM_DRIVER_MODULE_CURRENT,     /* the custom driver this init opened */
   VULKAN_CUSTOM_DRIVER_MODULE_OTHER        /* a custom driver still mapped */
};

/* Desktop: the custom driver opened last failed and its library is
 * closed: its slot is free again. False when no slot holds handle. */
bool vulkan_custom_driver_session_drop(vulkan_custom_driver_session_t *s,
      const void *handle);
/* Desktop: judges a new device by module. False when the custom driver
 * this init opened did not take effect: the init fails over to the
 * system driver. *msg is RESTART, once a session, when a system init runs
 * on a custom driver's library. A kept device is not judged. */
bool vulkan_custom_driver_session_device(vulkan_custom_driver_session_t *s,
      enum vulkan_custom_driver_module module,
      enum vulkan_custom_driver_msg *msg);

#define VULKAN_CUSTOM_DRIVER_EXT_NAME_MAX 256 /* VK_MAX_EXTENSION_NAME_SIZE */
#define VULKAN_CUSTOM_DRIVER_LOADER_EXTS  4

/* Desktop: an instance whose only driver is the custom one has that
 * driver's instance extensions and the loader's own. names holds count
 * extension names in records of stride bytes (a VkExtensionProperties
 * array); the loader's names not among them are appended, zero-filled,
 * up to cap records. Returns the new count. */
size_t vulkan_custom_driver_loader_exts(char *names, size_t stride,
      size_t count, size_t cap);

/* Overwrites the start of an ELF64 file's DT_SONAME with patch, up to the
 * shorter of the two. False, with elf untouched, when elf is not a
 * little-endian ELF64 with a DT_SONAME inside len. */
bool vulkan_custom_driver_patch_soname(uint8_t *elf, size_t len,
      const char *patch);

#ifdef HAVE_VULKAN_CUSTOM_DRIVER
/* Glue: vulkan_custom_driver_glue.c with vulkan_custom_driver_android.c,
 * or _desktop.c on Linux and Windows. Context init and surface creation
 * call open, kept, failed, started and current; the menu and the
 * frontend call the rest. */
/* Desktop: returns the ICD library, after the loader's global symbols load. */
dylib_t vulkan_custom_driver_open(void);                /* NULL: the system driver */
/* Desktop: closes the ICD library, so instance and device are destroyed first. */
void    vulkan_custom_driver_failed(void);              /* current custom driver failed */
void    vulkan_custom_driver_started(void);             /* first surface up */
void    vulkan_custom_driver_kept(void);                /* a kept instance is used, open skipped */
bool    vulkan_custom_driver_current(void);             /* this init opened a custom driver, not started yet */
bool    vulkan_custom_driver_mesa(void);                /* the driver open() returned is Mesa */
/* Desktop: valid only during Vulkan init. */
bool    vulkan_custom_driver_supported(void);           /* Adreno and API >= 28; desktop: the loader loads drivers directly */
bool    vulkan_custom_driver_crashed(const char *path); /* off for the running core after a crash */
void    vulkan_custom_driver_picked(const char *path);  /* clears path's crash state and INVALID mark */
void    vulkan_custom_driver_set_safe_mode(bool safe_mode); /* true: system driver until the process ends */
bool    vulkan_custom_driver_safe_mode(void);           /* Safe start: system driver for this process */
#ifndef ANDROID
/* Desktop: device creation passes the new device's vkQueueSubmit. False
 * when the custom driver this init opened did not take effect. */
bool    vulkan_custom_driver_device(function_t queue_submit);
/* Desktop: why this init's custom driver is failing; failed() shows it
 * in place of FAILED. */
void    vulkan_custom_driver_reason(enum vulkan_custom_driver_msg msg);
#endif
/* Between the glue files. vulkan_custom_driver_glue.c holds the state
 * both platforms share; its crash state and marker are in
 * <dir>/vulkan_drivers/. */
struct vulkan_custom_driver_glue
{
   vulkan_custom_driver_session_t session;
   char base[PATH_MAX_LENGTH];   /* <dir>/vulkan_drivers/ */
   char marker[PATH_MAX_LENGTH];
   char state[PATH_MAX_LENGTH];
   const char *core;             /* of the open() in progress */
   bool ready;
   bool safe_mode;
   bool safe_mode_msg;
};
struct vulkan_custom_driver_glue *vulkan_custom_driver_glue_get_ptr(void);
void vulkan_custom_driver_glue_msg(enum vulkan_custom_driver_msg msg);
/* A whole small file, NUL-terminated; NULL with *len 0 when missing. */
char *vulkan_custom_driver_glue_read(const char *path, int64_t *len);
/* The crash marker: written before driver code runs, deleted once it is
 * up or failed. */
void vulkan_custom_driver_glue_mark(const char *path, const char *core);
/* open() without the platform's log lines. NULL: the system driver.
 * *req is set unless the platform's dir is not known yet or, on
 * desktop, no driver path is set and none was before. */
void *vulkan_custom_driver_glue_open(struct vulkan_custom_driver_request *req,
      const struct vulkan_custom_driver_meta **meta,
      enum vulkan_custom_driver_msg *msg);
/* failed() without closing a library. True when a custom driver was
 * the current one. */
bool vulkan_custom_driver_glue_failed(void);
/* Each platform's glue: the dir vulkan_drivers/ goes in (false while it
 * is not known), its vulkan_custom_driver_load_t, and its part of
 * vulkan_custom_driver_picked(). */
bool vulkan_custom_driver_platform_dir(char *s, size_t len);
void *vulkan_custom_driver_platform_load(void *data, const char *path,
      uint32_t key, struct vulkan_custom_driver_meta *meta,
      enum vulkan_custom_driver_msg *msg);
void vulkan_custom_driver_platform_picked(const char *path);
/* vulkan_custom_driver_linker.c: a private copy of the system Vulkan
 * loader whose HAL is driver_dir/driver_name. hook_dir holds
 * libvulkan_driver_hook.so; below API 29 the copy is written to tmp_dir.
 * Directories end in a slash. NULL, with *reason set, on failure. */
void *vulkan_custom_driver_linker_open(const char *hook_dir,
      const char *driver_dir, const char *driver_name, const char *tmp_dir,
      int flags, const char **reason);
#endif

RETRO_END_DECLS

#endif
