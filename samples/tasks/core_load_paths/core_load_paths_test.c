/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Real loads through the load check: the shipping objects of a build
 * with HAVE_CORE_LOAD_CHECK, the public test key, null drivers, rgui.
 * One mode a run: a startup load happens once a process. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <compat/strl.h>
#include <features/features_cpu.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <time/rtime.h>

#include "../../../command.h"
#include "../../../configuration.h"
#include "../../../content.h"
#include "../../../core_info.h"
#include "../../../core_load_check.h"
#include "../../../core_trust_runtime.h"
#include "../../../frontend/frontend.h"
#include "../../../frontend/frontend_driver.h"
#include "../../../menu/menu_cbs.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_setting.h"
#include "../../../msg_hash.h"
#include "../../../msg_hash_lbl_str.h"
#include "../../../paths.h"
#include "../../../playlist.h"
#include "../../../retroarch.h"
#include "../../../runloop.h"
#include "../../../verbosity.h"
#include "../../../tasks/task_content.h"
#include "../../../tasks/tasks_internal.h"
#ifndef _WIN32
#include <sys/stat.h>
#include "../../../core_signature.h"
#endif
#ifdef HAVE_NETWORKING
#include "../../../network/netplay/netplay.h"
#endif
#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
#include "../../../runahead.h"
#endif

/* menu_cbs_ok.c's; no header declares it */
int action_ok_close_content(const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx);

static unsigned failures;
#define CHECK(cond, what) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
   failures++; } } while (0)

static char dir[256];
static char ext[16];
static char good_core[400];   /* never trusted before the mode trusts it */
static char bad_core[400];    /* another build: never trusted */
static char nc_core[400];     /* needs content: untrusted until the mode trusts it */
static char seeded[400];      /* in the first run's seed folder */
static char broken[400];      /* a core file with no library */
static char cores[340];
static char fw_core[400];     /* good_core's bytes; its info asks for firmware */
static char fwb_core[400];    /* bad_core's bytes, with the same info */
static char sub_core[400];    /* declares subsystem harness_sub_a; no info */
static char fws_core[400];    /* declares harness_sub_b; fw_core's info */
static char game[400];
static char sub_game[400];

static void pump(unsigned n)
{
   while (n--)
   {
      runloop_iterate();
      task_queue_check();
   }
}

static bool warning_up(void)
{
   enum core_warning_kind kind;
   enum core_check_result result;
   return core_warning_pending(&kind, &result) && kind == CORE_WARNING_LOAD;
}

/* The Security Warning's box is up on the menu */
static bool shown(void)
{
   menu_dialog_buttons_t b;
   menu_dialog_get_buttons(menu_state_get_ptr(), &b);
   return (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
      && b.count && !b.classic;
}

/* A choice the box offers, its button pressed as a pad does: a box up
 * for another request takes nothing. True when the answer was taken. */
static bool answer(enum core_warning_choice choice)
{
   enum core_warning_choice choices[CORE_WARNING_CHOICES_MAX];
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_dialog_buttons_t b;
   unsigned frames;
   unsigned n;
   unsigned i;
   /* The box goes up on a menu frame */
   for (frames = 0; frames < 60 && !shown(); frames++)
      pump(1);
   if (!shown())
      return false;
   menu_dialog_get_buttons(menu_st, &b);
   n = core_warning_choices(choices);
   for (i = 0; i < n; i++)
      if (choices[i] == choice)
         break;
   if (i == n || b.count != n)
      return false;
   menu_st->dialog_st.button_highlight = (int)i;
   menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), MENU_ACTION_OK, cpu_features_get_time_usec());
   return !warning_up();
}

/* An error a load put on screen since the last call; drains the queue */
static bool error_queued(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   msg_queue_entry_t entry;
   bool found = false;
   while (msg_queue_extract(&runloop_st->msg_queue, &entry))
      if (entry.category == MESSAGE_QUEUE_CATEGORY_ERROR)
         found = true;
   runloop_st->msg_queue_size = 0;
   return found;
}

/* Drains the queue: whether a load said its core failed to open */
static bool open_failed_queued(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *msg = msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE);
   msg_queue_entry_t entry;
   bool found      = false;
   while (msg_queue_extract(&runloop_st->msg_queue, &entry))
      if (string_is_equal(entry.msg, msg))
         found = true;
   runloop_st->msg_queue_size = 0;
   return found;
}

#ifdef CORE_LOAD_PATHS_WRAP
/* Drains the queue: whether it said a load was refused with no menu to
 * ask on; *open_failed set if it said a core failed to open */
static bool no_menu_refusal_queued(bool *open_failed)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *refused = msg_hash_to_str(MSG_CORE_LOAD_REFUSED_NO_MENU);
   const char *failed  = msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE);
   msg_queue_entry_t entry;
   bool found          = false;
   while (msg_queue_extract(&runloop_st->msg_queue, &entry))
   {
      if (string_is_equal(entry.msg, refused))
         found = true;
      else if (string_is_equal(entry.msg, failed))
         *open_failed = true;
   }
   runloop_st->msg_queue_size = 0;
   return found;
}
#endif

/* Drains the queue: whether it said core isn't trusted (or, seed set,
 * that the seed is still running); *error set for an error message */
static bool refusal_queued(const char *core, bool seed, bool *error)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   msg_queue_entry_t entry;
   char want[NAME_MAX_LENGTH + 128];
   bool found = false;
   if (seed)
      strlcpy(want, msg_hash_to_str(MSG_CORE_LOAD_REFUSED_SEED_PENDING),
            sizeof(want));
   else
      snprintf(want, sizeof(want),
            msg_hash_to_str(MSG_CORE_LOAD_REFUSED_LOAD_CORE),
            path_basename_nocompression(core));
   while (msg_queue_extract(&runloop_st->msg_queue, &entry))
   {
      if (string_is_equal(entry.msg, want))
         found = true;
      else if (entry.category == MESSAGE_QUEUE_CATEGORY_ERROR
            || string_is_equal(entry.msg,
               msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE)))
         *error = true;
   }
   runloop_st->msg_queue_size = 0;
   return found;
}

/* Equal but for the separators: RetroArch keeps a Windows path with
 * backslashes */
static bool same_path(const char *a, const char *b)
{
   if (!a || !b)
      return false;
   for (; *a && *b; a++, b++)
      if (*a != *b && !(PATH_CHAR_IS_SLASH(*a) && PATH_CHAR_IS_SLASH(*b)))
         return false;
   return *a == *b;
}

static bool core_is(enum rarch_core_type type)
{
   return runloop_state_get_ptr()->current_core_type == type
      && !runloop_is_content_switching();
}

static bool is_plain(void) { return core_is(CORE_TYPE_PLAIN); }
static bool is_dummy(void) { return core_is(CORE_TYPE_DUMMY); }

/* The core itself runs: a probe latches the plain type over the dummy
 * core too */
static bool runs(const char *core)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *name            = runloop_st->system.info.library_name;
   return is_plain()
      && runloop_st->lib_handle
      && name && string_is_equal(name, "content_load_harness")
      && same_path(path_get(RARCH_PATH_CORE), core);
}

static bool bad_runs(void)  { return runs(bad_core); }
static bool good_runs(void) { return runs(good_core); }

static bool quitting(void)
{
   return (runloop_state_get_ptr()->flags
         & RUNLOOP_FLAG_SHUTDOWN_INITIATED) != 0;
}

static bool wait_for(bool (*cond)(void), unsigned frames)
{
   while (frames--)
   {
      if (cond())
         return true;
      pump(1);
   }
   return cond();
}

/* The screen comes up within frames, or what is up instead is said */
static void check_shown(unsigned frames, const char *what)
{
   const char *label = NULL;
   if (wait_for(shown, frames))
      return;
   menu_entries_get_last_stack(NULL, &label, NULL, NULL, NULL);
   CHECK(false, what);
   fprintf(stderr, "  menu %s, on top \"%s\"\n",
         (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) ? "up" : "down",
         label ? label : "");
}

/* A load that must not ask: runs to its end with no warning */
static void load_quietly(const char *core)
{
   unsigned i;
   CHECK(task_push_load_contentless_core_from_menu(core),
         "a trusted core's load was not started");
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
   {
      CHECK(!warning_up(), "a trusted core asked");
      pump(1);
   }
   pump(2);
   CHECK(runs(core), "a trusted core did not run");
}

static void write_file(const char *path)
{
   FILE *f = fopen(path, "wb");
   if (!f)
      return;
   fputs("harness content", f);
   fclose(f);
}

/* name, from beside argv0, to dst: a new file in a folder the user can
 * write, so no exemption passes it */
static bool copy_beside(const char *argv0, const char *name,
      const char *dst)
{
   char src[600];
   void *buf         = NULL;
   int64_t len       = 0;
   const char *slash = strrchr(argv0, '/');
   bool ok;
#ifdef _WIN32
   const char *bslash = strrchr(argv0, '\\');
   if (bslash && (!slash || bslash > slash))
      slash = bslash;
#endif
   snprintf(src, sizeof(src), "%.*s/%s", slash ? (int)(slash - argv0) : 1,
         slash ? argv0 : ".", name);
   if (!filestream_read_file(src, &buf, &len))
      return false;
   ok = filestream_write_file(dst, buf, len);
   free(buf);
   return ok;
}

/* -L of a core nothing passes is hashed in place, refused, and
 * falls back to the dummy core and menu, asking there. The answer runs
 * it as a normal startup would have, from the file checked. */
static void mode_startup(enum core_warning_choice choice)
{
   CHECK(wait_for(warning_up, 120), "no Security Warning for the startup core");
   check_shown(60, "the startup core's warning is not shown");
   CHECK(answer(choice), "the startup warning's answer was not taken");
   CHECK(wait_for(bad_runs, 600), "the answer did not run the startup core");
   CHECK(!warning_up(), "the startup warning stayed after its answer");
   CHECK(!shown(), "the startup warning's screen stayed after its answer");
   /* Load Once lasts until another core loads */
   load_quietly(bad_core);
   if (choice != CORE_WARNING_ALWAYS_TRUST)
      return;
   /* After another core's load only the record passes it */
   CHECK(task_push_load_contentless_core_from_menu(good_core), "not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the other core");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(good_runs, 600), "Load Once did not run the other core");
   load_quietly(bad_core);
}

static bool bad_runs_game(void)
{
   return bad_runs()
      && (content_state_get_ptr()->flags & CONTENT_ST_FLAG_IS_INITED)
      && same_path(path_get(RARCH_PATH_CONTENT), game);
}

/* -L of a refused core with content: the answer runs it with that
 * content, as the startup would have */
static void mode_startup_content(void)
{
   CHECK(wait_for(warning_up, 120), "no Security Warning for the startup core");
   check_shown(60, "the startup core's warning is not shown");
   CHECK(answer(CORE_WARNING_LOAD_ONCE),
         "the startup warning's answer was not taken");
   CHECK(wait_for(bad_runs_game, 600),
         "the answer did not run the startup core with its content");
   CHECK(!warning_up(), "the startup warning stayed after its answer");
}

/* Load Core's state: the core named and described, waiting for content,
 * where a startup with --load-menu-on-error lands for a core that needs
 * content (without it, that startup fails) */
static bool nc_loaded(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *name            = runloop_st->system.info.library_name;
   return !runloop_is_content_switching()
      &&  (runloop_st->flags & RUNLOOP_FLAG_HAS_SET_CORE)
      &&  runloop_st->explicit_current_core_type == CORE_TYPE_PLAIN
      &&  same_path(path_get(RARCH_PATH_CORE), nc_core)
      &&  path_is_empty(RARCH_PATH_CONTENT)
      &&  name && string_is_equal(name, "needs_content");
}

static void mode_startup_needs_content(void)
{
   unsigned i;
   bool loads = false;
   CHECK(wait_for(warning_up, 120), "no Security Warning for the startup core");
   error_queued();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   /* As Load Core: the core is named, no load starts. A load can start
    * and fail inside one frame: its error message stays queued. */
   for (i = 0; i < 600 && !nc_loaded(); i++)
   {
      pump(1);
      if (runloop_is_content_switching() || error_queued())
         loads = true;
   }
   CHECK(!loads, "Load Once started a load of a core that needs content");
   if (!nc_loaded())
   {
      const char *name = runloop_state_get_ptr()->system.info.library_name;
      CHECK(false, "Load Once of a core that needs content did not land on the core loaded");
      fprintf(stderr, "  core type %d, switching %d, core \"%s\", library \"%s\"\n",
            (int)runloop_state_get_ptr()->current_core_type,
            (int)runloop_is_content_switching(), path_get(RARCH_PATH_CORE),
            name ? name : "");
   }
   pump(30);
   CHECK(!error_queued(), "Load Once of a core that needs content put an error on screen");
   CHECK(nc_loaded(), "the core loaded did not stay");
   CHECK(!warning_up(), "the warning came back");
   CHECK(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE, "the menu is not up");
   CHECK(!quitting(), "RetroArch quits");
}

/* With no menu to ask on, a startup load the pending seed holds back
 * runs again once the seed has trusted its core */
static void mode_null_seed(void)
{
   unsigned i;
   for (i = 0; i < 1200 && !is_plain() && !quitting(); i++)
      pump(1);
   CHECK(is_plain(), "the startup load did not run again after the seed");
   CHECK(!quitting(), "RetroArch quits");
   CHECK(same_path(path_get(RARCH_PATH_CORE), seeded),
         "another core runs");
}

/* A staged load is refused and asks; the screen outlasts the menu its
 * failed load returns to. Always Trust runs it and asks no more. */
static void staged_trust(const char *core)
{
   CHECK(task_push_load_contentless_core_from_menu(core),
         "the staged load was not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the staged load");
   CHECK(wait_for(is_dummy, 120), "the refused load did not end on the dummy core");
   CHECK(!open_failed_queued(), "the refused load said its core failed to open");
   pump(30);
   check_shown(1, "the warning screen did not outlast the refused load");
   CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   CHECK(wait_for(is_plain, 600), "Always Trust did not run the core");
   CHECK(!shown(), "the staged load's warning screen stayed after its answer");
   CHECK(same_path(path_get(RARCH_PATH_CORE), core),
         "another core runs");
   load_quietly(core);
}

#ifndef _WIN32
/* Moves only the core's ctime, as a chmod to its own mode does */
static bool ctime_move(const char *path)
{
   struct core_identity was, now;
   struct stat st;
   core_file_t *f = core_file_open(path);
   bool moved     = false;
   unsigned i;
   if (!f || !core_file_identity(f, &was))
   {
      core_file_close(f);
      return false;
   }
   /* Retried until a coarse clock ticks */
   for (i = 0; i < 1000 && !moved; i++)
   {
      if (     stat(path, &st) != 0
            || chmod(path, st.st_mode & 07777) != 0
            || !core_file_identity(f, &now))
         break;
      moved = now.ctime != was.ctime || now.ctime_nsec != was.ctime_nsec;
      if (!moved)
         retro_sleep(1);
   }
   core_file_close(f);
   return moved;
}

static bool marked_took;

/* As a lobby join's mark: takes the refused core */
static bool marked_refused(const char *core_path)
{
   marked_took = same_path(core_path, bad_core);
   path_clear(RARCH_PATH_CORE);
   return true;
}

static bool marked_taken(void) { return marked_took; }

/* A mark that lands on a staged load past its preload stage, as a lobby
 * join's does on a load already in flight: its refusal is taken, the
 * snapshot kept for a park dropped */
static void staged_marked_late(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   unsigned i;
   CHECK(task_push_load_contentless_core_from_menu(bad_core),
         "the staged load was not started");
   for (i = 0; i < 600 && (runloop_st->lib_handle
            || !core_load_check_stage_ready()); i++)
      pump(1);
   /* The preload stage keeps its snapshot; the core stage is next */
   pump(1);
   task_content_load_no_park(marked_refused);
   CHECK(wait_for(marked_taken, 600), "the marked load's refusal was not taken");
   pump(30);
   CHECK(!warning_up(), "the marked load parked");
}

