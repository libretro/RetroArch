/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>

#include "menu_dirwalk.h"

#include "../tasks/tasks_internal.h"

/* Sorting a large completed listing is itself a stall: qsort of a
 * 40k-entry directory costs ~15ms of main-thread time, well past the
 * shared window.  Above the synchronous threshold the sort runs as a
 * resumable bottom-up mergesort on the task: fixed-size runs are
 * qsorted one or more per window, then merge passes proceed with the
 * budget consulted every batch of copied elements.  The comparator
 * comes from dir_list_sort_cmp(), so the ordering is exactly the one
 * dir_list_sort() produces (equal-comparing entries excepted, whose
 * relative order qsort never specified).
 *
 * The constants are compile-overridable so the oracle can drive the
 * machinery deterministically with tiny values; production defaults
 * below. */
#ifndef MENU_DIRWALK_SORT_SYNC_MAX
#define MENU_DIRWALK_SORT_SYNC_MAX 8192   /* fast path sorts inline up to this */
#endif
#ifndef MENU_DIRWALK_SORT_RUN
#define MENU_DIRWALK_SORT_RUN      2048   /* qsorted run length */
#endif
#ifndef MENU_DIRWALK_SORT_BATCH
#define MENU_DIRWALK_SORT_BATCH    1024   /* merged elements between budget checks */
#endif

enum
{
   MENU_DIRWALK_PHASE_WALK = 0,
   MENU_DIRWALK_PHASE_SORT_RUNS,
   MENU_DIRWALK_PHASE_SORT_MERGE
};

/* Identity of a request.  Strings are owned copies. */
typedef struct
{
   char *dir;
   char *ext;   /* NULL when the request had none */
   unsigned sort_mode;
   unsigned tag;
   bool include_dirs;
   bool include_hidden;
   bool include_compressed;
} menu_dirwalk_ident_t;

/* State a background walk task owns exclusively while it runs.  The
 * handler (worker thread under a threaded queue) touches only this;
 * hand-off back to the module happens in the completion callback,
 * which the task queue always runs on the main thread. */
typedef struct
{
   dir_list_iter_t *iter;           /* NULL for a sort-only task */
   struct string_list *list;
   dir_list_sort_cmp_t cmp;         /* NULL when sort_mode is NONE */
   struct string_list_elem *scratch;
   size_t run_pos;                  /* SORT_RUNS: next run start */
   size_t width;                    /* SORT_MERGE: current run width */
   size_t pair_base;                /* SORT_MERGE: pair being merged */
   size_t a_pos;                    /* SORT_MERGE: intra-merge cursors */
   size_t b_pos;
   size_t o_pos;
   unsigned phase;
   unsigned sort_mode;
   unsigned tag;
   unsigned generation;
   bool src_is_scratch;             /* which buffer holds the runs */
   bool ok;
} menu_dirwalk_task_state_t;

/* The single slot.  Main-thread only. */
static struct
{
   menu_dirwalk_ident_t ident;
   retro_task_t *task;              /* in-flight walk, if any */
   struct string_list *done_list;   /* completed, awaiting pickup */
   void (*refresh_cb)(unsigned tag);
   unsigned generation;             /* stale-completion guard */
   bool have_ident;
   bool ready;
} dirwalk_st;

static void menu_dirwalk_ident_clear(menu_dirwalk_ident_t *ident)
{
   if (ident->dir)
      free(ident->dir);
   if (ident->ext)
      free(ident->ext);
   memset(ident, 0, sizeof(*ident));
}

static bool menu_dirwalk_ident_matches(const menu_dirwalk_ident_t *ident,
      const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      unsigned sort_mode, unsigned tag)
{
   if (   !string_is_equal(ident->dir, dir)
       || (ident->include_dirs       != include_dirs)
       || (ident->include_hidden     != include_hidden)
       || (ident->include_compressed != include_compressed)
       || (ident->sort_mode          != sort_mode)
       || (ident->tag                != tag))
      return false;
   if (!ident->ext)
      return (!ext || !*ext);
   if (!ext)
      return false;
   return string_is_equal(ident->ext, ext);
}

static void menu_dirwalk_sort(struct string_list *list, unsigned sort_mode)
{
   switch (sort_mode)
   {
      case MENU_DIRWALK_SORT_DIR_FIRST:
         dir_list_sort(list, true);
         break;
      case MENU_DIRWALK_SORT_IGNORE_EXT:
         dir_list_sort_ignore_ext(list, true);
         break;
      default:
         break;
   }
}

