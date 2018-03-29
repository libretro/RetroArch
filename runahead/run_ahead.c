#include "core.h"
#include "dynamic.h"
#include "audio/audio_driver.h"
#include "gfx/video_driver.h"
#include "boolean.h"
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "dirty_input.h"
#include "mylist.h"
#include "secondary_core.h"

static void *runahead_save_state_alloc(void);
static void runahead_save_state_free(void *state);
static void runahead_save_state_list_init(size_t saveStateSize);
static void runahead_save_state_list_destroy(void);
static void runahead_save_state_list_rotate(void);

static void add_hooks(void);
static void remove_hooks(void);
static void deinit_hook(void);
static void unload_hook(void);

static void runahead_clear_variables(void);
static void runahead_check_for_gui(void);
static void runahead_error(void);
static bool runahead_create(void);
static bool runahead_save_state(void);
static bool runahead_load_state(void);
static bool runahead_load_state_secondary(void);
static bool runahead_run_secondary(void);
static void runahead_suspend_audio(void);
static void runahead_resume_audio(void);
static void runahead_suspend_video(void);
static void runahead_resume_video(void);
void run_ahead(int runAheadCount, bool useSecondary);
void runahead_destroy(void);

static size_t runahead_save_state_size = -1;

/* Save State List for Run Ahead */
static MyList *runahead_save_state_list;

static void *runahead_save_state_alloc(void)
{
   retro_ctx_serialize_info_t *savestate;
   savestate = (retro_ctx_serialize_info_t*)malloc(sizeof(retro_ctx_serialize_info_t));
   savestate->size = runahead_save_state_size;
   if (runahead_save_state_size > 0 && runahead_save_state_size != -1)
   {
      savestate->data = malloc(runahead_save_state_size);
      savestate->data_const = savestate->data;
   }
   else
   {
      savestate->data = NULL;
      savestate->data_const = NULL;
      savestate->size = 0;
   }
   return savestate;
}

static void runahead_save_state_free(void *state)
{
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)state;
   if (!savestate)
      return;
   free(savestate->data);
   free(savestate);
}

static void runahead_save_state_list_init(size_t saveStateSize)
{
   runahead_save_state_size = saveStateSize;
   mylist_create(&runahead_save_state_list, 16,
         runahead_save_state_alloc, runahead_save_state_free);
}

static void runahead_save_state_list_destroy(void)
{
   mylist_destroy(&runahead_save_state_list);
}

static void runahead_save_state_list_rotate(void)
{
   int i;
   void *element;
   void *firstElement;
   firstElement = runahead_save_state_list->data[0];
   for (i = 1; i < runahead_save_state_list->size; i++)
   {
      runahead_save_state_list->data[i - 1] = runahead_save_state_list->data[i - 1];
   }
   runahead_save_state_list->data[runahead_save_state_list->size - 1] = firstElement;
}

/* Hooks - Hooks to cleanup, and add dirty input hooks */

static function_t originalRetroDeinit = NULL;
static function_t originalRetroUnload = NULL;

typedef struct retro_core_t _retro_core_t;
extern _retro_core_t current_core;
extern struct retro_callbacks retro_ctx;

static void deinit_hook(void);
static void unload_hook(void);

static void add_hooks(void)
{
   if (!originalRetroDeinit)
   {
      originalRetroDeinit       = current_core.retro_deinit;
      current_core.retro_deinit = deinit_hook;
   }

   if (!originalRetroUnload)
   {
      originalRetroUnload = current_core.retro_unload_game;
      current_core.retro_unload_game = unload_hook;
   }

   add_input_state_hook();
}

static void remove_hooks(void)
{
   if (originalRetroDeinit)
   {
      current_core.retro_deinit = originalRetroDeinit;
      originalRetroDeinit = NULL;
   }

   if (originalRetroUnload)
   {
      current_core.retro_unload_game = originalRetroUnload;
      originalRetroUnload = NULL;
   }
   remove_input_state_hook();
}

static void deinit_hook(void)
{
   remove_hooks();
   runahead_destroy();
   secondary_core_destroy();
   if (current_core.retro_deinit)
   {
      current_core.retro_deinit();
   }
}

static void unload_hook(void)
{
   remove_hooks();
   runahead_destroy();
   secondary_core_destroy();
   if (current_core.retro_unload_game)
   {
      current_core.retro_unload_game();
   }
}

/* Runahead Code */

static bool runahead_video_driver_is_active   = true;
static bool runahead_available                = true;
static bool runahead_secondary_core_available = true;
static bool runahead_force_input_dirty        = true;
static uint64_t runahead_last_frame_count     = 0;

static void runahead_clear_variables(void)
{
   runahead_save_state_size          = -1;
   runahead_video_driver_is_active   = true;
   runahead_available                = true;
   runahead_secondary_core_available = true;
   runahead_force_input_dirty        = true;
   runahead_last_frame_count         = 0;
}

static void runahead_check_for_gui(void)
{
   /* Hack: If we were in the GUI, force a resync. */
   bool is_dirty             = false;
   bool is_alive, is_focused = false;
   uint64_t frame_count      = 0;

   video_driver_get_status(&frame_count, &is_alive, &is_focused);

   if (frame_count != runahead_last_frame_count + 1)
      runahead_force_input_dirty = true;

   runahead_last_frame_count = frame_count;
}