/* Unthreaded, the core stage opens a staged pass a frame after its
 * check: a core that moves in that frame is checked again, never a
 * failed load */
static void mode_staged_moved(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   unsigned i;
   staged_trust(good_core);
   /* Not passed from its record: the stage checks it */
   CHECK(ctime_move(good_core), "the core's ctime did not move");
   CHECK(task_push_load_contentless_core_from_menu(good_core),
         "the staged load was not started");
   /* The old session closed and the stage's check settled: the core
    * stage opens it a frame later */
   for (i = 0; i < 600 && (runloop_st->lib_handle
            || !core_load_check_stage_ready()); i++)
      pump(1);
   CHECK(!runloop_st->lib_handle, "the old session did not close");
   CHECK(ctime_move(good_core), "the core's ctime did not move");
   CHECK(wait_for(good_runs, 600), "the core moved before its open did not run");
   CHECK(!warning_up(), "a trusted core asked");
   staged_marked_late();
}

/* Load Once on a startup core with no content that moved since its
 * check: the re-run's open checks it again and asks again */
static void mode_startup_moved(void)
{
   CHECK(wait_for(warning_up, 120), "no Security Warning for the startup core");
   CHECK(ctime_move(nc_core), "the core's ctime did not move");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(warning_up, 600),
         "Load Once on a changed core did not ask again");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(nc_loaded, 600), "the answered startup did not load the core");
   CHECK(!warning_up(), "the warning came back");
}
#endif

/* The check and the open on the task worker: the worker opens only the
 * file the check passed */
static void set_threaded(void)
{
   settings_t *settings = config_get_ptr();
   configuration_set_bool(settings,
         settings->bools.threaded_data_runloop_enable, true);
   task_queue_set_threaded();
}

/* A playlist Run reaches CMD_EVENT_LOAD_CORE with no explicit core
 * type; its refused open parks and asks all the same */
static void mode_playlist(void)
{
   content_ctx_info_t info;
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(bad_core, game,
            "title", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the playlist Run");
   CHECK(wait_for(is_dummy, 120), "the refused Run did not end on the dummy core");
   pump(30);
   check_shown(1, "the warning screen did not outlast the refused Run");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(is_plain, 600), "Load Once did not run the playlist entry");
   CHECK(same_path(path_get(RARCH_PATH_CONTENT), game),
         "the re-run lost its content");
}

/* Its refused core said and not loaded, the action asking nothing */
static void check_refusal_said(bool said, bool error, const char *what)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *name            = runloop_st->system.info.library_name;
   CHECK(said, what);
   CHECK(!error, "a refused core put an error on screen");
   CHECK(!warning_up(), "a refused core's action asked");
   CHECK(     !runloop_st->lib_handle
         && !(name && string_is_equal(name, "content_load_harness")),
         "a refused core loaded");
}

/* Runs a playlist entry of core with subsystem content */
static void playlist_sub_run(const char *core)
{
   menu_file_list_cbs_t cbs;
   playlist_config_t config;
   struct playlist_entry entry;
   union string_list_elem_attr attr;
   struct string_list *roms = string_list_new();
   char lpl[400];
   snprintf(sub_game, sizeof(sub_game), "%s/sub.bin", dir);
   write_file(sub_game);
   snprintf(lpl, sizeof(lpl), "%s/harness.lpl", dir);
   memset(&config, 0, sizeof(config));
   config.capacity       = 16;
   playlist_config_set_path(&config, lpl);
   attr.i                = 0;
   memset(&entry, 0, sizeof(entry));
   entry.path            = sub_game;
   entry.label           = (char*)"sub";
   entry.core_path       = (char*)core;
   entry.core_name       = (char*)"bad";
   entry.subsystem_ident = (char*)"harness_sub";
   entry.subsystem_name  = (char*)"Harness Sub";
   entry.subsystem_roms  = roms;
   CHECK(     roms && string_list_append(roms, sub_game, attr)
         &&   playlist_init_cached(&config)
         &&   playlist_push(playlist_get_cached(), &entry),
         "no playlist entry");
   string_list_free(roms);
   memset(&cbs, 0, sizeof(cbs));
   menu_cbs_init_bind_ok(&cbs, "", "", 0, FILE_TYPE_PLAYLIST_ENTRY, 0, "", 0);
   CHECK(cbs.action_ok != NULL, "no OK action for a playlist entry");
   if (cbs.action_ok)
      cbs.action_ok(sub_game, "", FILE_TYPE_PLAYLIST_ENTRY, 0, 0);
   CHECK(core_load_check_refused(core), "the entry's core was not refused");
}

/* A playlist entry with subsystem content probes its core with no load
 * after it to park: a refused core is said, not loaded */
static void mode_playlist_sub(void)
{
   bool said  = false;
   bool error = false;
   bool loads = false;
   unsigned i;
   refusal_queued(bad_core, false, &error);
   error = false;
   playlist_sub_run(bad_core);
   said = refusal_queued(bad_core, false, &error);
   for (i = 0; i < 60; i++)
   {
      if (runloop_is_content_switching())
         loads = true;
      pump(1);
      if (refusal_queued(bad_core, false, &error))
         said = true;
   }
   CHECK(!loads, "the playlist entry's refused core started a load");
   check_refusal_said(said, error,
         "the playlist entry's refused core was not said");
}

static bool good_runs_game(void)
{
   return good_runs()
      && (content_state_get_ptr()->flags & CONTENT_ST_FLAG_IS_INITED)
      && same_path(path_get(RARCH_PATH_CONTENT), game);
}

#ifdef HAVE_NETWORKING
/* Netplay's reload of a session with subsystem content probes its core
 * with no load after it to park: a refused core is said, not loaded */
static void mode_netplay_sub(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   union string_list_elem_attr attr;
   bool said                   = false;
   bool error                  = false;
   unsigned i;
   snprintf(sub_game, sizeof(sub_game), "%s/sub.bin", dir);
   write_file(sub_game);
   attr.i = 0;
   /* A core runs; netplay reloads a session of bad_core with subsystem
    * content */
   staged_trust(good_core);
   path_set(RARCH_PATH_CORE, bad_core);
   path_set(RARCH_PATH_SUBSYSTEM, "harness_sub");
   if (!runloop_st->subsystem_fullpaths)
      runloop_st->subsystem_fullpaths = string_list_new();
   string_list_append(runloop_st->subsystem_fullpaths, sub_game, attr);
   refusal_queued(bad_core, false, &error);
   error = false;
   CHECK(task_push_netplay_content_reload(NULL),
         "netplay's reload was not started");
   for (i = 0; i < 120; i++)
   {
      pump(1);
      if (refusal_queued(bad_core, false, &error))
         said = true;
   }
   /* The unload's dummy core start ran without it */
   CHECK(is_dummy(), "netplay's unload did not end on the dummy core");
   check_refusal_said(said, error,
         "netplay's refused core was not said");
}

/* A lobby join of a refused core is said, not parked, and leaves no
 * client mode or host behind to connect a later load. With its probe
 * cached the core push opens nothing: the join's load is refused. */
static void netplay_join(void)
{
   bool said  = false;
   bool error = false;
   unsigned i;
   path_set(RARCH_PATH_CORE, bad_core);
   refusal_queued(bad_core, false, &error);
   error = false;
   CHECK(task_push_netplay_content_reload("127.0.0.1|9"),
         "netplay's reload was not started");
   for (i = 0; i < 120; i++)
   {
      pump(1);
      if (refusal_queued(bad_core, false, &error))
         said = true;
   }
   check_refusal_said(said, error, "the join's refused core was not said");
   CHECK(!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL),
         "the refused join left netplay on");
   CHECK(!(networking_state_get_ptr()->flags
            & NET_DRIVER_ST_FLAG_NETPLAY_CLIENT_DEFERRED),
         "the refused join left its host to connect");
}

/* Joins from a core running without content, then with content, then,
 * uncached, from the dummy core: no load follows its push to take the
 * refusal, which a cached probe would leave to the next load */
static void mode_netplay_join(bool cached)
{
   content_ctx_info_t info;
   staged_trust(good_core);
   netplay_join();
   CHECK(is_dummy(), "the refused join did not end on the dummy core");
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_menu(good_core, game,
            &info, CORE_TYPE_PLAIN, NULL, NULL), "the game was not started");
   CHECK(wait_for(good_runs_game, 600), "the trusted core did not run the game");
   netplay_join();
   CHECK(is_dummy(), "the refused join did not end on the dummy core");
   if (!cached)
   {
      netplay_join();
      CHECK(string_is_empty(path_get(RARCH_PATH_CORE)),
            "the refused join left its core set");
   }
}
#endif

/* The pending state as restored, or what the re-run's load set from it
 * (content_set_subsystem_info()) if a later stage cleared it already */
static bool sub_restored(void)
{
   content_state_t *p_content = content_state_get_ptr();
   return ((p_content->flags & CONTENT_ST_FLAG_PENDING_SUBSYSTEM_INIT)
         && string_is_equal(p_content->pending_subsystem_ident, "harness_sub")
         && p_content->pending_subsystem_rom_num == 1
         && p_content->pending_subsystem_roms[0]
         && same_path(p_content->pending_subsystem_roms[0], sub_game))
      || string_is_equal(path_get(RARCH_PATH_SUBSYSTEM), "harness_sub");
}

/* The re-run restores the subsystem state it parked, not the one
 * current then. The harness core loads no subsystem, so the re-run's
 * load fails; what it restored is checked. */
static void mode_subsystem(void)
{
   content_state_t *p_content = content_state_get_ptr();
   content_ctx_info_t info;
   snprintf(sub_game, sizeof(sub_game), "%s/sub.bin", dir);
   write_file(sub_game);
   memset(&info, 0, sizeof(info));
   path_set(RARCH_PATH_CORE, bad_core);
   content_clear_subsystem();
   strlcpy(p_content->pending_subsystem_ident, "harness_sub",
         sizeof(p_content->pending_subsystem_ident));
   p_content->pending_subsystem_id      = 0;
   p_content->pending_subsystem_rom_num = 1;
   p_content->pending_subsystem_rom_id  = 1;
   p_content->pending_subsystem_roms[0] = strdup(sub_game);
   CHECK(task_push_load_subsystem_with_core(NULL, "sub", &info,
            CORE_TYPE_PLAIN, NULL, NULL), "the subsystem load was not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the subsystem load");
   CHECK(wait_for(is_dummy, 120), "the refused load did not end");
   /* Anything in between may change the current state */
   content_clear_subsystem();
   path_clear(RARCH_PATH_SUBSYSTEM);
   CHECK(!sub_restored(), "the subsystem state outlived its clear");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(sub_restored, 600), "the re-run lost the parked subsystem state");
}

/* A probe is quiet; Load Core's park asks, through a tab switch's flush
 * and a Quick Menu push; Cancel drops it. The Core row's own action
 * probes quietly: the menu's OK on that row opens the core browser. */
static void mode_probe(void)
{
   struct retro_system_info info;
   rarch_setting_t *setting = NULL;
   bool no_content          = false;
   bool flush               = true;
   CHECK(!libretro_get_system_info(bad_core, &info, &no_content),
         "a probe opened an unchecked core");
   pump(60);
   CHECK(!warning_up(), "a probe asked");
   CHECK(task_content_park_core_load(bad_core), "Load Core was not parked");
   CHECK(wait_for(warning_up, 120), "no warning for the parked Load Core");
   check_shown(60, "the parked Load Core's warning is not shown");
   menu_entries_flush_stack(MENU_ENUM_LABEL_MAIN_MENU_STR, 0);
   check_shown(60, "the warning did not come back after a menu flush");
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, &flush);
   pump(10);
   check_shown(60, "the warning did not come back after a Quick Menu push");
   CHECK(answer(CORE_WARNING_CANCEL), "Cancel not taken");
   pump(10);
   CHECK(!warning_up(), "the warning stayed after Cancel");
   CHECK(!shown(), "the warning screen stayed after Cancel");
   path_set(RARCH_PATH_CORE, bad_core);
   setting = menu_setting_find_enum(MENU_ENUM_LABEL_CORE_LIST);
   CHECK(setting && setting->actions && setting->actions->ok,
         "no Core row");
   if (setting && setting->actions && setting->actions->ok)
      setting->actions->ok(setting, 0, false);
   pump(60);
   CHECK(!warning_up(), "the Core row's action asked");
   CHECK(core_load_check_refused(bad_core),
         "the Core row's action did not probe the core");
   /* The companion UI's Load Core starts a core that runs without
    * content once answered */
   CHECK(task_content_park_core_start(bad_core), "Load Core was not parked");
   CHECK(wait_for(warning_up, 120), "no warning for the parked Load Core");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(bad_runs, 600), "the answered Load Core did not start the core");
}

static void unload(void)
{
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   CHECK(wait_for(is_dummy, 120), "the core did not unload");
}

/* The companion UI's first Load Core of a core asks: the park can only
 * follow the open. A trusted core's start is pushed without asking;
 * one that needs content is only loaded. */
static void mode_load_core_start(void)
{
   unsigned i;
   bool loads = false;
   CHECK(!task_content_park_core_start(bad_core),
         "Load Core parked a core nothing had opened");
   task_content_load_core_start(bad_core);
   CHECK(wait_for(warning_up, 120), "the first Load Core did not ask");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(bad_runs, 600), "the answered Load Core did not start the core");
   staged_trust(good_core);
   /* Of the running core, the open reads back its flag and the start
    * is pushed, as upstream's were */
   task_content_load_core_start(good_core);
   CHECK(runloop_core_probe_live(), "the open was not of the running core");
   CHECK(runloop_state_get_ptr()->system.load_no_content,
         "Load Core of the running core cleared its flag");
   CHECK(runloop_is_content_switching(),
         "Load Core of the running core did not start it");
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
      pump(1);
   CHECK(!warning_up(), "Load Core of the running core asked");
   /* So the open below is not of the running core */
   unload();
   task_content_load_core_start(good_core);
   /* What the start runs is upstream's: the current core type */
   CHECK(runloop_is_content_switching(),
         "Load Core did not start a trusted core");
   pump(60);
   CHECK(!warning_up(), "Load Core of a trusted core asked");
   task_content_load_core_start(nc_core);
   CHECK(wait_for(warning_up, 120),
         "Load Core of a core that needs content did not ask");
   error_queued();
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   for (i = 0; i < 600 && !nc_loaded(); i++)
   {
      pump(1);
      if (runloop_is_content_switching() || error_queued())
         loads = true;
   }
   pump(30);
   CHECK(!loads && !error_queued(),
         "Load Once started a core that needs content");
   CHECK(nc_loaded(), "Load Once did not load a core that needs content");
}

#ifndef _WIN32
static bool good_probed(void) { return !core_load_check_refused(good_core); }

/* Load Once on a parked Load Core whose core moved since its check:
 * the re-run's open checks it again and asks again. Each variant
 * parks again as itself: the start starts the core, the load only
 * loads it. */
static void mode_load_core_moved(void)
{
   struct retro_system_info info;
   bool no_content = false;
   CHECK(!libretro_get_system_info(bad_core, &info, &no_content),
         "a probe opened an unchecked core");
   pump(60);
   CHECK(task_content_park_core_start(bad_core), "Load Core was not parked");
   CHECK(wait_for(warning_up, 120), "no warning for the parked Load Core");
   CHECK(ctime_move(bad_core), "the core's ctime did not move");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(warning_up, 600),
         "Load Once on a changed core did not ask again");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(bad_runs, 600), "the answered Load Core did not start the core");
   /* While a core runs, runs() can't tell another core's probe from
    * its start */
   unload();
   CHECK(!libretro_get_system_info(good_core, &info, &no_content),
         "a probe opened an unchecked core");
   pump(60);
   CHECK(task_content_park_core_load(good_core), "Load Core was not parked");
   CHECK(wait_for(warning_up, 120), "no warning for the parked Load Core");
   CHECK(ctime_move(good_core), "the core's ctime did not move");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(warning_up, 600),
         "Load Once on a changed core did not ask again");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(good_probed, 600), "the answered Load Core did not load the core");
   pump(60);
   CHECK(!good_runs(), "the answered Load Core started the core");
}
#endif