static dir_list_sort_cmp_t menu_dirwalk_cmp(unsigned sort_mode)
{
   switch (sort_mode)
   {
      case MENU_DIRWALK_SORT_DIR_FIRST:
         return dir_list_sort_cmp(true, false);
      case MENU_DIRWALK_SORT_IGNORE_EXT:
         return dir_list_sort_cmp(true, true);
      default:
         break;
   }
   return NULL;
}

static bool menu_dirwalk_within_budget(void *ud)
{
   return task_nbio_slice_within_budget(ud, 0, 0);
}

/* Enter the budgeted sort.  Allocation failure falls back to the
 * one-shot qsort - a stall is a lesser evil than a lost listing -
 * and reports the phase complete. */
static bool menu_dirwalk_sort_begin(menu_dirwalk_task_state_t *state)
{
   size_t n = state->list->size;

   if (!state->cmp || n <= 1)
      return true;   /* nothing to do */

   if (!(state->scratch = (struct string_list_elem*)malloc(
         n * sizeof(*state->scratch))))
   {
      menu_dirwalk_sort(state->list, state->sort_mode);
      return true;
   }

   state->phase         = MENU_DIRWALK_PHASE_SORT_RUNS;
   state->run_pos       = 0;
   state->width         = 0;
   state->src_is_scratch = false;
   return false;      /* phases remain */
}

/* One budgeted slice of the sort.  Returns true when the listing is
 * fully ordered.  @b NULL removes the budget (threaded queue). */
static bool menu_dirwalk_sort_step(menu_dirwalk_task_state_t *state,
      nbio_budget_t *b)
{
   struct string_list_elem *elems = state->list->elems;
   size_t n                       = state->list->size;

   if (state->phase == MENU_DIRWALK_PHASE_SORT_RUNS)
   {
      /* qsort fixed-length runs in place; each run is bounded work */
      while (state->run_pos < n)
      {
         size_t run = n - state->run_pos;
         if (run > MENU_DIRWALK_SORT_RUN)
            run = MENU_DIRWALK_SORT_RUN;
         qsort(elems + state->run_pos, run, sizeof(*elems), state->cmp);
         state->run_pos += run;
         if (state->run_pos < n && b && !menu_dirwalk_within_budget(b))
            return false;
      }
      if (n <= MENU_DIRWALK_SORT_RUN)
         return true;   /* single run: already fully ordered */
      state->phase     = MENU_DIRWALK_PHASE_SORT_MERGE;
      state->width     = MENU_DIRWALK_SORT_RUN;
      state->pair_base = 0;
      state->a_pos     = 0;
      state->b_pos     = 0;
      state->o_pos     = 0;
   }

   /* Bottom-up merge passes, resumable at element granularity. */
   for (;;)
   {
      struct string_list_elem *src = state->src_is_scratch
            ? state->scratch : elems;
      struct string_list_elem *dst = state->src_is_scratch
            ? elems : state->scratch;

      while (state->pair_base < n)
      {
         size_t a_end = state->pair_base + state->width;
         size_t b_end = state->pair_base + (state->width << 1);
         size_t batch = 0;

         if (a_end > n)
            a_end = n;
         if (b_end > n)
            b_end = n;

         /* Fresh pair: initialise cursors */
         if (state->o_pos == 0 && state->a_pos == 0 && state->b_pos == 0)
         {
            state->a_pos = state->pair_base;
            state->b_pos = a_end;
            state->o_pos = state->pair_base;
         }

         while (state->o_pos < b_end)
         {
            if (   state->a_pos < a_end
                && (   state->b_pos >= b_end
                    || state->cmp(&src[state->a_pos],
                                  &src[state->b_pos]) <= 0))
               dst[state->o_pos++] = src[state->a_pos++];
            else
               dst[state->o_pos++] = src[state->b_pos++];

            if (++batch >= MENU_DIRWALK_SORT_BATCH)
            {
               batch = 0;
               if (b && !menu_dirwalk_within_budget(b))
                  return false;
            }
         }

         state->pair_base = b_end;
         state->a_pos     = 0;
         state->b_pos     = 0;
         state->o_pos     = 0;

         if (state->pair_base < n && b && !menu_dirwalk_within_budget(b))
            return false;
      }

      state->src_is_scratch = !state->src_is_scratch;
      state->width        <<= 1;
      state->pair_base      = 0;

      if (state->width >= n)
      {
         if (state->src_is_scratch)
            memcpy(elems, state->scratch, n * sizeof(*elems));
         return true;
      }

      if (b && !menu_dirwalk_within_budget(b))
         return false;
   }
}

/* ------------------------------------------------------------------ */
/* Background walk task                                               */
/* ------------------------------------------------------------------ */

