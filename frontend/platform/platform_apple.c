/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
 * Copyright (C) 2012-2014 - Jason Fetters
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

#include "../menu/menu_common.h"
#include "../../settings_data.h"

#include "../frontend.h"

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>

/* Forward declarations */
void apple_start_iteration(void);
void apple_stop_iteration(void);
void apple_content_loaded(const char *, const char *);

static CFRunLoopObserverRef iterate_observer;

static void do_iteration(void)
{
   int ret = main_entry_decide(0, NULL, NULL);

   if (ret == -1)
   {
      main_exit(NULL);
      return;
   }

   if (ret == 0) 
      CFRunLoopWakeUp(CFRunLoopGetMain());

   /* TODO/FIXME
      I am almost positive that this is not necessary and is actually a
      bad thing.

      1st. Why it is bad thing.

      This wakes up the main event loop immediately and the main loop
      has only one observer, which is this function. In other words,
      this causes the function to be called immediately. I did an
      experiment where I saved the time before calling this and then
      reported the difference between it and the start of
      do_iteration, and as expected it was about 0. As a result, when
      we remove this, idle performance (i.e. displaying the RetroArch
      menu) is 0% CPU as desired.

      2nd. Why it is not necessary.

      The event loop will wake up itself when there is input to the
      process. This includes touch events, keyboard, bluetooth,
      etc. Thus, it will be woken up and without any intervention so
      that we can process that event.

      Nota bene. Why this analysis might be wrong (and what to do about it).

      If RA is not idle and is running a core, then I believe it is
      designed to expect to be called in a busy loop like this because
      it implements its own frame timer to ensure that the emulation
      simulation isn't too fast. In that case, this change would only
      allow emulation to run when there was input, which would make
      all games turn-based. :)

      There are two good ways to fix this and still have the desired
      0% CPU idle behavior.

      Approach 1: Change main_entry_decide from returning a boolean
      (two-values) that are interpreted as CONTINUE and QUIT. Into
      returning a char-sized enum with three values that are
      interpreted as QUIT, WAIT, and AGAIN, such that QUIT calls
      main_exit, WAIT doesn't wake up the loop, and AGAIN does. It
      would then return AGAIN when a core was active. An ugly way to
      get the same effect is to look have this code just look at
      g_extern.is_menu and use the WAIT behavior in that case.

      Approach 2: Instead of signalling outside of RA whether a core
      is running, instead externalize the frame time that is inside
      retroarch. change main_entry_decide to return a value in
      [-1,MAX_INT] where -1 is interpreted as QUIT, [0,MAX_INT) is
      interpreted as the amount of time to wait until continuing, and
      MAX_INT is interpreted as WAIT. This could be more robust
      because we'd be exposing the scheduling behavior of RA to iOS,
      which might be good in other platforms as well.

      Approach 1 is the simplest and essentially just pushes down
      these requirements to rarch_main_iterate. I have gone with the
      "ugly way" first because it is the most expedient and
      safe. Other eyeballs should decide if it isn't necessary.
      */
}

void apple_start_iteration(void)
{
   iterate_observer = CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting,
         true, 0, (CFRunLoopObserverCallBack)do_iteration, 0);
   CFRunLoopAddObserver(CFRunLoopGetMain(), iterate_observer,
         kCFRunLoopCommonModes);
}

void apple_stop_iteration(void)
{
   CFRunLoopObserverInvalidate(iterate_observer);
   CFRelease(iterate_observer);
   iterate_observer = 0;
}

static void frontend_apple_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{

   char bundle_path_buf[PATH_MAX + 1], home_dir_buf[PATH_MAX + 1];
   CFURLRef bundle_url;
   CFBundleRef bundle = CFBundleGetMainBundle();
   if (!bundle)
      return;
   bundle_url = CFBundleCopyBundleURL(bundle);
   CFStringRef bundle_path = CFURLCopyPath(bundle_url);
    
   (void)bundle_path_buf;
   (void)home_dir_buf;
   
#ifdef IOS
   CFURLRef home_dir = CFCopyHomeDirectoryURL();
   int doc_len = 10; /* Length of "/Documents" */
   int pre_len = 7;  /* Length of "file://" */
   int dd_len = CFURLGetBytes(home_dir, NULL, 0 );
   UInt8 *dd_bs = (UInt8 *)(malloc(dd_len - pre_len + doc_len));
   CFURLGetFileSystemRepresentation(home_dir, true, dd_bs, dd_len - pre_len);
   /* We subtract another 1 to get the NUL */
   strlcpy((char *)(dd_bs + dd_len - pre_len - 1), "/Documents", doc_len + 1);
    
   CFStringRef home_dir_ref = CFURLCopyPath(home_dir);

   CFStringGetCString(home_dir_ref,    home_dir_buf,    sizeof(home_dir_buf),    kCFStringEncodingUTF8);
   CFStringGetCString(bundle_path, bundle_path_buf, sizeof(bundle_path_buf), kCFStringEncodingUTF8);

   fill_pathname_join(g_defaults.system_dir, home_dir_buf, ".RetroArch", sizeof(g_defaults.system_dir));
   fill_pathname_join(g_defaults.core_dir, bundle_path_buf, "modules", sizeof(g_defaults.core_dir));

   strlcpy(g_defaults.menu_config_dir, g_defaults.system_dir, sizeof(g_defaults.menu_config_dir));
   fill_pathname_join(g_defaults.config_path, g_defaults.menu_config_dir, "retroarch.cfg", sizeof(g_defaults.config_path));

   strlcpy(g_defaults.sram_dir, g_defaults.system_dir, sizeof(g_defaults.sram_dir));
   strlcpy(g_defaults.savestate_dir, g_defaults.system_dir, sizeof(g_defaults.savestate_dir));

   path_mkdir(bundle_path_buf);

   if (access(bundle_path_buf, 0755) != 0)
      RARCH_ERR("Failed to create or access base directory: %s\n", bundle_path_buf);
   else
   {
      path_mkdir(g_defaults.system_dir);

      if (access(g_defaults.system_dir, 0755) != 0)
         RARCH_ERR("Failed to create or access system directory: %s.\n", g_defaults.system_dir);
   }

   CFRelease(home_dir);
#endif

   CFRelease(bundle_path);
   CFRelease(bundle_url);
}

extern void apple_rarch_exited(void);

static void frontend_apple_content_loaded(void)
{
    apple_content_loaded(g_settings.libretro, g_extern.fullpath);
}

static void frontend_apple_shutdown(bool unused)
{
    apple_rarch_exited();
}

static int frontend_apple_get_rating(void)
{
   /* TODO/FIXME - look at unique identifier per device and 
    * determine rating for some */
   return -1;
}
const frontend_ctx_driver_t frontend_ctx_apple = {
   frontend_apple_get_environment_settings, /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_apple_shutdown,      /* shutdown */
   NULL,                         /* get_name */
   frontend_apple_get_rating,    /* get_rating */
   frontend_apple_content_loaded,  /* load_content */
   "apple",
};