/* The first run's seed decides, then the startup load runs again */
static void mode_seed(void)
{
   bool failed = false;
   unsigned i;
   for (i = 0; i < 1200 && !is_plain(); i++)
   {
      CHECK(!warning_up(), "the seed's core asked");
      pump(1);
   }
   CHECK(is_plain(), "the startup load did not run again after the seed");
   CHECK(same_path(path_get(RARCH_PATH_CORE), seeded),
         "another core runs");
   /* Passed by the seed, but the loader fails: that is still said */
   open_failed_queued();
   task_push_load_contentless_core_from_menu(broken);
   for (i = 0; i < 600 && !(failed = open_failed_queued()); i++)
      pump(1);
   CHECK(failed, "a core the loader could not open was not said");
   CHECK(!warning_up(), "the seed's broken core asked");
}

/* Before any frame, so the seed is still running: a refusal it holds
 * says so, not that the core isn't trusted */
static void seed_said(void)
{
   bool error = false;
   refusal_queued(seeded, true, &error);
   error = false;
   CHECK(core_trust_runtime_seed_pending(), "the seed is not running");
   playlist_sub_run(seeded);
   CHECK(refusal_queued(seeded, true, &error),
         "the seed's wait was not said as the seed's");
   CHECK(!error, "the seed's wait put an error on screen");
}

/* Close Content's reload of the core library: the dummy session
 * carries the harness core's system info */
static bool reloaded(void)
{
   const char *name = runloop_state_get_ptr()->system.info.library_name;
   return is_dummy() && name && string_is_equal(name, "content_load_harness");
}

/* Load Once lasts through Close Content and a load of that core
 * again, until another core loads */
static void mode_load_once(void)
{
   struct core_load_source src;
   char cache[300];
   unsigned i;
   CHECK(task_push_load_contentless_core_from_menu(bad_core), "not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the staged load");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(is_plain, 600), "Load Once did not run the core");
   CHECK(core_load_check_copy_source(bad_core, &src) == CORE_LOAD_COPY_CHECK,
         "the running core gives run-ahead no source");
   /* A probe cache hit opens nothing: the reload must reach the check */
   snprintf(cache, sizeof(cache), "%s/core_probe.cache", dir);
   runloop_core_probe_cache_free();
   remove(cache);
   /* Close Content as the Quick Menu's does, with Load Core's last core */
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   path_set(RARCH_PATH_CORE_LAST, bad_core);
   menu_state_get_ptr()->flags |= MENU_ST_FLAG_PENDING_CLOSE_CONTENT;
   for (i = 0; i < 600 && !reloaded(); i++)
   {
      CHECK(!warning_up(), "Close Content's reload asked");
      pump(1);
   }
   CHECK(reloaded(), "the reload after Close Content was refused");
   CHECK(core_load_check_copy_source(bad_core, &src) == CORE_LOAD_COPY_NONE,
         "the closed core still counts as running");
   load_quietly(bad_core);
   /* Another core loads: the first one's Load Once ends */
   CHECK(task_push_load_contentless_core_from_menu(good_core), "not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the other core");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(is_plain, 600), "Load Once did not run the other core");
   CHECK(task_push_load_contentless_core_from_menu(bad_core), "not started");
   CHECK(wait_for(warning_up, 600), "Load Once outlived another core's load");
   CHECK(core_warning_choose(CORE_WARNING_CANCEL), "Cancel not taken");
   pump(10);
}

/* Any load but its own re-run ends a waiting park, as Cancel would */
static void mode_park_ends(void)
{
   unsigned i;
   staged_trust(good_core);
   CHECK(task_push_load_contentless_core_from_menu(bad_core), "not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the other core");
   CHECK(wait_for(is_dummy, 120), "the refused load did not end");
   CHECK(task_push_load_contentless_core_from_menu(good_core),
         "the trusted core's load was not started");
   CHECK(wait_for(is_plain, 600), "the trusted core did not run");
   CHECK(!warning_up(), "the parked core's warning outlived another load");
   for (i = 0; i < 60; i++)
   {
      CHECK(!runloop_is_content_switching(), "the ended park ran again");
      pump(1);
   }
   CHECK(same_path(path_get(RARCH_PATH_CORE), good_core),
         "the parked core replaced the session");
}

#ifdef CORE_LOAD_PATHS_WRAP
/* Counted across the frontend's objects: probes of a core's system
 * info, and core info lookups */
static unsigned sysinfo_probes;
static unsigned info_finds;

bool __real_libretro_get_system_info(const char *path,
      struct retro_system_info *sysinfo, bool *load_no_content);
bool __wrap_libretro_get_system_info(const char *path,
      struct retro_system_info *sysinfo, bool *load_no_content)
{
   sysinfo_probes++;
   return __real_libretro_get_system_info(path, sysinfo, load_no_content);
}

bool __real_core_info_find(const char *core_path, core_info_t **core_info);
bool __wrap_core_info_find(const char *core_path, core_info_t **core_info)
{
   info_finds++;
   return __real_core_info_find(core_path, core_info);
}
#endif

static bool fw_box_up(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   return menu_st->dialog_st.confirm_msg == MSG_CORE_FIRMWARE_MISSING
      && menu_st->dialog_st.confirm_cmd == CMD_EVENT_CORE_FIRMWARE_DOWNLOAD;
}

static unsigned fw_boxes;
static bool fw_was_up;

/* Counts the times the firmware box came up */
static void fw_seen(void)
{
   bool up = fw_box_up();
   if (up && !fw_was_up)
      fw_boxes++;
   fw_was_up = up;
}

static void fw_pump(unsigned n)
{
   while (n--)
   {
      pump(1);
      fw_seen();
   }
}

static bool fw_wait_box(unsigned frames)
{
   fw_seen();
   while (!fw_box_up() && frames--)
      fw_pump(1);
   return fw_box_up();
}

/* The firmware box answered as a pad does */
static void fw_answer(enum menu_action action)
{
   fw_seen();
   menu_driver_iterate(menu_state_get_ptr(), disp_get_ptr(), anim_get_ptr(),
         config_get_ptr(), action, cpu_features_get_time_usec());
   fw_seen();
}

static size_t stack_size(void)
{
   return MENU_LIST_GET_STACK_SIZE(menu_state_get_ptr()->entries.list, 0);
}

/* The firmware box pushes no screen: answering it leaves the menu's
 * level. A confirm on its own info screen still pops that screen. */
static void mode_fw_stack(void)
{
   settings_t *settings = config_get_ptr();
   content_ctx_info_t info;
   size_t s;
   unsigned i;
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   generic_action_ok_displaylist_push("", NULL, "", 0, 0, 0,
         ACTION_OK_DL_CORE_SETTINGS_LIST);
   pump(3);
   s = stack_size();
   CHECK(s >= 2, "the settings list did not open");
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, game,
            "fw", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(fw_wait_box(600), "no firmware box");
   fw_answer(MENU_ACTION_CANCEL);
   CHECK(stack_size() == s, "Cancel on the firmware box popped a menu level");
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
      fw_pump(1);
   CHECK(!runloop_is_content_switching(), "the cancelled Run did not end");
   CHECK(stack_size() == s, "the cancelled Run's end changed the menu level");

   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, game,
            "fw", &info, NULL, NULL), "the second Run was not started");
   CHECK(fw_wait_box(600), "no firmware box for the second Run");
   fw_answer(MENU_ACTION_OK);
   CHECK(stack_size() == s, "OK on the firmware box popped a menu level");
   CHECK(wait_for(warning_up, 600), "no Security Warning after OK");
   CHECK(answer(CORE_WARNING_CANCEL), "the warning's Cancel not taken");
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
      fw_pump(1);
   CHECK(!runloop_is_content_switching(), "the refused Run did not end");
   CHECK(stack_size() == s, "the refused Run's end changed the menu level");

   /* Close Content's confirm, over the info screen it pushes */
   configuration_set_bool(settings, settings->bools.menu_show_confirm, true);
   configuration_set_bool(settings, settings->bools.confirm_close, true);
   s = stack_size();
   action_ok_close_content(NULL, NULL, 0, 0, 0);
   CHECK(stack_size() == s + 1, "Close Content's confirm pushed no screen");
   fw_answer(MENU_ACTION_CANCEL);
   CHECK(stack_size() == s, "a confirm on an info screen no longer pops it");
}

/* A contentless core whose probe is refused, after a core that needs
 * content: its firmware is asked for before the warning */
static void mode_fw_contentless(void)
{
   content_ctx_info_t info;
   unsigned i;
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_menu(nc_core, game,
            &info, CORE_TYPE_PLAIN, NULL, NULL), "the game was not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the content core");
   CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   CHECK(wait_for(is_plain, 600), "Always Trust did not run the content core");
   CHECK(!runloop_state_get_ptr()->system.load_no_content,
         "fixture: the content core did not clear load_no_content");

   CHECK(task_push_load_contentless_core_from_menu(fw_core),
         "the contentless load was not started");
   CHECK(fw_wait_box(600) && !warning_up(),
         "no firmware box before the warning for a contentless core");
   fw_answer(MENU_ACTION_CANCEL);
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
      fw_pump(1);
   CHECK(!runloop_is_content_switching(), "the cancelled load did not end");
   CHECK(!warning_up(), "the cancelled load went on to the Security Warning");
}

static bool reset_confirm_shown(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   return (menu_st->flags & MENU_ST_FLAG_ALIVE)
      && menu_st->dialog_st.confirm_msg == MSG_PRESS_AGAIN_TO_RESET;
}

/* A confirm set while the menu was closed: a staged load waiting
 * behind it opens the menu, then shows its firmware box */
static void mode_fw_hidden_confirm(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   content_ctx_info_t info;
   char other[300];
   dylib_t handle;
   unsigned i;
   staged_trust(good_core);
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game,
            "title", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(wait_for(good_runs_game, 600), "the playlist Run did not run");
   if (menu_st->flags & MENU_ST_FLAG_ALIVE)
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   CHECK(!(menu_st->flags & MENU_ST_FLAG_ALIVE), "the menu did not close");

   menu_dialog_confirm_set(menu_st, MSG_PRESS_AGAIN_TO_RESET, CMD_EVENT_RESET);
   handle = runloop_state_get_ptr()->lib_handle;
   snprintf(other, sizeof(other), "%s/other.bin", dir);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_companion_ui(fw_core,
            other, NULL, NULL, NULL, &info, NULL, NULL),
         "the companion load was not started");
   CHECK(wait_for(reset_confirm_shown, 5),
         "the load waits behind a confirm the menu does not show");
   /* Open it ourselves if the stage did not */
   if (!(menu_st->flags & MENU_ST_FLAG_ALIVE))
      command_event(CMD_EVENT_MENU_TOGGLE, NULL);
   fw_answer(MENU_ACTION_CANCEL);
   CHECK(fw_wait_box(60), "no firmware box after the confirm's answer");
   fw_answer(MENU_ACTION_CANCEL);
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
      fw_pump(1);
   CHECK(!runloop_is_content_switching(), "the cancelled load did not end");
   CHECK(runloop_state_get_ptr()->lib_handle == handle
         && (content_state_get_ptr()->flags & CONTENT_ST_FLAG_IS_INITED),
         "the cancelled load did not keep the running core");
}

/* The firmware box is asked once: the load Always Trust re-runs does
 * not ask again */
static void mode_fw_twice(void)
{
   content_ctx_info_t info;
   unsigned i;
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, game,
            "fw", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(fw_wait_box(600), "no firmware box");
   fw_answer(MENU_ACTION_OK);
   CHECK(wait_for(warning_up, 600), "no Security Warning after OK");
   CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   for (i = 0; i < 600 && !runs(fw_core); i++)
   {
      fw_pump(1);
      if (fw_box_up())
         fw_answer(MENU_ACTION_OK);
   }
   CHECK(runs(fw_core), "Always Trust did not run the firmware core");
   if (fw_boxes != 1)
      fprintf(stderr, "firmware boxes: %u\n", fw_boxes);
   CHECK(fw_boxes == 1, "the firmware box was asked more than once");
}

/* Drains the queue: whether it said harness/fw.bin is missing */
static bool fw_told(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *missing         = msg_hash_to_str(MSG_CORE_FIRMWARE_MISSING);
   msg_queue_entry_t entry;
   bool found                  = false;
   while (msg_queue_extract(&runloop_st->msg_queue, &entry))
      if (     string_starts_with(entry.msg, missing)
            && strstr(entry.msg, "harness/fw.bin"))
         found = true;
   runloop_st->msg_queue_size = 0;
   return found;
}

static bool fw_raised(void)
{
   return menu_state_get_ptr()->dialog_st.confirm_cmd
      == CMD_EVENT_CORE_FIRMWARE_DOWNLOAD;
}

/* A companion load of the seeded core with menu_driver "null": it runs
 * with no box. True when it said the firmware is missing. */
static bool fw_null_load(const char *content)
{
   content_ctx_info_t info;
   bool raised = false;
   bool ran;
   unsigned i;
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_companion_ui(seeded,
            content, NULL, NULL, NULL, &info, NULL, NULL),
         "the companion load was not started");
   ran = runloop_is_content_switching();
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
   {
      pump(1);
      if (fw_raised())
         raised = true;
   }
   CHECK(ran && is_plain() && same_path(path_get(RARCH_PATH_CONTENT), content),
         "the companion load did not run with menu_driver null");
   CHECK(!raised, "a firmware box nothing draws was raised for the companion load");
   return fw_told();
}

/* With menu_driver "null" nothing can ask: a load whose core's
 * firmware is missing runs, and says so */
static void mode_fw_null(void)
{
   settings_t *settings = config_get_ptr();
   char other[300];
   char fw[PATH_MAX_LENGTH];
   bool raised          = false;
   unsigned i;
   for (i = 0; i < 1200 && !is_plain() && !quitting(); i++)
   {
      pump(1);
      if (fw_raised())
         raised = true;
   }
   CHECK(is_plain() && same_path(path_get(RARCH_PATH_CORE), seeded)
         && !quitting(), "the startup load did not run with menu_driver null");
   CHECK(!raised, "a firmware box nothing draws was raised");
   CHECK(fw_told(), "missing firmware was not told");

   snprintf(other, sizeof(other), "%s/other.bin", dir);
   write_file(other);
   CHECK(fw_null_load(other),
         "missing firmware was not told for the companion load");

   /* The file is there, or the notification is off: nothing to tell.
    * Each load runs other content than the one before it. */
   snprintf(fw, sizeof(fw), "%s/system/harness", dir);
   path_mkdir(fw);
   strlcat(fw, "/fw.bin", sizeof(fw));
   write_file(fw);
   CHECK(!fw_null_load(game), "firmware on disk was told missing");
   remove(fw);
   configuration_set_bool(settings,
         settings->bools.core_firmware_notification_enable, false);
   CHECK(!fw_null_load(other),
         "missing firmware was told with its notification off");
}

/* What a firmware Cancel leaves as it was */
struct fw_session
{
   char core[PATH_MAX_LENGTH];
   char core_last[PATH_MAX_LENGTH];
   char content[PATH_MAX_LENGTH];
   char basename[PATH_MAX_LENGTH];
   char label[PATH_MAX_LENGTH];
   char db[PATH_MAX_LENGTH];
   char crc[32];
   char info[PATH_MAX_LENGTH];
   char title[PATH_MAX_LENGTH];
   enum rarch_core_type type;
   enum rarch_core_type explicit_type;
   bool has_set_core;
   bool core_running;
   unsigned subsystems;
   dylib_t handle;
};