static void menu_dirwalk_task_handler(retro_task_t *task)
{
   menu_dirwalk_task_state_t *state = NULL;
   int r;

   if (!task)
      return;
   if (!(state = (menu_dirwalk_task_state_t*)task->state))
      return;

   if ((task_get_flags(task) & RETRO_TASK_FLG_CANCELLED) > 0)
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   if (task_queue_is_threaded())
   {
      /* Off the main thread: the shared window exists to protect
       * frame pacing, which a worker cannot hurt.  Run to the end,
       * one-shot sort included. */
      r = state->iter ? dir_list_iter_step(state->iter, NULL, NULL) : 1;
      if (r > 0)
      {
         menu_dirwalk_sort(state->list, state->sort_mode);
         state->ok = true;
      }
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }

   {
      nbio_budget_t b;
      task_nbio_slice_open(&b);

      if (state->phase == MENU_DIRWALK_PHASE_WALK)
      {
         r = state->iter
               ? dir_list_iter_step(state->iter, menu_dirwalk_within_budget, &b)
               : 1;
         if (r == 0)
         {
            task_nbio_slice_close(&b);
            return;   /* yielded; the queue calls the handler again */
         }
         if (r < 0)
         {
            task_nbio_slice_close(&b);
            task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
            return;   /* ok stays false */
         }
         /* Walk complete: the sort may span further invocations. */
         if (menu_dirwalk_sort_begin(state))
         {
            state->ok = true;
            task_nbio_slice_close(&b);
            task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
            return;
         }
      }

      if (menu_dirwalk_sort_step(state, &b))
      {
         state->ok = true;
         task_nbio_slice_close(&b);
         task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
         return;
      }
      task_nbio_slice_close(&b);
   }
}

/* Main thread (task_queue_check runs callbacks there for both queue
 * implementations). */
static void menu_dirwalk_task_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   menu_dirwalk_task_state_t *state = NULL;

   (void)task_data;
   (void)user_data;
   (void)err;

   if (!task)
      return;
   if (!(state = (menu_dirwalk_task_state_t*)task->state))
      return;

   /* A cancelled or superseded walk: the cleanup that follows this
    * callback releases everything; the slot moved on. */
   if (state->generation != dirwalk_st.generation)
      return;

   dirwalk_st.task = NULL;

   if (state->ok)
   {
      dirwalk_st.done_list = state->list;   /* hand-off */
      state->list          = NULL;
      dirwalk_st.ready     = true;
   }
   else
      /* The walk failed mid-flight.  Drop the identity so the
       * refresh-triggered re-request starts over (and can fail fast
       * into the caller's error path). */
      menu_dirwalk_ident_clear(&dirwalk_st.ident);

   if (dirwalk_st.refresh_cb)
      dirwalk_st.refresh_cb(state->tag);
}

static void menu_dirwalk_task_free(retro_task_t *task)
{
   menu_dirwalk_task_state_t *state = NULL;

   if (!task)
      return;
   if (!(state = (menu_dirwalk_task_state_t*)task->state))
      return;

   dir_list_iter_free(state->iter);
   if (state->list)
      string_list_free(state->list);
   if (state->scratch)
      free(state->scratch);
   free(state);
}

static bool menu_dirwalk_task_push(dir_list_iter_t *iter,
      struct string_list *list, unsigned sort_mode, unsigned tag)
{
   retro_task_t *task               = NULL;
   menu_dirwalk_task_state_t *state = NULL;

   task  = task_init();
   state = (menu_dirwalk_task_state_t*)calloc(1, sizeof(*state));

   if (!task || !state)
   {
      if (task)
         free(task);
      if (state)
         free(state);
      return false;
   }

   state->iter       = iter;   /* NULL: sort-only task */
   state->list       = list;
   state->cmp        = menu_dirwalk_cmp(sort_mode);
   state->phase      = MENU_DIRWALK_PHASE_WALK;
   state->sort_mode  = sort_mode;
   state->tag        = tag;
   state->generation = dirwalk_st.generation;

   /* Silent task: no title, no notifications. */
   task->handler     = menu_dirwalk_task_handler;
   task->state       = state;
   task->title       = NULL;
   task->callback    = menu_dirwalk_task_callback;
   task->cleanup     = menu_dirwalk_task_free;

   task_queue_push(task);

   dirwalk_st.task = task;
   return true;
}

/* ------------------------------------------------------------------ */
/* Public API (main thread)                                           */
/* ------------------------------------------------------------------ */

void menu_dirwalk_cancel(void)
{
   /* New generation: a completion already in flight becomes stale
    * and its callback leaves the slot alone. */
   dirwalk_st.generation++;

   if (dirwalk_st.task)
   {
      /* Thread-safe under the threaded queue; the handler notices on
       * its next invocation and finishes, and the task's own cleanup
       * releases the iterator and partial list. */
      task_set_flags(dirwalk_st.task, RETRO_TASK_FLG_CANCELLED, true);
      dirwalk_st.task = NULL;
   }

   if (dirwalk_st.done_list)
   {
      string_list_free(dirwalk_st.done_list);
      dirwalk_st.done_list = NULL;
   }
   dirwalk_st.ready = false;

   menu_dirwalk_ident_clear(&dirwalk_st.ident);
   dirwalk_st.have_ident = false;
}

