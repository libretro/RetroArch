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

static unsigned failures;
#define CHECK(cond, what) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, what); \
   failures++; } } while (0)

static char dir[256];
static char ext[16];
static char good_core[400];   /* never trusted before the mode trusts it */
static char bad_core[400];    /* another build: never trusted */
static char nc_core[400];     /* needs content: never trusted */
static char seeded[400];      /* in the first run's seed folder */
static char broken[400];      /* a core file with no library */
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

static bool good_runs_game(void)
{
   return good_runs()
      && (content_state_get_ptr()->flags & CONTENT_ST_FLAG_IS_INITED)
      && same_path(path_get(RARCH_PATH_CONTENT), game);
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
#endif

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
static char cores[340];
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
   rmdir(cores);
   if (*seeded)
      remove(seeded);
   /* LIBRETRO_DIRECTORY puts the caches there */
   remove_in(ra_cores, "core_probe.cache");
   remove_in(ra_cores, "core_info.cache");
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
   /* A netplay client's save folder */
   {
      char path[PATH_MAX_LENGTH];
      snprintf(path, sizeof(path), "%s/.netplay", dir);
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
                       || string_is_equal(mode, "null-load-core");
   bool seed_mode       = string_is_equal(mode, "seed")
                       || string_is_equal(mode, "seed-said")
                       || string_is_equal(mode, "null-seed");
   bool cached_startup  = string_is_equal(mode, "startup-cached")
                       || string_is_equal(mode, "startup-cached-always");
   bool outside         = cached_startup
                       || string_is_equal(mode, "startup-outside");
   bool verbatim_dot    = string_is_equal(mode, "startup-verbatim-dot");
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
   else if (string_is_equal(mode, "startup-content"))
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
   }

   if (cached_startup || string_is_equal(mode, "netplay-join-cached"))
      probe_earlier(bad_core);
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
      else if (string_is_equal(mode, "builtin"))
#ifdef HAVE_NETWORKGAMEPAD
         mode_builtin();
#else
         fprintf(stderr, "builtin: not in this build\n");
#endif
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