static struct fw_session fw_ran;
static struct fw_session fw_now;

static void fw_record(struct fw_session *s)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   content_state_t *p_content  = content_state_get_ptr();
   core_info_t *info           = NULL;
   strlcpy(s->core, path_get(RARCH_PATH_CORE), sizeof(s->core));
   strlcpy(s->core_last, path_get(RARCH_PATH_CORE_LAST),
         sizeof(s->core_last));
   strlcpy(s->content, path_get(RARCH_PATH_CONTENT), sizeof(s->content));
   strlcpy(s->basename, path_get(RARCH_PATH_BASENAME), sizeof(s->basename));
   strlcpy(s->label, runloop_st->name.label, sizeof(s->label));
   strlcpy(s->db, p_content->companion_ui_db_name, sizeof(s->db));
   strlcpy(s->crc, p_content->companion_ui_crc32, sizeof(s->crc));
   strlcpy(s->info,
         (core_info_get_current_core(&info) && info && info->path)
         ? info->path : "", sizeof(s->info));
   menu_entries_get_core_title(s->title, sizeof(s->title));
   s->type          = runloop_st->current_core_type;
   s->explicit_type = runloop_st->explicit_current_core_type;
   s->has_set_core  = (runloop_st->flags & RUNLOOP_FLAG_HAS_SET_CORE) ? true : false;
   s->core_running  = (runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING) ? true : false;
   s->subsystems    = runloop_st->subsystem_current_count;
   s->handle        = runloop_st->lib_handle;
}

/* 1 when name's value changed, printed when say is set */
static unsigned fw_differs(bool say, const char *name, const char *was,
      const char *now)
{
   if (string_is_equal(was, now))
      return 0;
   if (say)
      fprintf(stderr, "  %s: \"%s\", was \"%s\"\n", name, now, was);
   return 1;
}

/* How much of the session fw_ran recorded fw_now no longer has, the
 * running core and the menu up included */
static unsigned fw_changes(bool say)
{
   unsigned n = 0;
   n += fw_differs(say, "core", fw_ran.core, fw_now.core);
   n += fw_differs(say, "core_last", fw_ran.core_last, fw_now.core_last);
   n += fw_differs(say, "content", fw_ran.content, fw_now.content);
   n += fw_differs(say, "basename", fw_ran.basename, fw_now.basename);
   n += fw_differs(say, "label", fw_ran.label, fw_now.label);
   n += fw_differs(say, "db", fw_ran.db, fw_now.db);
   n += fw_differs(say, "crc", fw_ran.crc, fw_now.crc);
   n += fw_differs(say, "core info", fw_ran.info, fw_now.info);
   n += fw_differs(say, "menu title", fw_ran.title, fw_now.title);
   if (fw_now.type != fw_ran.type)
   {
      if (say)
         fprintf(stderr, "  core type: %d, was %d\n", (int)fw_now.type,
               (int)fw_ran.type);
      n++;
   }
   if (     fw_now.explicit_type != fw_ran.explicit_type
         || fw_now.has_set_core  != fw_ran.has_set_core)
   {
      if (say)
         fprintf(stderr, "  explicit core type: %d (set %d), was %d (set %d)\n",
               (int)fw_now.explicit_type, (int)fw_now.has_set_core,
               (int)fw_ran.explicit_type, (int)fw_ran.has_set_core);
      n++;
   }
   if (fw_now.subsystems != fw_ran.subsystems)
   {
      if (say)
         fprintf(stderr, "  subsystem count: %u, was %u\n",
               fw_now.subsystems, fw_ran.subsystems);
      n++;
   }
   if (     fw_now.handle != fw_ran.handle
         || !(content_state_get_ptr()->flags & CONTENT_ST_FLAG_IS_INITED))
   {
      if (say)
         fprintf(stderr, "  the running core was closed\n");
      n++;
   }
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
   {
      if (say)
         fprintf(stderr, "  the menu is down\n");
      n++;
   }
   return n;
}

#ifdef HAVE_COMMAND
static void fw_cmd_reply(struct command_handler *cmd, const char *data,
      size_t len)
{
   (void)cmd;
   (void)data;
   (void)len;
}
#endif

/* The load just started asks for its firmware; Cancel ends it */
static bool fw_cancelled(void)
{
   unsigned i;
   if (!fw_wait_box(600))
      return false;
#ifdef CORE_LOAD_PATHS_WRAP
   /* Waiting on its answer looks nothing up */
   {
      unsigned finds = info_finds;
      fw_pump(30);
      CHECK(fw_box_up() && info_finds == finds,
            "the firmware stage looked its core up while its box waited");
   }
#endif
   fw_answer(MENU_ACTION_CANCEL);
   for (i = 0; i < 600 && (task_content_load_pending()
            || runloop_is_content_switching()); i++)
      fw_pump(1);
   return !task_content_load_pending();
}

/* Probes of a core's system info fw_check_kept's Cancel made */
static unsigned fw_restore_probes;

/* That Cancel leaves the session fw_ran recorded as it was. With no
 * subsystems to put back the restore opens no core. */
static void fw_check_kept(const char *what)
{
   bool cancelled;
#ifdef CORE_LOAD_PATHS_WRAP
   unsigned probes   = sysinfo_probes;
#endif
   cancelled         = fw_cancelled();
#ifdef CORE_LOAD_PATHS_WRAP
   fw_restore_probes = sysinfo_probes - probes;
   CHECK(fw_ran.subsystems || !fw_restore_probes,
         "a Cancel probed a core with no subsystems to put back");
#endif
   fw_record(&fw_now);
   if (cancelled && !fw_changes(false))
      return;
   CHECK(false, what);
   if (!cancelled)
      fprintf(stderr, "  no firmware box, or its Cancel did not end the load\n");
   fw_changes(true);
}

static bool fw_runs_game(void)
{
   return runs(fw_core) && same_path(path_get(RARCH_PATH_CONTENT), game);
}

/* fw_core's Run on the dummy session, cancelled at its box, leaves the
 * dummy session and the core the menu names as they were */
static void fw_check_dummy_kept(const char *what)
{
   content_ctx_info_t info;
   unsigned i;
   for (i = 0; i < 600 && task_content_load_pending(); i++)
      fw_pump(1);
   fw_record(&fw_ran);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, game,
            "fw", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(fw_cancelled(), "no firmware box, or its Cancel did not end the Run");
   fw_record(&fw_now);
   CHECK(   is_dummy()
         && string_is_empty(path_get(RARCH_PATH_CONTENT))
         && string_is_equal(fw_now.title, fw_ran.title)
         && string_is_equal(fw_now.info, fw_ran.info), what);
   fw_differs(true, "menu title", fw_ran.title, fw_now.title);
   fw_differs(true, "core info", fw_ran.info, fw_now.info);
}

/* core runs content, through its firmware box and its Security Warning */
static void fw_run_trusting(const char *core, const char *content,
      const char *label)
{
   content_ctx_info_t info;
   unsigned i;
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(core, content,
            label, &info, NULL, NULL), "the Run was not started");
   for (i = 0; i < 1200 && !(runs(core)
            && same_path(path_get(RARCH_PATH_CONTENT), content)); i++)
   {
      fw_pump(1);
      if (fw_box_up())
         fw_answer(MENU_ACTION_OK);
      if (warning_up())
         CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   }
   CHECK(runs(core) && same_path(path_get(RARCH_PATH_CONTENT), content),
         "the Run did not run");
}

/* The session's one subsystem, as the menu reads it, is ident: a
 * running core's own, else the probe's */
static bool fw_sub_ident_is(const char *ident)
{
   runloop_state_t *runloop_st             = runloop_state_get_ptr();
   const struct retro_subsystem_info *subs = runloop_st->system.subsystem.data
      ? runloop_st->system.subsystem.data : runloop_st->subsystem_data;
   return runloop_st->subsystem_current_count == 1
      && subs[0].ident
      && string_is_equal(subs[0].ident, ident);
}

/* core's Run, cancelled at its box, leaves the session fw_ran
 * recorded, the running core or the dummy session, and its subsystem;
 * the restore probes a core only if reprobe */
static void fw_check_sub_cancel(const char *core, bool reprobe,
      const char *what)
{
   content_ctx_info_t info;
   bool cancelled;
#ifdef CORE_LOAD_PATHS_WRAP
   unsigned probes;
#endif
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(core, game,
            "fw", &info, NULL, NULL), "the playlist Run was not started");
#ifdef CORE_LOAD_PATHS_WRAP
   /* A Run whose core is checked on its way in (every Run on Windows)
    * has its probe held, so no tables were written over to read again */
   probes    = sysinfo_probes;
   reprobe   = reprobe
      && runloop_state_get_ptr()->subsystem_current_count != 0;
#endif
   cancelled = fw_cancelled();
   fw_record(&fw_now);
   CHECK(   cancelled
         && fw_now.handle     == fw_ran.handle
         && fw_now.type       == fw_ran.type
         && fw_now.subsystems == fw_ran.subsystems
         && string_is_equal(fw_now.core,  fw_ran.core)
         && string_is_equal(fw_now.title, fw_ran.title)
         && string_is_equal(fw_now.info,  fw_ran.info)
         && fw_sub_ident_is("harness_sub_a"), what);
#ifdef CORE_LOAD_PATHS_WRAP
   probes = sysinfo_probes - probes;
   CHECK(reprobe ? probes != 0 : probes == 0, what);
#else
   (void)reprobe;
#endif
}
#ifdef HAVE_COMMAND
/* LOAD_CONTENT probes its core before the load saves the session: its
 * firmware Cancel, on the No Core session a Security Warning Cancel
 * left, names no core */
static void fw_load_content_no_core(const char *other)
{
   content_ctx_info_t info;
   command_t cmd;
   char arg[sizeof(fw_core) + sizeof(game) + 1];
   memset(&info, 0, sizeof(info));
   memset(&cmd, 0, sizeof(cmd));
   cmd.replier = fw_cmd_reply;
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game,
            "first", &info, NULL, NULL), "the first Run was not started again");
   CHECK(wait_for(good_runs_game, 600), "the first Run did not run again");
   snprintf(arg, sizeof(arg), "%s|%s", bad_core, other);
   CHECK(command_load_content(&cmd, arg), "LOAD_CONTENT was not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for LOAD_CONTENT");
   CHECK(answer(CORE_WARNING_CANCEL), "Cancel not taken");
   fw_pump(30);
   snprintf(arg, sizeof(arg), "%s|%s", fw_core, game);
   CHECK(command_load_content(&cmd, arg), "LOAD_CONTENT was not started");
   CHECK(fw_cancelled(), "no firmware box for LOAD_CONTENT, or its Cancel did not end it");
   fw_record(&fw_now);
   CHECK(   is_dummy() && !*fw_now.core
         && string_ends_with(fw_now.title,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)),
         "a firmware Cancel through LOAD_CONTENT named a core");
   fw_differs(true, "core", "", fw_now.core);
   if (!string_ends_with(fw_now.title, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
      fprintf(stderr, "  menu title: \"%s\"\n", fw_now.title);
}
#endif

/* A firmware box's Cancel leaves the session as it was: the game that
 * runs, or the dummy session */
static void mode_fw_cancel_restores(void)
{
   content_ctx_info_t info;
   char other[300];
   char fw[PATH_MAX_LENGTH];
   unsigned boxes;
   unsigned i;
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   snprintf(other, sizeof(other), "%s/other.bin", dir);
   write_file(game);
   write_file(other);

   /* On the dummy session no core path comes back to probe */
   fw_check_dummy_kept(
         "a Cancel on the dummy session left the cancelled Run's session");

   /* fw_core is trusted with it: the store matches by hash */
   staged_trust(good_core);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game,
            "first", &info, NULL, NULL), "the first Run was not started");
   CHECK(wait_for(good_runs_game, 600), "the first Run did not run");
   fw_record(&fw_ran);

   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, other,
            "other", &info, NULL, NULL), "the playlist Run was not started");
   fw_check_kept("a cancelled playlist Run changed the session");
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_menu(fw_core, other,
            &info, CORE_TYPE_PLAIN, NULL, NULL),
         "the load with a core was not started");
   fw_check_kept("a cancelled load with a core changed the session");
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_companion_ui(fw_core,
            other, "other", "db", "crc", &info, NULL, NULL),
         "the companion load was not started");
   fw_check_kept("a cancelled companion load changed the session");
   CHECK(task_push_load_contentless_core_from_menu(fw_core),
         "the contentless load was not started");
   fw_check_kept("a cancelled contentless load changed the session");

   /* A request while the box is up is turned away */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, other,
            "other", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(fw_wait_box(600), "no firmware box");
   CHECK(!task_push_load_contentless_core_from_menu(fw_core),
         "a load started while another waited on its firmware box");
   fw_check_kept("a turned away request outlived the Cancel");
   /* A core with no firmware too: had it reached the paths, the load
    * would pass its box */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, other,
            "other", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(fw_wait_box(600), "no firmware box");
   CHECK(!task_push_load_contentless_core_from_menu(good_core),
         "a load started while another waited on its firmware box");
   fw_check_kept("a turned away request for a firmware-less core outlived the Cancel");
   /* The companion UI's load with a new core, which writes the most */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, other,
            "other", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(fw_wait_box(600), "no firmware box");
   memset(&info, 0, sizeof(info));
   CHECK(!task_push_load_content_with_new_core_from_companion_ui(good_core,
            other, "other", "db", "crc", &info, NULL, NULL),
         "a companion load started while another waited on its firmware box");
   CHECK(same_path(path_get(RARCH_PATH_CORE), fw_core),
         "a turned away companion load named its core");
   fw_check_kept("a turned away companion load outlived the Cancel");

   /* Loads into the running core */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, game,
            "fw", &info, NULL, NULL), "the firmware core's Run was not started");
   CHECK(fw_wait_box(600), "no firmware box for the firmware core's Run");
   fw_answer(MENU_ACTION_OK);
   for (i = 0; i < 600 && !fw_runs_game(); i++)
   {
      fw_pump(1);
      if (warning_up())
         CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   }
   CHECK(fw_runs_game(), "the firmware core did not run");
   fw_record(&fw_ran);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_core(other, &info, CORE_TYPE_PLAIN,
            NULL, NULL), "the load into the running core was not started");
   fw_check_kept("a cancelled load into the running core changed the session");
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_current_core_from_companion_ui(other,
            &info, CORE_TYPE_PLAIN, NULL, NULL),
         "the companion load into the running core was not started");
   fw_check_kept("a cancelled companion load into the running core changed the session");
#ifdef HAVE_COMMAND
   /* START_CORE starts the running core with no content: the game's
    * save names stay its own until a load is past the box */
   {
      command_t cmd;
      memset(&cmd, 0, sizeof(cmd));
      cmd.replier = fw_cmd_reply;
      CHECK(*fw_ran.basename, "the running game names no saves");
      CHECK(command_start_core(&cmd, ""), "START_CORE was not started");
      fw_check_kept("a cancelled START_CORE changed the session");
   }
#endif

   /* The box's OK loads what it asked about, not a request turned away
    * while it waited */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, other,
            "other", &info, NULL, NULL), "the playlist Run was not started");
   CHECK(fw_wait_box(600), "no firmware box");
   CHECK(!task_push_load_contentless_core_from_menu(good_core),
         "a load started while another waited on its firmware box");
   fw_answer(MENU_ACTION_OK);
   for (i = 0; i < 600 && !(runs(fw_core)
            && same_path(path_get(RARCH_PATH_CONTENT), other)); i++)
   {
      fw_pump(1);
      if (warning_up())
         CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   }
   CHECK(runs(fw_core) && same_path(path_get(RARCH_PATH_CONTENT), other),
         "the box's OK loaded a request turned away while it waited");

   /* A core that ran is probed by its Run before the box asks */
   unload();
   fw_check_dummy_kept(
         "a trusted core's cancelled Run left it in the dummy session");