bool menu_dirwalk_pending(void)
{
   return (dirwalk_st.task != NULL) || dirwalk_st.ready;
}

void menu_dirwalk_set_refresh_cb(void (*cb)(unsigned tag))
{
   dirwalk_st.refresh_cb = cb;
}

enum menu_dirwalk_status menu_dirwalk_request(
      const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      enum menu_dirwalk_sort sort_mode, unsigned tag,
      struct string_list **out_list)
{
   dir_list_iter_t *iter    = NULL;
   struct string_list *list = NULL;
   nbio_budget_t b;
   int r;

   if (!dir || !*dir || !out_list)
      return MENU_DIRWALK_FAILED;

   if (   dirwalk_st.have_ident
       && menu_dirwalk_ident_matches(&dirwalk_st.ident, dir, ext,
            include_dirs, include_hidden, include_compressed,
            (unsigned)sort_mode, tag))
   {
      if (dirwalk_st.ready)
      {
         /* Consume: the identity is spent, so a later visit to the
          * same directory enumerates afresh, exactly as every visit
          * does today. */
         *out_list            = dirwalk_st.done_list;
         dirwalk_st.done_list = NULL;
         dirwalk_st.ready     = false;
         menu_dirwalk_ident_clear(&dirwalk_st.ident);
         dirwalk_st.have_ident = false;
         return MENU_DIRWALK_DONE;
      }
      if (dirwalk_st.task)
         return MENU_DIRWALK_PENDING;
      /* Identity recorded but neither pending nor ready: the task
       * push failed last time.  Fall through and start over. */
   }

   /* Different request (or restart): supersede whatever is there. */
   menu_dirwalk_cancel();

   if (!(list = string_list_new()))
      return MENU_DIRWALK_FAILED;

   /* NULL under the same condition dir_list_new() returns NULL:
    * unreadable directory (or allocation failure).  The caller's
    * existing error entry covers it. */
   if (!(iter = dir_list_iter_new(dir, ext, include_dirs,
         include_hidden, include_compressed, false, list)))
   {
      string_list_free(list);
      return MENU_DIRWALK_FAILED;
   }

   /* Fast path: one budgeted pass, synchronously.  The iterator
    * consults the window between entries with a one-entry floor, so
    * this single call walks as much as the shared window allows -
    * for small and medium directories, all of it. */
   task_nbio_slice_open(&b);
   r = dir_list_iter_step(iter, menu_dirwalk_within_budget, &b);
   task_nbio_slice_close(&b);

   if (r > 0)
   {
      dir_list_iter_free(iter);
      iter = NULL;
      if (   sort_mode == MENU_DIRWALK_SORT_NONE
          || list->size <= MENU_DIRWALK_SORT_SYNC_MAX)
      {
         menu_dirwalk_sort(list, (unsigned)sort_mode);
         *out_list = list;
         return MENU_DIRWALK_DONE;
      }
      /* The walk fit the window but the listing is too large to
       * sort inside it: defer just the sort.  Falls through to the
       * task push below with no iterator. */
   }
   else if (r < 0)
   {
      dir_list_iter_free(iter);
      string_list_free(list);
      return MENU_DIRWALK_FAILED;
   }

   /* Too large for the window: record the identity and hand the
    * remaining work (walk and/or sort) to a task. */
   dirwalk_st.ident.dir                = strldup(dir, strlen(dir) + 1);
   dirwalk_st.ident.ext                = (ext && *ext)
         ? strldup(ext, strlen(ext) + 1) : NULL;
   dirwalk_st.ident.include_dirs       = include_dirs;
   dirwalk_st.ident.include_hidden     = include_hidden;
   dirwalk_st.ident.include_compressed = include_compressed;
   dirwalk_st.ident.sort_mode          = (unsigned)sort_mode;
   dirwalk_st.ident.tag                = tag;
   dirwalk_st.have_ident               = true;

   if (   !dirwalk_st.ident.dir
       || !menu_dirwalk_task_push(iter, list, (unsigned)sort_mode, tag))
   {
      dir_list_iter_free(iter);
      string_list_free(list);
      menu_dirwalk_ident_clear(&dirwalk_st.ident);
      dirwalk_st.have_ident = false;
      return MENU_DIRWALK_FAILED;
   }

   return MENU_DIRWALK_PENDING;
}