void run_ahead(int runAheadCount, bool useSecondary)
{
   int frameNumber;
   bool okay;
   bool lastFrame;
   bool suspendedFrame;

   if (runAheadCount <= 0 || !runahead_available)
   {
      core_run();
      runahead_force_input_dirty = true;
      return;
   }

   if (runahead_save_state_size == -1)
   {
      if (!runahead_create())
      {
         /* RunAhead has been disabled because the core does not support savestates. */
         core_run();
         runahead_force_input_dirty = true;
         return;
      }
   }

   runahead_check_for_gui();

   if (!useSecondary || !HAVE_DYNAMIC || !runahead_secondary_core_available)
   {
      /* TODO: multiple savestates for higher performance when not using secondary core */
      for (frameNumber = 0; frameNumber <= runAheadCount; frameNumber++)
      {
         lastFrame = frameNumber == runAheadCount;
         suspendedFrame = !lastFrame;
         if (suspendedFrame)
         {
            runahead_suspend_audio();
            runahead_suspend_video();
         }

         if (frameNumber == 0)
            core_run();
         else
            core_run_no_input_polling();

         if (suspendedFrame)
         {
            runahead_resume_video();
            runahead_resume_audio();
         }

         if (frameNumber == 0)
         {
            /* RunAhead has been disabled due to save state failure */
            if (!runahead_save_state())
               return;
         }

         if (lastFrame)
         {
            /* RunAhead has been disabled due to load state failure */
            if (!runahead_load_state())
               return;
         }
      }
   }
   else
   {
#if HAVE_DYNAMIC
      /* run main core with video suspended */
      runahead_suspend_video();
      core_run();
      runahead_resume_video();

      if (input_is_dirty || runahead_force_input_dirty)
      {
         unsigned frame_count;

         input_is_dirty       = false;

         if (!runahead_save_state())
            return;

         /* Could not create a secondary core. 
          * RunAhead wll only use the main core now. */
         if (!runahead_load_state_secondary())
            return;

         for (frame_count = 0; frame_count < runAheadCount - 1; frame_count++)
         {
            runahead_suspend_video();
            runahead_suspend_audio();
            okay = runahead_run_secondary();
            runahead_resume_audio();
            runahead_resume_video();

            /* Could not create a secondary core. RunAhead 
             * will only use the main core now. */
            if (!okay)
               return;
         }
      }
      runahead_suspend_audio();
      okay = runahead_run_secondary();
      runahead_resume_audio();

      /* Could not create a secondary core. RunAhead 
       * will only use the main core now. */
      if (!okay)
         return;
#endif
   }
   runahead_force_input_dirty = false;
}

static void runahead_error(void)
{
   runahead_available = false;
   runahead_save_state_list_destroy();
   remove_hooks();
   runahead_save_state_size = 0;
}

static bool runahead_create(void)
{
   /* get savestate size and allocate buffer */
   retro_ctx_size_info_t info;
   core_serialize_size(&info);

   runahead_save_state_list_init(info.size);
   runahead_video_driver_is_active = video_driver_is_active();

   if (runahead_save_state_size == 0 || runahead_save_state_size == -1)
   {
      runahead_error();
      return false;
   }

   add_hooks();
   runahead_force_input_dirty = true;
   mylist_resize(runahead_save_state_list, 1, true);
   return true;
}

static bool runahead_save_state(void)
{
   retro_ctx_serialize_info_t *serialize_info = 
      (retro_ctx_serialize_info_t*)runahead_save_state_list->data[0];
   bool okay = core_serialize(serialize_info);

   if (!okay)
      runahead_error();
   return okay;
}

static bool runahead_load_state(void)
{
   retro_ctx_serialize_info_t *serialize_info = (retro_ctx_serialize_info_t*)runahead_save_state_list->data[0];
   bool lastDirty = input_is_dirty;
   bool okay      = core_unserialize(serialize_info);
   input_is_dirty = lastDirty;

   if (!okay)
      runahead_error();

   return okay;
}

static bool runahead_load_state_secondary(void)
{
   retro_ctx_serialize_info_t *serialize_info = 
      (retro_ctx_serialize_info_t*)runahead_save_state_list->data[0];
   bool okay = secondary_core_deserialize(serialize_info->data_const, serialize_info->size);

   if (!okay)
      runahead_secondary_core_available = false;

   return okay;
}

static bool runahead_run_secondary(void)
{
   bool okay = secondary_core_run_no_input_polling();
   if (!okay)
      runahead_secondary_core_available = false;
   return okay;
}

static void runahead_suspend_audio(void)
{
   audio_driver_suspend();
}

static void runahead_resume_audio(void)
{
   audio_driver_resume();
}

static void runahead_suspend_video(void)
{
   video_driver_unset_active();
}

static void runahead_resume_video(void)
{
   if (runahead_video_driver_is_active)
      video_driver_set_active();
   else
      video_driver_unset_active();
}

void runahead_destroy(void)
{
   runahead_save_state_list_destroy();
   remove_hooks();
   runahead_clear_variables();
}