#ifdef HAVE_COMMAND
   fw_load_content_no_core(other);
#endif

   /* Past its box a load closes the old game: a re-run that asks again
    * and is cancelled ends on the dummy session */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game,
            "first", &info, NULL, NULL), "the first Run was not started again");
   CHECK(wait_for(good_runs_game, 600), "the first Run did not run again");
   snprintf(fw, sizeof(fw), "%s/system/harness", dir);
   path_mkdir(fw);
   strlcat(fw, "/fw.bin", sizeof(fw));
   write_file(fw);
   boxes = fw_boxes;
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fwb_core, other,
            "other", &info, NULL, NULL), "the untrusted Run was not started");
   for (i = 0; i < 600 && !warning_up(); i++)
      fw_pump(1);
   CHECK(warning_up() && fw_boxes == boxes,
         "no Security Warning, or a firmware box, for the untrusted Run");
   remove(fw);
   CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   CHECK(fw_cancelled(), "the re-run did not ask for the firmware that went away");
   CHECK(is_dummy() && !same_path(path_get(RARCH_PATH_CONTENT), game),
         "a cancelled re-run left a phantom session or restored the closed game");

   /* Subsystems. A running core's own declare them; the restore opens
    * no core for it */
   fw_run_trusting(fws_core, other, "fws");
   fw_run_trusting(sub_core, game, "sub");
   /* Checked on its way in, so no probe read its tables: the running
    * core's own declaration lists them */
   CHECK(fw_sub_ident_is("harness_sub_a"),
         "a checked subsystem core's subsystems are not listed");
   /* Trusted now, each Run's probe opens its core and reads its tables */
   fw_run_trusting(fws_core, other, "fws");
   fw_run_trusting(sub_core, game, "sub");
   fw_record(&fw_ran);
   CHECK(fw_sub_ident_is("harness_sub_a"),
         "the running subsystem core's subsystems are not its own");
   fw_check_sub_cancel(fws_core, false,
         "a cancelled Run of another subsystem core over a running one");
   fw_check_sub_cancel(fw_core, false,
         "a cancelled Run of a core with no subsystems over a running one");

   /* A core Load Core named, not started: its tables are the probe's,
    * opened again only when another core's probe wrote over them */
   unload();
   path_set(RARCH_PATH_CORE, sub_core);
   CHECK(command_event(CMD_EVENT_LOAD_CORE, NULL), "Load Core's probe failed");
   fw_record(&fw_ran);
   CHECK(is_dummy() && fw_sub_ident_is("harness_sub_a"),
         "the named subsystem core's probe left no subsystems");
   fw_check_sub_cancel(fws_core, true,
         "a cancelled Run of another subsystem core over a named one");
   fw_check_sub_cancel(fw_core, false,
         "a cancelled Run of a core with no subsystems over a named one");
}

/* A firmware Cancel ends the launch a Load Once was given for: the
 * next launch asks again */
static void mode_fw_load_once(void)
{
   content_ctx_info_t info;
   unsigned i;
   fw_record(&fw_ran);
   CHECK(wait_for(warning_up, 120), "no Security Warning for the startup core");
   CHECK(answer(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(fw_wait_box(600), "no firmware box after Load Once");
   CHECK(fw_boxes == 1, "Load Once's launch asked for its firmware twice");
   CHECK(fw_cancelled(), "the firmware box's Cancel did not end the launch");
   /* Nothing of the refused startup's core stays named */
   CHECK(is_dummy(), "a cancelled launch left a phantom session");
   CHECK(!warning_up() && !error_queued(),
         "the cancelled launch's core asked or failed on its way back");
   fw_record(&fw_now);
   CHECK(   string_ends_with(fw_now.title,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE))
         && !*fw_now.core && !*fw_now.info,
         "the dummy session names the cancelled core");
   fw_differs(true, "menu title", fw_ran.title, fw_now.title);

   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_companion_ui(fw_core,
            game, "fw", "db", "crc", &info, NULL, NULL),
         "the relaunch was not started");
   CHECK(fw_wait_box(600), "no firmware box for the relaunch");
   fw_answer(MENU_ACTION_OK);
   CHECK(wait_for(warning_up, 600), "Load Once outlived a cancelled launch");
   CHECK(answer(CORE_WARNING_CANCEL), "the relaunch's Cancel not taken");
   for (i = 0; i < 600 && (task_content_load_pending()
            || runloop_is_content_switching()); i++)
      fw_pump(1);
   error_queued();

   /* Refused now: the Cancel puts the dummy session back without
    * opening that core again */
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_with_new_core_from_companion_ui(fw_core,
            game, "fw", "db", "crc", &info, NULL, NULL),
         "the refused core's launch was not started");
   CHECK(fw_wait_box(600) && core_load_check_refused(fw_core),
         "no firmware box, or its core not refused");
   CHECK(fw_cancelled(), "the refused core's Cancel did not end the launch");
   CHECK(is_dummy() && !warning_up() && !error_queued(),
         "the refused core's Cancel left a session, asked or failed");
}

#ifdef HAVE_NETWORKGAMEPAD
static bool pad_runs(void) { return core_is(CORE_TYPE_NETRETROPAD); }

/* Drains the queue: whether it said core is being checked */
static bool checking_queued(const char *core)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   msg_queue_entry_t entry;
   char want[NAME_MAX_LENGTH + 64];
   bool found = false;
   snprintf(want, sizeof(want), "%s%s", msg_hash_to_str(MSG_CHECKING_CORE),
         path_basename_nocompression(core));
   while (msg_queue_extract(&runloop_st->msg_queue, &entry))
      if (string_is_equal(entry.msg, want))
         found = true;
   runloop_st->msg_queue_size = 0;
   return found;
}

static void start_pad(void)
{
   content_ctx_info_t info;
   info.argc        = 0;
   info.argv        = NULL;
   info.args        = NULL;
   info.environ_get = NULL;
   CHECK(task_push_start_builtin_core(&info, CORE_TYPE_NETRETROPAD,
            NULL, NULL), "the built-in core's start was not pushed");
}

/* A built-in core opens no library, so its load checks none; it ends a
 * waiting park and Load Once as another core's load does */
static void mode_builtin(void)
{
   unsigned i;
   CHECK(task_push_load_contentless_core_from_menu(bad_core), "not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the core");
   CHECK(wait_for(is_dummy, 120), "the refused load did not end");
   start_pad();
   CHECK(wait_for(pad_runs, 600), "the built-in core did not run");
   CHECK(!warning_up(), "the parked core's warning outlived a built-in core's load");
   if (warning_up())
      core_warning_choose(CORE_WARNING_CANCEL);
   for (i = 0; i < 60; i++)
   {
      CHECK(!runloop_is_content_switching(), "the ended park ran again");
      pump(1);
   }
   CHECK(pad_runs(), "the parked core replaced the built-in core");
   /* Load Core's core, never checked, is still named */
   path_set(RARCH_PATH_CORE, nc_core);
   checking_queued(nc_core);
   start_pad();
   for (i = 0; i < 600 && runloop_is_content_switching(); i++)
   {
      CHECK(!checking_queued(nc_core),
            "a built-in core's load checked another core");
      pump(1);
   }
   CHECK(wait_for(pad_runs, 60), "the built-in core did not run again");
   CHECK(task_push_load_contentless_core_from_menu(bad_core), "not started");
   CHECK(wait_for(warning_up, 600), "no Security Warning for the core");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(bad_runs, 600), "Load Once did not run the core");
   start_pad();
   CHECK(wait_for(pad_runs, 600), "the built-in core did not run");
   CHECK(task_push_load_contentless_core_from_menu(bad_core), "not started");
   CHECK(wait_for(warning_up, 600), "Load Once outlived a built-in core's load");
   CHECK(core_warning_choose(CORE_WARNING_CANCEL), "Cancel not taken");
   pump(10);
}

/* The first press starts what was asked for, with no warning on the way */
static void builtin_first_press(const char *what)
{
   unsigned i;
   start_pad();
   for (i = 0; i < 600 && !pad_runs(); i++)
   {
      CHECK(!warning_up(), what);
      pump(1);
   }
   CHECK(pad_runs(), what);
   unload();
}

static void mode_builtin_first_press(void)
{
   content_ctx_info_t info;
   unsigned i;
   memset(&info, 0, sizeof(info));

   CHECK(!task_push_load_new_core(bad_core, NULL, &info, CORE_TYPE_PLAIN,
            NULL, NULL), "the untrusted Load Core opened");
   CHECK(task_content_park_core_load(bad_core), "Load Core was not parked");
   CHECK(wait_for(warning_up, 120), "no Security Warning");
   CHECK(answer(CORE_WARNING_CANCEL), "Cancel not taken");
   pump(10);
   builtin_first_press("the first press after a cancelled Load Core");

   snprintf(broken, sizeof(broken), "%s/broken_libretro.%s", cores, ext);
   write_file(broken);
   CHECK(!task_push_load_new_core(broken, NULL, &info, CORE_TYPE_PLAIN,
            NULL, NULL), "a file that is no core opened");
   CHECK(task_content_park_core_load(broken), "Load Core was not parked");
   CHECK(wait_for(warning_up, 120), "no Security Warning");
   CHECK(answer(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   for (i = 0; i < 600 && !open_failed_queued(); i++)
      pump(1);
   builtin_first_press("the first press after a failed Load Core");

   staged_trust(good_core);
   unload();
   CHECK(task_push_load_new_core(good_core, NULL, &info, CORE_TYPE_PLAIN,
            NULL, NULL), "the trusted Load Core failed");
   /* Not unload(): the latched plain type never reads as dummy here */
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   pump(10);
   builtin_first_press("the first press after Unload Core");
}

/* A built-in core's start runs no core RARCH_PATH_CORE names: no
 * firmware box for that core */
static void mode_fw_builtin(void)
{
   content_ctx_info_t info;
   char library[NAME_MAX_LENGTH];
   path_set(RARCH_PATH_CORE, fw_core);
   start_pad();
   CHECK(!fw_wait_box(600),
         "a built-in core's start asked for the named core's firmware");
   if (fw_box_up())
      fw_answer(MENU_ACTION_OK);
   CHECK(wait_for(pad_runs, 600), "the built-in core did not run");

   /* A Cancel over the built-in core keeps its identity: a trusted core
    * Load Core named before it started is not probed in its place */
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   staged_trust(good_core);
   unload();
   /* Load Core names it, then the built-in core is started */
   path_set(RARCH_PATH_CORE, good_core);
   CHECK(command_event(CMD_EVENT_LOAD_CORE, NULL), "Load Core's probe failed");
   start_pad();
   CHECK(wait_for(pad_runs, 600), "the built-in core did not run again");
   CHECK(same_path(path_get(RARCH_PATH_CORE), good_core),
         "the built-in core's start left another core named");
   fw_record(&fw_ran);
   strlcpy(library, runloop_state_get_ptr()->system.info.library_name
         ? runloop_state_get_ptr()->system.info.library_name : "",
         sizeof(library));
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(fw_core, game,
            "fw", &info, NULL, NULL), "the playlist Run was not started");
   fw_check_kept("a cancelled Run over the built-in core changed the session");
   CHECK(string_is_equal(library,
            runloop_state_get_ptr()->system.info.library_name
            ? runloop_state_get_ptr()->system.info.library_name : ""),
         "a cancelled Run over the built-in core renamed it");
   CHECK(pad_runs(), "the built-in core stopped running");
}
#endif

static void warn_cancel(const char *what)
{
   CHECK(wait_for(warning_up, 600), what);
   CHECK(answer(CORE_WARNING_CANCEL), "Cancel not taken");
   pump(30);
}

/* The dummy session, naming no core */
static void warn_check_no_core(const char *what)
{
   static struct fw_session s;
   fw_record(&s);
   CHECK(   is_dummy() && !*s.core && !*s.core_last && !*s.content
         && !*s.basename
         && !*s.info && !s.has_set_core && !s.core_running
         && string_ends_with(s.title,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)), what);
   fw_differs(true, "core", "", s.core);
   fw_differs(true, "basename", "", s.basename);
   fw_differs(true, "core info", "", s.info);
   if (!string_ends_with(s.title, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
      fprintf(stderr, "  menu title: \"%s\"\n", s.title);
}

/* The chosen core fw_ran recorded, as it was */
static void warn_check_kept(const char *what)
{
   fw_record(&fw_now);
   CHECK(   string_is_equal(fw_now.core, fw_ran.core)
         && string_is_equal(fw_now.core_last, fw_ran.core_last)
         && string_is_equal(fw_now.content, fw_ran.content)
         && string_is_equal(fw_now.basename, fw_ran.basename)
         && string_is_equal(fw_now.title, fw_ran.title)
         && string_is_equal(fw_now.info, fw_ran.info)
         && fw_now.type == fw_ran.type
         && fw_now.explicit_type == fw_ran.explicit_type
         && fw_now.has_set_core == fw_ran.has_set_core, what);
   fw_differs(true, "core", fw_ran.core, fw_now.core);
   fw_differs(true, "core_last", fw_ran.core_last, fw_now.core_last);
   fw_differs(true, "basename", fw_ran.basename, fw_now.basename);
   fw_differs(true, "menu title", fw_ran.title, fw_now.title);
   fw_differs(true, "core info", fw_ran.info, fw_now.info);
}

static bool warn_lc(const char *core, const char *path)
{
   content_ctx_info_t info;
   memset(&info, 0, sizeof(info));
   return task_push_load_content_with_new_core_from_menu(core, path, &info,
         CORE_TYPE_PLAIN, NULL, NULL);
}

static bool warn_ln(const char *core)
{
   content_ctx_info_t info;
   memset(&info, 0, sizeof(info));
   return task_push_load_new_core(core, NULL, &info, CORE_TYPE_PLAIN,
         NULL, NULL);
}

/* companion_core_load_core()'s body: the companions' objects are not
 * in this build */
static bool warn_companion_ln(const char *core)
{
   if (warn_ln(core))
      return true;
   task_content_park_core_load(core);
   return false;
}

static void warn_reset(void)
{
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   path_clear(RARCH_PATH_CORE_LAST);
   pump(30);
}

static const char *const warn_probed_names[] = {
   "Load Content", "the companion load", "History"
#ifdef HAVE_COMMAND
   , "LOAD_CONTENT"
#endif
};

/* Entry e's load of path on bad_core after a probe of it, as
 * LOAD_CONTENT makes its own */
static bool warn_probed_load(unsigned e, const char *path)
{
   content_ctx_info_t info;
   struct retro_system_info sysinfo;
   bool no_game = false;
   memset(&info, 0, sizeof(info));
   if (e < 3)
      CHECK(libretro_get_system_info(bad_core, &sysinfo, &no_game),
            "the probe ahead of the load failed");
   switch (e)
   {
      case 0:
         return warn_lc(bad_core, path);
      case 1:
         return task_push_load_content_with_new_core_from_companion_ui(
               bad_core, path, NULL, NULL, NULL, &info, NULL, NULL);
      case 2:
         return task_push_load_content_from_playlist_from_menu(bad_core,
               path, "h", &info, NULL, NULL);
#ifdef HAVE_COMMAND
      case 3:
         {
            command_t cmd;
            char arg[sizeof(bad_core) + 300];
            memset(&cmd, 0, sizeof(cmd));
            cmd.replier = fw_cmd_reply;
            snprintf(arg, sizeof(arg), "%s|%s", bad_core, path);
            return command_load_content(&cmd, arg);
         }
#endif
      default:
         break;
   }
   return false;
}

/* Each way a load reaches the warning, then its Cancel */
static void mode_warn_cancel_restores(void)
{
   content_ctx_info_t info;
   char other[300];
   unsigned i;
   memset(&info, 0, sizeof(info));
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   snprintf(other, sizeof(other), "%s/other.bin", dir);
   write_file(other);
   staged_trust(good_core);
   unload();
   warn_reset();

   /* R1 Load Content */
   CHECK(warn_lc(bad_core, game), "Load Content was not started");
   warn_cancel("no warning for Load Content");
   warn_check_no_core("Load Content's Cancel left the core named");
#ifdef HAVE_NETWORKGAMEPAD
   start_pad();
   CHECK(wait_for(pad_runs, 600), "the pad did not start");
   CHECK(!*path_get(RARCH_PATH_CORE),
         "the pad started under the cancelled core's name");
   unload();
#endif
   warn_reset();

   /* R2 The same load asks again */
   CHECK(warn_lc(bad_core, game), "the second Load Content was not started");
   warn_cancel("the second load did not ask");
   warn_check_no_core("the second Cancel left the core named");
   warn_reset();

   /* R3 History */
   CHECK(task_push_load_content_from_playlist_from_menu(bad_core, game, "h",
            &info, NULL, NULL), "the History load was not started");
   warn_cancel("no warning for History");
   warn_check_no_core("History's Cancel left the core named");
   warn_reset();

   /* R4 Load Core, then Load Content */
   CHECK(warn_ln(bad_core), "Load Core did not answer from the record");
   CHECK(task_push_load_content_with_core(game, &info, CORE_TYPE_PLAIN,
            NULL, NULL), "Load Content was not started");
   warn_cancel("no warning for Load Core then Load Content");
   warn_check_no_core("Load Core then Load Content's Cancel left the core named");
   warn_reset();

   /* R4b As R4, the core named another way than the check writes it */
   {
      char alias[sizeof(cores) + 64];
      snprintf(alias, sizeof(alias), "%s/./bad_libretro.%s", cores, ext);
      /* Answered or refused, it names the core */
      warn_ln(alias);
      CHECK(task_push_load_content_with_core(game, &info, CORE_TYPE_PLAIN,
               NULL, NULL), "Load Content was not started");
      warn_cancel("no warning for the other-named Load Core");
      warn_check_no_core("an other-named Load Core's Cancel left the core named");
      warn_reset();
   }

   /* R5 Start Core */
   CHECK(warn_ln(bad_core), "Load Core did not answer from the record");
   CHECK(task_push_start_current_core(&info), "Start Core was not started");
   warn_cancel("no warning for Start Core");
   warn_check_no_core("Start Core's Cancel left the core named");
   warn_reset();

   /* R6 Another core stays chosen. Run first: a volume that can't keep
    * its record passes Load Core only for the core loaded last. */
   load_quietly(good_core);
   unload();
   warn_reset();
   CHECK(warn_ln(good_core), "the trusted Load Core failed");
   fw_record(&fw_ran);
   CHECK(warn_lc(bad_core, game), "Load Content was not started");
   warn_cancel("no warning over the chosen core");
   warn_check_kept("a Cancel lost the core Load Core had chosen");
   warn_reset();

   /* R7 A newer load replaces the park */
   CHECK(task_push_load_contentless_core_from_menu(bad_core),
         "the refused load was not started");
   CHECK(wait_for(warning_up, 600), "no warning for the replaced park");
   CHECK(wait_for(is_dummy, 120), "the refused load did not end");
   CHECK(task_push_load_contentless_core_from_menu(good_core),
         "the newer load was not started");
   CHECK(wait_for(good_runs, 600), "the newer load did not run");
   pump(120);
   CHECK(good_runs(), "a dropped park's session came back");
   warn_reset();

   /* R8 The load closes a game, no core chosen */
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game, "g",
            &info, NULL, NULL), "the game was not started");
   CHECK(wait_for(good_runs_game, 600), "the game did not run");
   CHECK(warn_lc(bad_core, other), "Load Content was not started");
   warn_cancel("no warning over the game");
   warn_check_no_core("a Cancel named the game its load closed");
   warn_reset();

   /* R9 The load closes a game on a manually loaded core */
   CHECK(warn_ln(good_core), "the trusted Load Core failed");
   CHECK(task_push_load_content_with_core(game, &info, CORE_TYPE_PLAIN,
            NULL, NULL), "the game was not started");
   CHECK(wait_for(good_runs_game, 600), "the game did not run");
   CHECK(warn_lc(bad_core, other), "Load Content was not started");
   warn_cancel("no warning over the manual core's game");
   pump(60);
   fw_record(&fw_now);
   CHECK(   is_dummy() && same_path(fw_now.core, good_core) && !*fw_now.content
         && !*fw_now.basename && !fw_now.core_running
         && !string_ends_with(fw_now.title,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)),
         "a Cancel did not bring the manually loaded core back");
   warn_reset();

   /* R10 Load Once runs what was asked for */
   CHECK(warn_lc(bad_core, game), "Load Content was not started");
   CHECK(wait_for(warning_up, 600), "no warning for Load Once");
   CHECK(answer(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   CHECK(wait_for(bad_runs_game, 600), "Load Once did not run the core");
   pump(120);
   CHECK(bad_runs_game(), "a stale restore replaced the session Load Once ran");
   warn_reset();

   /* R12 A core that failed to open is latched over a game the load
    * closes: the title reads no string of the closed library */
   snprintf(broken, sizeof(broken), "%s/broken_libretro.%s", cores, ext);
   write_file(broken);
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game, "g",
            &info, NULL, NULL), "the game was not started");
   CHECK(wait_for(good_runs_game, 600), "the game did not run");
   CHECK(!warn_ln(broken), "a file that is no core opened");
   CHECK(task_content_park_core_load(broken), "Load Core was not parked");
   CHECK(wait_for(warning_up, 600), "no warning for the broken core");
   CHECK(answer(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   for (i = 0; i < 600 && !open_failed_queued(); i++)
      pump(1);
   CHECK(warn_lc(bad_core, other), "Load Content was not started");
   warn_cancel("no warning over the failed core's game");
   fw_record(&fw_now);
   CHECK(   !*fw_now.content && !*fw_now.basename && !*fw_now.label
         && !*fw_now.info && !fw_now.core_running
         && string_ends_with(fw_now.title,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)),
         "a Cancel named the game closed behind a failed core");
   fw_differs(true, "content", "", fw_now.content);
   fw_differs(true, "basename", "", fw_now.basename);
   fw_differs(true, "label", "", fw_now.label);
   fw_differs(true, "core info", "", fw_now.info);
   warn_reset();

   /* R13 A core chosen over a game the load closes stays chosen, the
    * game gone from behind it */
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game, "g",
            &info, NULL, NULL), "the game was not started");
   CHECK(wait_for(good_runs_game, 600), "the game did not run");
   CHECK(warn_ln(good_core), "the trusted Load Core failed");
   CHECK(warn_lc(bad_core, other), "Load Content was not started");
   warn_cancel("no warning over the chosen core's game");
   fw_record(&fw_now);
   CHECK(   same_path(fw_now.core, good_core) && !*fw_now.content
         && !*fw_now.basename && !*fw_now.label && !fw_now.core_running
         && !string_ends_with(fw_now.title,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)),
         "a Cancel named the game closed behind a chosen core");
   fw_differs(true, "content", "", fw_now.content);
   fw_differs(true, "basename", "", fw_now.basename);
   fw_differs(true, "label", "", fw_now.label);
   warn_reset();

   /* R14 A chosen core keeps the subsystem it declared */
   staged_trust(sub_core);
   unload();
   warn_reset();
   CHECK(warn_ln(sub_core), "the subsystem core's Load Core failed");
   fw_record(&fw_ran);
   CHECK(fw_ran.subsystems == 1, "the chosen core declared no subsystem");
   CHECK(warn_lc(bad_core, game), "Load Content was not started");
   warn_cancel("no warning over the subsystem core");
   warn_check_kept("a Cancel lost the chosen subsystem core");
   CHECK(fw_now.subsystems == 1, "a Cancel lost the chosen core's subsystem");
   warn_reset();

   /* R15 A probe of the refused core ahead of each load: the second
    * Cancel, on the No Core session the first left, names no core */
   for (i = 0; i < ARRAY_SIZE(warn_probed_names); i++)
   {
      char what[96];
      unsigned j;
      CHECK(task_push_load_content_from_playlist_from_menu(good_core, game,
               "g", &info, NULL, NULL), "the game was not started");
      CHECK(wait_for(good_runs_game, 600), "the game did not run");
      for (j = 0; j < 2; j++)
      {
         CHECK(warn_probed_load(i, other), "the probed load was not started");
         warn_cancel("no warning for the probed load");
      }
      snprintf(what, sizeof(what), "%s's second Cancel named a core",
            warn_probed_names[i]);
      warn_check_no_core(what);
      warn_reset();
   }

   /* R11 Quit with the warning up: main_exit()'s deinit puts nothing back */
   CHECK(warn_lc(nc_core, game), "Load Content was not started");
   CHECK(wait_for(warning_up, 600), "no warning to quit under");
   fw_record(&fw_ran);
   core_load_check_deinit();
   warn_check_kept("quitting under the warning restored the session");
}

/* A park no load saved for: its Cancel has nothing to put back */
static void warn_check_nothing_kept(const char *what)
{
   path_set(RARCH_PATH_CORE, bad_core);
   CHECK(!command_event(CMD_EVENT_LOAD_CORE, NULL), "the untrusted open succeeded");
   CHECK(task_content_park_core_load(bad_core), "Load Core was not parked");
   warn_cancel("no warning for the park");
   CHECK(same_path(path_get(RARCH_PATH_CORE), bad_core), what);
}

/* No record: LOAD_CORE's refused open leaves the running game alone */
static void mode_warn_cancel_live(void)
{
   content_ctx_info_t info;
   char missing[400];
   memset(&info, 0, sizeof(info));
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   staged_trust(good_core);
   unload();
   CHECK(task_push_load_content_from_playlist_from_menu(good_core, game, "g",
            &info, NULL, NULL), "the game was not started");
   CHECK(wait_for(good_runs_game, 600), "the game did not run");

   /* L1 LOAD_CORE over a game */
   fw_record(&fw_ran);
   CHECK(!warn_ln(bad_core), "the untrusted Load Core opened");
   CHECK(task_content_park_core_load(bad_core), "Load Core was not parked");
   warn_cancel("no warning for LOAD_CORE");
   fw_record(&fw_now);
   CHECK(!fw_changes(false), "the LOAD_CORE Cancel changed the running session");
   fw_changes(true);

   /* C1 The companion UI's Load Core over a game */
   CHECK(!warn_companion_ln(bad_core), "the companion Load Core opened");
   warn_cancel("no warning for the companion Load Core");
   fw_record(&fw_now);
   CHECK(!fw_changes(false), "the companion Cancel changed the running session");
   fw_changes(true);

   /* L2 The game closes under the warning */
   CHECK(!warn_ln(bad_core), "the untrusted Load Core opened");
   CHECK(task_content_park_core_load(bad_core), "Load Core was not parked");
   command_event(CMD_EVENT_UNLOAD_CORE, NULL);
   pump(120);
   warn_cancel("no warning once the game closed");
   warn_check_no_core("a Cancel named the game closed under its warning");

   /* C2 The companion UI's Load Core, nothing chosen */
   CHECK(!warn_companion_ln(bad_core), "the companion Load Core opened");
   warn_cancel("no warning for the companion Load Core");
   warn_check_no_core("the companion Cancel left the core named");

   /* C3 Another core stays chosen */
   CHECK(warn_companion_ln(good_core), "the trusted companion Load Core failed");
   fw_record(&fw_ran);
   CHECK(!warn_companion_ln(bad_core), "the companion Load Core opened");
   warn_cancel("no warning over the chosen core");
   warn_check_kept("a companion Cancel lost the chosen core");

   /* C4 A load that opened keeps nothing for a later Cancel */
   CHECK(warn_companion_ln(good_core), "the trusted companion Load Core failed");
   warn_check_nothing_kept("a companion load that opened left its snapshot");

   /* C5 Nor does one that failed without a warning */
   snprintf(missing, sizeof(missing), "%s/missing_libretro.%s", cores, ext);
   path_set(RARCH_PATH_CORE, good_core);
   CHECK(!warn_companion_ln(missing), "a missing core opened");
   CHECK(!warning_up(), "a missing core asked");
   warn_check_nothing_kept("a companion load that failed left its snapshot");

   /* C6 Nor does a refusal only reported, never parked (netplay) */
   CHECK(warn_ln(good_core), "the trusted Load Core failed");
   CHECK(!warn_ln(bad_core), "the untrusted Load Core opened");
   CHECK(task_content_core_refused_notify(bad_core),
         "the refusal was not reported");
   warn_check_nothing_kept("a reported refusal left its snapshot");
}

static void mode_warn_cancel_startup(void)
{
   warn_cancel("no warning for the startup core");
   warn_check_no_core("the startup core's Cancel left it named");
}

static void mode_fw_cancel_startup(void)
{
   content_ctx_info_t info;
   memset(&info, 0, sizeof(info));

   /* F1 Always Trust, firmware Cancel */
   CHECK(wait_for(warning_up, 120), "no Security Warning for the startup core");
   CHECK(answer(CORE_WARNING_ALWAYS_TRUST), "Always Trust not taken");
   CHECK(fw_cancelled(), "the firmware box's Cancel did not end the launch");
   warn_check_no_core("a firmware Cancel after Always Trust left the core named");

   /* F2 Firmware OK, then Cancel */
   CHECK(task_push_load_content_from_playlist_from_menu(fwb_core, game, "f",
            &info, NULL, NULL), "the untrusted Run was not started");
   CHECK(fw_wait_box(600), "no firmware box");
   fw_answer(MENU_ACTION_OK);
   warn_cancel("no warning after the firmware box's OK");
   warn_check_no_core("a Cancel after the firmware box's OK left the core named");
}

#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
/* Until the copy task has an answer: it reads and writes the whole
 * core file, which a loaded runner takes seconds over */
static enum runahead_copy_status secondary_wait(runloop_state_t *runloop_st,
      settings_t *settings)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 10000000;
   for (;;)
   {
      enum runahead_copy_status st =
         secondary_core_ensure_exists(runloop_st, settings);
      if (     st != RUNAHEAD_COPY_PENDING
            || cpu_features_get_time_usec() > deadline)
         return st;
      task_queue_check();
      retro_sleep(1);
   }
}

/* Run-ahead's copy of a core trusted through its check: the copy
 * matches and its second instance runs. bad_core is the build without
 * a hardware context, which run-ahead's second instance needs. */
static void mode_runahead(void)
{
   settings_t *settings        = config_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   enum runahead_copy_status st;
   set_threaded();
   staged_trust(bad_core);
   st = secondary_wait(runloop_st, settings);
   CHECK(st == RUNAHEAD_COPY_READY,
         "the copy of a trusted running core gave no instance");
   CHECK(runloop_st->secondary_lib_handle
         && runloop_st->secondary_lib_handle != runloop_st->lib_handle,
         "no second instance");
   runahead_secondary_core_destroy(runloop_st);
}
#endif

/* builtin's core saves its options in the config folder */
#define PAD_OPT_DIR "Remote RetroPad"

/* What RetroArch makes in its config folder, deepest first */
static const char *const made_dirs[] = {
#ifdef __APPLE__
   "assets", "cht/content_load_harness", "cht",
#endif
   "autoconfig", "cheats/content_load_harness", "cheats",
   "config/" PAD_OPT_DIR, "config/remaps", "config", "cores",
   "database/rdb", "database", "downloads",
   "filters/audio", "filters/video", "filters", "logs",
   "overlays/keyboards", "overlays", "playlists", "records",
   "records_config", "saves", "screenshots", "shaders", "states",
   "system", "thumbnails"
};

#ifdef __APPLE__
/* Under the run's home: macOS keeps the store and most folders in
 * HOME's Application Support, the rest in Foundation's Documents */
static const char *const apple_dirs[] = {
   "Library/Application Support", "Library",
   "Documents/RetroArch/logs", "Documents/RetroArch/playlists/builtin",
   "Documents/RetroArch/playlists", "Documents/RetroArch/records",
   "Documents/RetroArch/records_config", "Documents/RetroArch/saves",
   "Documents/RetroArch/screenshots", "Documents/RetroArch/states",
   "Documents/RetroArch/system", "Documents/RetroArch", "Documents"
};
#endif

static char cfg_home[300];
static char ra_dir[320];
static char ra_cores[340];
#ifndef _WIN32
/* Run-ahead's copy folder, in TMPDIR */
static char ra_temp[300];
static bool ra_temp_had;
#endif

#ifdef _WIN32
/* RetroArch's default folders beside the program; the cfg can't move
 * them all, deepest first */
static const char *const win_dirs[] = {
   ":\\assets\\wallpapers", ":\\assets", ":\\autoconfig",
   ":\\cheats\\content_load_harness", ":\\cheats",
   ":\\config\\" PAD_OPT_DIR, ":\\config\\record", ":\\config\\remaps",
   ":\\config", ":\\cores", ":\\database\\rdb",
   ":\\database", ":\\downloads", ":\\filters\\audio",
   ":\\filters\\video", ":\\filters", ":\\info", ":\\logs",
   ":\\overlays\\keyboards", ":\\overlays", ":\\playlists",
   ":\\recordings", ":\\saves", ":\\screenshots", ":\\shaders",
   ":\\states", ":\\system", ":\\thumbnails"
};
static bool win_had[sizeof(win_dirs) / sizeof(win_dirs[0])];
/* bad_core's name with a trailing '.', as \\?\ keeps it */
static char dot_core[410];
#endif

static void remove_in(const char *in, const char *name)
{
   char path[800];
   snprintf(path, sizeof(path), "%s/%s", in, name);
   remove(path);
}

/* Everything a run made, by name; what was never made is skipped */
static void clean_run(void)
{
   size_t i;
#ifdef _WIN32
   /* Only the folders this run made: rmdir takes only empty ones */
   for (i = 0; i < sizeof(win_dirs) / sizeof(win_dirs[0]); i++)
   {
      char path[PATH_MAX_LENGTH];
      if (win_had[i])
         continue;
      if (string_is_equal(win_dirs[i], ":\\config\\" PAD_OPT_DIR))
      {
         fill_pathname_expand_special(path,
               ":\\config\\" PAD_OPT_DIR "\\" PAD_OPT_DIR ".opt",
               sizeof(path));
         remove(path);
      }
      fill_pathname_expand_special(path, win_dirs[i], sizeof(path));
      rmdir(path);
   }
   if (*dot_core)
      remove(dot_core);
#endif
   remove(good_core);
   remove(bad_core);
   remove(nc_core);
   if (*broken)
      remove(broken);
   if (*fw_core)
      remove(fw_core);
   if (*fwb_core)
      remove(fwb_core);
   if (*sub_core)
      remove(sub_core);
   if (*fws_core)
      remove(fws_core);
   rmdir(cores);
   if (*seeded)
      remove(seeded);
   /* LIBRETRO_DIRECTORY puts the caches there */
   remove_in(ra_cores, "core_probe.cache");
   remove_in(ra_cores, "core_info.cache");
   remove_in(ra_cores, "seeded_libretro.info");
   remove_in(ra_dir, CORE_TRUST_FILE_NAME);
   remove_in(ra_dir, CORE_TRUST_FILE_NAME ".testkey");
   remove_in(ra_dir, "config/" PAD_OPT_DIR "/" PAD_OPT_DIR ".opt");
   for (i = 0; i < sizeof(made_dirs) / sizeof(made_dirs[0]); i++)
   {
      char path[PATH_MAX_LENGTH];
      snprintf(path, sizeof(path), "%s/%s", ra_dir, made_dirs[i]);
      rmdir(path);
   }
   rmdir(ra_dir);
#ifdef __APPLE__
   for (i = 0; i < sizeof(apple_dirs) / sizeof(apple_dirs[0]); i++)
   {
      char path[PATH_MAX_LENGTH];
      snprintf(path, sizeof(path), "%s/%s", cfg_home, apple_dirs[i]);
      rmdir(path);
   }
#endif
   rmdir(cfg_home);
   remove_in(dir, "harness.cfg");
   remove_in(dir, "title.bin");
   remove_in(dir, "sub.bin");
   remove_in(dir, "harness.lpl");
   remove_in(dir, "core_probe.cache");
   remove_in(dir, "core_info.cache");
   remove_in(dir, "fw_libretro.info");
   remove_in(dir, "fwb_libretro.info");
   remove_in(dir, "fws_libretro.info");
   remove_in(dir, "seeded_libretro.info");
   remove_in(dir, "other.bin");
   remove_in(dir, "system/harness/fw.bin");
   /* A netplay client's save folder; the fw- modes' system folder */
   {
      char path[PATH_MAX_LENGTH];
      snprintf(path, sizeof(path), "%s/.netplay", dir);
      rmdir(path);
      snprintf(path, sizeof(path), "%s/system/harness", dir);
      rmdir(path);
      snprintf(path, sizeof(path), "%s/system", dir);
      rmdir(path);
   }
   rmdir(dir);
#ifndef _WIN32
   if (!ra_temp_had)
      rmdir(ra_temp);
#endif
}

#ifdef _WIN32
/* Refused as named, unchecked: nothing asks or opens */
static void staged_dot_refused(const char *what)
{
   CHECK(wait_for(is_dummy, 600), "the refused load did not end on the dummy core");
   pump(30);
   CHECK(!warning_up(), what);
   if (warning_up())
      core_warning_choose(CORE_WARNING_CANCEL);
   CHECK(!runloop_state_get_ptr()->lib_handle, "a core was opened");
}

/* Load Core and a playlist Run of a \\?\ core path ending in '.' */
static void mode_staged_verbatim_dot(void)
{
   content_ctx_info_t info;
   snprintf(dot_core, sizeof(dot_core), "\\\\?\\%s.", bad_core);
   pathname_conform_slashes_to_os(dot_core);
   write_file(dot_core);
   CHECK(task_push_load_contentless_core_from_menu(dot_core),
         "the staged load was not started");
   staged_dot_refused("a staged \\\\?\\ core path ending in '.' asked");
   snprintf(game, sizeof(game), "%s/title.bin", dir);
   write_file(game);
   memset(&info, 0, sizeof(info));
   CHECK(task_push_load_content_from_playlist_from_menu(dot_core, game,
            "title", &info, NULL, NULL), "the playlist Run was not started");
   staged_dot_refused("a playlist Run of a \\\\?\\ core path ending in '.' asked");
}
#endif

/* Load Once on a parked Load Core whose core then fails to open says
 * so, as a Load Core that fails does */
static void mode_load_core_broken(void)
{
   struct retro_system_info info;
   bool no_content = false;
   bool failed     = false;
   unsigned i;
   snprintf(broken, sizeof(broken), "%s/broken_libretro.%s", cores, ext);
   write_file(broken);
   CHECK(!libretro_get_system_info(broken, &info, &no_content),
         "a probe opened an unchecked core");
   pump(60);
   CHECK(task_content_park_core_load(broken), "Load Core was not parked");
   CHECK(wait_for(warning_up, 120), "no warning for the parked Load Core");
   CHECK(!open_failed_queued(), "the parked Load Core said it failed");
   CHECK(core_warning_choose(CORE_WARNING_LOAD_ONCE), "Load Once not taken");
   for (i = 0; i < 600 && !(failed = open_failed_queued()); i++)
      pump(1);
   CHECK(failed, "Load Once on a core that fails to open said nothing");
   CHECK(!warning_up(), "the core that fails to open asked again");
}

#ifdef CORE_LOAD_PATHS_WRAP
/* -1: the real quick check; else the result it gives, final */
static int quick_forced = -1;

bool __real_core_check_quick(const core_trust_set_t *s, core_file_t *f,
      const char *path, struct core_check *c);

bool __wrap_core_check_quick(const core_trust_set_t *s, core_file_t *f,
      const char *path, struct core_check *c)
{
   bool ret = __real_core_check_quick(s, f, path, c);
   if (quick_forced < 0)
      return ret;
   c->result = (enum core_check_result)quick_forced;
   return true;
}
#endif

/* With no menu, a parked Load Core refused on its re-run and not
 * parked again says only that it was refused */
static void mode_null_load_core(void)
{
#ifdef CORE_LOAD_PATHS_WRAP
   struct retro_system_info info;
   bool no_content  = false;
   bool open_failed = false;
   bool refused     = false;
   unsigned i;
   CHECK(!libretro_get_system_info(bad_core, &info, &no_content),
         "a probe opened an unchecked core");
   /* Answered without asking, so it runs again */
   quick_forced = CORE_CHECK_OK;
   CHECK(task_content_park_core_load(bad_core), "Load Core was not parked");
   /* Its re-run refused, with an answer the park can't ask about */
   quick_forced = CORE_CHECK_ERROR;
   for (i = 0; i < 60 && !refused; i++)
   {
      pump(1);
      refused = no_menu_refusal_queued(&open_failed);
   }
   quick_forced = -1;
   CHECK(refused, "the refused re-run did not say why");
   pump(30);
   no_menu_refusal_queued(&open_failed);
   CHECK(!open_failed, "the refused re-run also said its core failed to open");
#else
   fprintf(stderr, "null-load-core: needs ld's --wrap\n");
#endif
}

/* Before the startup, as an earlier session's probe: the record on
 * disk, so a later probe of the core opens nothing */
static void probe_earlier(const char *core)
{
   settings_t *settings = config_get_ptr();
   struct retro_system_info info;
   char cache[300];
   bool no_game         = false;
   strlcpy(settings->paths.path_libretro_info, dir,
         sizeof(settings->paths.path_libretro_info));
   CHECK(libretro_get_system_info(core, &info, &no_game) && no_game,
         "the earlier session's probe failed");
   snprintf(cache, sizeof(cache), "%s/core_probe.cache", dir);
   CHECK(path_is_valid(cache), "the earlier session's probe was not kept");
}

/* dir/name: core info that asks for a file the empty system folder
 * lacks */
/* name, an info file asking for harness/fw.bin, in the info folder in */
static bool fw_info_write(const char *in, const char *name)
{
   char path[PATH_MAX_LENGTH];
   FILE *f;
   snprintf(path, sizeof(path), "%s/%s", in, name);
   if (!(f = fopen(path, "wb")))
      return false;
   fprintf(f, "display_name = \"Firmware harness\"\n");
   fprintf(f, "supports_no_game = \"true\"\n");
   fprintf(f, "firmware_count = \"1\"\n");
   fprintf(f, "firmware0_desc = \"fw.bin (harness)\"\n");
   fprintf(f, "firmware0_path = \"harness/fw.bin\"\n");
   fprintf(f, "firmware0_opt = \"false\"\n");
   fclose(f);
   return true;
}

/* fw_core and fwb_core in a fw- mode, with fw_info_write's info */
static bool fw_setup(const char *argv0)
{
   char path[PATH_MAX_LENGTH];
   snprintf(fw_core, sizeof(fw_core), "%s/fw_libretro.%s", cores, ext);
   snprintf(fwb_core, sizeof(fwb_core), "%s/fwb_libretro.%s", cores, ext);
   snprintf(sub_core, sizeof(sub_core), "%s/sub_libretro.%s", cores, ext);
   snprintf(fws_core, sizeof(fws_core), "%s/fws_libretro.%s", cores, ext);
   snprintf(path, sizeof(path), "%s/system", dir);
   return path_mkdir(path)
      && copy_beside(argv0, "harness_core.so", fw_core)
      && copy_beside(argv0, "harness_core_b.so", fwb_core)
      && copy_beside(argv0, "harness_core_sub_a.so", sub_core)
      && copy_beside(argv0, "harness_core_sub_b.so", fws_core)
      && fw_info_write(dir, "fw_libretro.info")
      && fw_info_write(dir, "fwb_libretro.info")
      && fw_info_write(dir, "fws_libretro.info");
}

int main(int argc, char *argv[])
{
   static char env_xdg[400];
   static char env_home[400];
#ifdef __APPLE__
   static char env_cf_home[400];
#endif
#ifdef _WIN32
   static char env_appdata[400];
   static char env_cores[400];
#endif
   char cfg_path[300];
   char name[32];
   char *rarch_argv[8]  = {0};
   int rarch_argc       = 0;
   const char *mode     = argc > 1 ? argv[1] : "";
   const char *tmp      = getenv("TMPDIR");
   bool startup_dummy   = false;
   bool no_menu         = string_is_equal(mode, "null-seed")
                       || string_is_equal(mode, "null-refused")
                       || string_is_equal(mode, "null-load-core")
                       || string_is_equal(mode, "fw-null");
   bool seed_mode       = string_is_equal(mode, "seed")
                       || string_is_equal(mode, "seed-said")
                       || string_is_equal(mode, "null-seed")
                       || string_is_equal(mode, "fw-null");
   bool cached_startup  = string_is_equal(mode, "startup-cached")
                       || string_is_equal(mode, "startup-cached-always");
   bool outside         = cached_startup
                       || string_is_equal(mode, "startup-outside");
   bool verbatim_dot    = string_is_equal(mode, "startup-verbatim-dot");
   bool fw_mode         = string_starts_with_size(mode, "fw-",
         STRLEN_CONST("fw-"));
   bool inited;
   unsigned n;
   size_t i;
   FILE *cfg;

#ifdef _WIN32
   for (i = 0; i < sizeof(win_dirs) / sizeof(win_dirs[0]); i++)
   {
      char path[PATH_MAX_LENGTH];
      fill_pathname_expand_special(path, win_dirs[i], sizeof(path));
      win_had[i] = path_is_directory(path);
   }
#endif
   if (!tmp || !*tmp)
      tmp = getenv("TEMP");
   if (!tmp || !*tmp)
      tmp = "/tmp";
#ifndef _WIN32
   snprintf(ra_temp, sizeof(ra_temp), "%s/retroarch_temp", tmp);
   ra_temp_had = path_is_directory(ra_temp);
#endif
   /* A new folder for each run: nothing one leaves reaches the next,
    * even with a pid that repeats (Wine's) */
   for (i = 0; mode[i] && i < sizeof(name) - 1; i++)
      name[i] = ISALNUM(mode[i]) ? mode[i] : '-';
   name[i] = '\0';
   for (n = 0; ; n++)
   {
      snprintf(dir, sizeof(dir), "%s/core_load_paths_%s_%ld_%u", tmp,
            name, (long)getpid(), n);
      if (!path_is_valid(dir))
         break;
      if (n == 99)
      {
         fprintf(stderr, "FAIL: no new run folder in %s\n", tmp);
         return 1;
      }
   }
   snprintf(cfg_home, sizeof(cfg_home), "%s/cfg", dir);
#ifdef __APPLE__
   snprintf(ra_dir, sizeof(ra_dir), "%s/Library/Application Support/RetroArch",
         cfg_home);
#else
   snprintf(ra_dir, sizeof(ra_dir), "%s/retroarch", cfg_home);
#endif
   snprintf(ra_cores, sizeof(ra_cores), "%s/cores", ra_dir);
   snprintf(cores, sizeof(cores), "%s/cores", dir);
   if (!path_mkdir(cfg_home) || !path_mkdir(cores))
   {
      clean_run();
      return 1;
   }
   /* A fresh trust store and default cores folder for every mode */
   snprintf(env_xdg, sizeof(env_xdg), "XDG_CONFIG_HOME=%s", cfg_home);
   snprintf(env_home, sizeof(env_home), "HOME=%s", cfg_home);
   putenv(env_xdg);
   putenv(env_home);
#ifdef __APPLE__
   /* Foundation's home, which ignores HOME */
   snprintf(env_cf_home, sizeof(env_cf_home), "CFFIXED_USER_HOME=%s",
         cfg_home);
   putenv(env_cf_home);
#endif
#ifdef _WIN32
   /* The store's folder there */
   snprintf(env_appdata, sizeof(env_appdata), "APPDATA=%s", cfg_home);
   putenv(env_appdata);
   /* The default cores folder, the seed's, is beside the program
    * unless this names it. It also overrides libretro_directory, so
    * only for the seed modes. */
   if (seed_mode)
   {
      snprintf(env_cores, sizeof(env_cores), "LIBRETRO_DIRECTORY=%s",
            ra_cores);
      putenv(env_cores);
   }
#endif
   if (getenv("HARNESS_VERBOSE"))
      verbosity_enable();

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   runloop_msg_queue_init();

   /* The seed walks only files with the platform's core extension */
   if (!frontend_driver_get_core_extension(ext, sizeof(ext)))
      strlcpy(ext, "so", sizeof(ext));
   snprintf(good_core, sizeof(good_core), "%s/good_libretro.%s", cores, ext);
   snprintf(bad_core, sizeof(bad_core), "%s/bad_libretro.%s", cores, ext);
   snprintf(nc_core, sizeof(nc_core), "%s/nc_libretro.%s", cores, ext);
   if (     !copy_beside(argv[0], "harness_core.so", good_core)
         || !copy_beside(argv[0], "harness_core_b.so", bad_core)
         || !copy_beside(argv[0], "needs_content_core.so", nc_core))
   {
      fprintf(stderr, "FAIL: no harness cores beside %s: build.sh builds them\n",
            argv[0]);
      clean_run();
      return 1;
   }
   if (fw_mode && !fw_setup(argv[0]))
   {
      fprintf(stderr, "FAIL: no firmware core\n");
      clean_run();
      return 1;
   }
   if (string_is_equal(mode, "warn-cancel-restores"))
   {
      snprintf(sub_core, sizeof(sub_core), "%s/sub_libretro.%s", cores, ext);
      if (!copy_beside(argv[0], "harness_core_sub_a.so", sub_core))
      {
         fprintf(stderr, "FAIL: no subsystem core\n");
         clean_run();
         return 1;
      }
   }

   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", dir);
   if (!(cfg = fopen(cfg_path, "wb")))
   {
      clean_run();
      return 1;
   }
   fprintf(cfg, "video_driver = \"null\"\n");
   fprintf(cfg, "audio_driver = \"null\"\n");
   fprintf(cfg, "audio_enable = \"false\"\n");
   fprintf(cfg, "microphone_enable = \"false\"\n");
   fprintf(cfg, "input_driver = \"null\"\n");
   fprintf(cfg, "input_joypad_driver = \"test\"\n");
   fprintf(cfg, "menu_driver = \"%s\"\n", no_menu ? "null" : "rgui");
   fprintf(cfg, "video_threaded = \"false\"\n");
   fprintf(cfg, "video_vsync = \"false\"\n");
   fprintf(cfg, "menu_pause_libretro = \"true\"\n");
   fprintf(cfg, "pause_nonactive = \"false\"\n");
   fprintf(cfg, "gamemode_enable = \"false\"\n");
   fprintf(cfg, "suspend_screensaver_enable = \"false\"\n");
   fprintf(cfg, "config_save_on_exit = \"false\"\n");
   fprintf(cfg, "savestate_auto_save = \"false\"\n");
   fprintf(cfg, "history_list_enable = \"false\"\n");
   fprintf(cfg, "sort_savefiles_enable = \"false\"\n");
   fprintf(cfg, "sort_savestates_enable = \"false\"\n");
   fprintf(cfg, "menu_show_load_content_animation = \"false\"\n");
   /* A first run's prompt would take the warning screen's input */
   fprintf(cfg, "rgui_show_start_screen = \"false\"\n");
   fprintf(cfg, "threaded_data_runloop_enable = \"false\"\n");
   fprintf(cfg, "libretro_info_path = \"%s\"\n", dir);
   /* The cores folder, whose cores the core info list holds: the
    * harness cores' but for the startups outside it; the seed modes
    * keep the default, the seed's */
   if (outside)
      fprintf(cfg, "libretro_directory = \"%s\"\n", ra_cores);
   else if (!seed_mode)
      fprintf(cfg, "libretro_directory = \"%s\"\n", cores);
   fprintf(cfg, "savefile_directory = \"%s\"\n", dir);
   fprintf(cfg, "savestate_directory = \"%s\"\n", dir);
   if (fw_mode)
   {
      fprintf(cfg, "system_directory = \"%s/system\"\n", dir);
      fprintf(cfg, "core_firmware_notification_enable = \"true\"\n");
      fprintf(cfg, "core_info_cache_enable = \"false\"\n");
   }
   fclose(cfg);

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;
   if (     string_is_equal(mode, "startup")
         || string_is_equal(mode, "startup-always")
         || outside
         || string_is_equal(mode, "null-refused"))
   {
      rarch_argv[rarch_argc++] = (char*)"-L";
      rarch_argv[rarch_argc++] = bad_core;
      startup_dummy            = true;
   }
   else if (string_is_equal(mode, "startup-content")
         || string_is_equal(mode, "warn-cancel-startup"))
   {
      snprintf(game, sizeof(game), "%s/title.bin", dir);
      write_file(game);
      rarch_argv[rarch_argc++] = (char*)"-L";
      rarch_argv[rarch_argc++] = bad_core;
      rarch_argv[rarch_argc++] = game;
      startup_dummy            = true;
   }
   else if (string_is_equal(mode, "startup-needs-content")
         || string_is_equal(mode, "startup-moved"))
   {
      rarch_argv[rarch_argc++] = (char*)"-L";
      rarch_argv[rarch_argc++] = nc_core;
      startup_dummy            = true;
   }
   else if (string_is_equal(mode, "fw-load-once")
         || string_is_equal(mode, "fw-cancel-startup"))
   {
      snprintf(game, sizeof(game), "%s/title.bin", dir);
      write_file(game);
      rarch_argv[rarch_argc++] = (char*)"-L";
      rarch_argv[rarch_argc++] = fw_core;
      rarch_argv[rarch_argc++] = game;
      startup_dummy            = true;
   }
#ifdef _WIN32
   else if (verbatim_dot)
   {
      /* The literal file -L takes; the load must not drop the '.' and
       * open bad_core */
      snprintf(dot_core, sizeof(dot_core), "\\\\?\\%s.", bad_core);
      pathname_conform_slashes_to_os(dot_core);
      write_file(dot_core);
      rarch_argv[rarch_argc++] = (char*)"-L";
      rarch_argv[rarch_argc++] = dot_core;
   }
#endif
   else if (seed_mode)
   {
      /* In the default cores folder, which the first run's seed walks */
      path_mkdir(ra_cores);
      snprintf(seeded, sizeof(seeded), "%s/seeded_libretro.%s", ra_cores,
            ext);
      if (!copy_beside(argv[0], "harness_core.so", seeded))
      {
         clean_run();
         return 1;
      }
      snprintf(broken, sizeof(broken), "%s/broken_libretro.%s", ra_cores,
            ext);
      write_file(broken);
      rarch_argv[rarch_argc++] = (char*)"-L";
      rarch_argv[rarch_argc++] = seeded;
      startup_dummy            = true;
      if (string_is_equal(mode, "fw-null"))
      {
         snprintf(game, sizeof(game), "%s/title.bin", dir);
         write_file(game);
         rarch_argv[rarch_argc++] = game;
         /* Where the info list reads: on Windows LIBRETRO_DIRECTORY,
          * set for the seed modes, names the info folder too */
#ifdef _WIN32
         if (!fw_info_write(ra_cores, "seeded_libretro.info"))
#else
         if (!fw_info_write(dir, "seeded_libretro.info"))
#endif
         {
            clean_run();
            return 1;
         }
      }
   }

   if (     cached_startup || string_is_equal(mode, "netplay-join-cached")
         || string_is_equal(mode, "warn-cancel-restores")
         || string_is_equal(mode, "warn-cancel-startup"))
      probe_earlier(bad_core);
   if (string_is_equal(mode, "fw-cancel-startup"))
      probe_earlier(fw_core);
   retroarch_user_dirs_hold();
   {
      /* As rarch_main() starts: the one load the check hashes in place */
      frontend_ctx_driver_t *frontend = frontend_state_get_ptr()->current_frontend_ctx;
      content_ctx_info_t info;
      info.argc        = rarch_argc;
      info.argv        = rarch_argv;
      info.args        = NULL;
      info.environ_get = frontend ? frontend->environment_get : NULL;
      inited           = task_push_load_content_from_cli(NULL, NULL, &info,
            CORE_TYPE_PLAIN, NULL, NULL);
   }
   CHECK(!open_failed_queued(),
         "a held back startup load said its core failed to open");
   if (string_is_equal(mode, "null-refused"))
   {
      /* Refused with no menu to ask on: the startup fails */
      CHECK(!inited, "a refused startup load with no menu did not fail");
      CHECK(core_load_check_refused(bad_core),
            "the startup failed, but not on the check's refusal");
      /* As the process's exit would: Windows deletes no open file */
      core_load_check_deinit();
   }
#ifdef _WIN32
   else if (verbatim_dot)
   {
      /* Refused as named, unchecked: nothing asks or opens */
      CHECK(!inited, "a \\\\?\\ core path ending in '.' did not fail the startup");
      CHECK(!warning_up(), "a \\\\?\\ core path ending in '.' asked");
      CHECK(!runloop_state_get_ptr()->lib_handle, "a core was opened");
      core_load_check_deinit();
   }
#endif
   else if (!inited)
   {
      fprintf(stderr, "FAIL: the startup failed\n");
      failures++;
   }
   else
   {
      /* Before any frame: nothing can have run it again yet */
      if (startup_dummy)
         CHECK(is_dummy(), "the held back startup core did not fall back to the dummy core");
      if (string_is_equal(mode, "seed-said"))
         seed_said();
      pump(5);
      if (!core_trust_runtime_active())
      {
         fprintf(stderr, "FAIL: no trust store: build with build.sh\n");
         failures++;
      }
      else if (string_is_equal(mode, "startup"))
         mode_startup(CORE_WARNING_LOAD_ONCE);
      else if (string_is_equal(mode, "startup-always"))
         mode_startup(CORE_WARNING_ALWAYS_TRUST);
      else if (string_is_equal(mode, "startup-outside")
            || string_is_equal(mode, "startup-cached"))
         mode_startup(CORE_WARNING_LOAD_ONCE);
      else if (string_is_equal(mode, "startup-cached-always"))
         mode_startup(CORE_WARNING_ALWAYS_TRUST);
      else if (string_is_equal(mode, "startup-content"))
         mode_startup_content();
      else if (string_is_equal(mode, "startup-needs-content"))
         mode_startup_needs_content();
      else if (string_is_equal(mode, "startup-moved"))
#ifdef _WIN32
         /* The check's handle is held: the core can't move */
         fprintf(stderr, "startup-moved: not on Windows\n");
#else
         mode_startup_moved();
#endif
#ifndef _WIN32
      else if (verbatim_dot)
         fprintf(stderr, "startup-verbatim-dot: Windows only\n");
#endif
      else if (string_is_equal(mode, "null-seed"))
         mode_null_seed();
      else if (string_is_equal(mode, "staged"))
         staged_trust(bad_core);
      else if (string_is_equal(mode, "staged-threaded"))
      {
         set_threaded();
         staged_trust(bad_core);
      }
      else if (string_is_equal(mode, "staged-moved"))
#ifdef _WIN32
         /* The check's handle is held: the core can't move */
         fprintf(stderr, "staged-moved: not on Windows\n");
#else
         mode_staged_moved();
#endif
      else if (string_is_equal(mode, "staged-verbatim-dot"))
#ifdef _WIN32
         mode_staged_verbatim_dot();
#else
         fprintf(stderr, "staged-verbatim-dot: Windows only\n");
#endif
      else if (string_is_equal(mode, "playlist"))
         mode_playlist();
      else if (string_is_equal(mode, "subsystem"))
         mode_subsystem();
      else if (string_is_equal(mode, "playlist-sub"))
         mode_playlist_sub();
      else if (string_is_equal(mode, "netplay-sub"))
#ifdef HAVE_NETWORKING
         mode_netplay_sub();
#else
         fprintf(stderr, "netplay-sub: not in this build\n");
#endif
      else if (string_is_equal(mode, "netplay-join")
            || string_is_equal(mode, "netplay-join-cached"))
#ifdef HAVE_NETWORKING
         mode_netplay_join(string_is_equal(mode, "netplay-join-cached"));
#else
         fprintf(stderr, "%s: not in this build\n", mode);
#endif
      else if (string_is_equal(mode, "probe"))
         mode_probe();
      else if (string_is_equal(mode, "load-core-start"))
         mode_load_core_start();
      else if (string_is_equal(mode, "load-core-moved"))
#ifdef _WIN32
         /* The check's handle is held: the core can't move */
         fprintf(stderr, "load-core-moved: not on Windows\n");
#else
         mode_load_core_moved();
#endif
      else if (string_is_equal(mode, "load-core-broken"))
         mode_load_core_broken();
      else if (string_is_equal(mode, "null-load-core"))
         mode_null_load_core();
      else if (string_is_equal(mode, "seed")
            || string_is_equal(mode, "seed-said"))
         mode_seed();
      else if (string_is_equal(mode, "load-once"))
         mode_load_once();
      else if (string_is_equal(mode, "park-ends"))
         mode_park_ends();
      else if (string_is_equal(mode, "fw-stack"))
         mode_fw_stack();
      else if (string_is_equal(mode, "fw-contentless"))
         mode_fw_contentless();
      else if (string_is_equal(mode, "fw-hidden-confirm"))
         mode_fw_hidden_confirm();
      else if (string_is_equal(mode, "fw-null"))
         mode_fw_null();
      else if (string_is_equal(mode, "fw-twice"))
         mode_fw_twice();
      else if (string_is_equal(mode, "fw-cancel-restores"))
         mode_fw_cancel_restores();
      else if (string_is_equal(mode, "fw-load-once"))
         mode_fw_load_once();
      else if (string_is_equal(mode, "builtin"))
#ifdef HAVE_NETWORKGAMEPAD
         mode_builtin();
#else
         fprintf(stderr, "builtin: not in this build\n");
#endif
      else if (string_is_equal(mode, "builtin-first-press"))
#ifdef HAVE_NETWORKGAMEPAD
         mode_builtin_first_press();
#else
         fprintf(stderr, "builtin-first-press: not in this build\n");
#endif
      else if (string_is_equal(mode, "fw-builtin"))
#ifdef HAVE_NETWORKGAMEPAD
         mode_fw_builtin();
#else
         fprintf(stderr, "fw-builtin: not in this build\n");
#endif
      else if (string_is_equal(mode, "warn-cancel-restores"))
         mode_warn_cancel_restores();
      else if (string_is_equal(mode, "warn-cancel-live"))
         mode_warn_cancel_live();
      else if (string_is_equal(mode, "warn-cancel-startup"))
         mode_warn_cancel_startup();
      else if (string_is_equal(mode, "fw-cancel-startup"))
         mode_fw_cancel_startup();
      else if (string_is_equal(mode, "runahead"))
#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC) && defined(HAVE_THREADS)
         mode_runahead();
#else
         fprintf(stderr, "runahead: not in this build\n");
#endif
      else
      {
         fprintf(stderr, "FAIL: mode \"%s\"\n", mode);
         failures++;
      }
      main_exit(NULL);
      /* Nothing a session refused, parked or held outlives it */
      if (string_is_equal(mode, "probe"))
         CHECK(!core_load_check_refused(bad_core),
               "the load check's state outlived main_exit");
   }

   clean_run();

   fprintf(stderr, "%s: %u failures\n", mode, failures);
   return failures ? 1 : 0;
}
