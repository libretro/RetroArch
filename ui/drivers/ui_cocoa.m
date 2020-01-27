/* RetroArch - A frontend for libretro.
 * Copyright (C) 2013-2014 - Jason Fetters
 * Copyright (C) 2011-2017 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <objc/objc-runtime.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <queues/task_queue.h>
#include <retro_timers.h>

#include "cocoa/cocoa_defines.h"
#include "cocoa/cocoa_common.h"
#include "cocoa/apple_platform.h"
#include "../ui_companion_driver.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"
#include "../../frontend/frontend.h"
#include "../../configuration.h"
#include "../../paths.h"
#include "../../core.h"
#include "../../retroarch.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#include ".././verbosity.h"

static void app_terminate(void)
{
   [[NSApplication sharedApplication] terminate:nil];
}

#if defined(HAVE_COCOA_METAL)
@interface RAWindow : NSWindow
@end

@implementation RAWindow
#elif defined(HAVE_COCOA)
@interface RApplication : NSApplication
@end

@implementation RApplication
#endif


- (void)sendEvent:(NSEvent *)event {
   [super sendEvent:event];

   cocoa_input_data_t *apple = NULL;
   NSEventType event_type = event.type;

   switch ((int32_t)event_type)
   {
      case NSEventTypeKeyDown:
      case NSEventTypeKeyUp:
         {
            NSString* ch       = event.characters;
            uint32_t character = 0;
            uint32_t mod       = 0;

            if (ch && ch.length != 0)
            {
               uint32_t i;
               character = [ch characterAtIndex:0];

               if (event.modifierFlags & NSEventModifierFlagCapsLock)
                  mod |= RETROKMOD_CAPSLOCK;
               if (event.modifierFlags & NSEventModifierFlagShift)
                  mod |=  RETROKMOD_SHIFT;
               if (event.modifierFlags & NSEventModifierFlagControl)
                  mod |=  RETROKMOD_CTRL;
               if (event.modifierFlags & NSEventModifierFlagOption)
                  mod |= RETROKMOD_ALT;
               if (event.modifierFlags & NSEventModifierFlagCommand)
                  mod |= RETROKMOD_META;
               if (event.modifierFlags & NSEventModifierFlagNumericPad)
                  mod |=  RETROKMOD_NUMLOCK;

               for (i = 1; i < ch.length; i++)
                  apple_input_keyboard_event(event_type == NSEventTypeKeyDown,
                        0, [ch characterAtIndex:i], mod, RETRO_DEVICE_KEYBOARD);
            }

            apple_input_keyboard_event(event_type == NSEventTypeKeyDown,
                  event.keyCode, character, mod, RETRO_DEVICE_KEYBOARD);
         }
         break;
#if defined(HAVE_COCOA_METAL)
        case NSEventTypeFlagsChanged:
#elif defined(HAVE_COCOA)
        case NSFlagsChanged:
#endif
         {
            static uint32_t old_flags = 0;
            uint32_t new_flags        = event.modifierFlags;
            bool down                 = (new_flags & old_flags) == old_flags;

            old_flags                 = new_flags;

            apple_input_keyboard_event(down, event.keyCode,
                  0, event.modifierFlags, RETRO_DEVICE_KEYBOARD);
         }
         break;
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
	    case NSEventTypeOtherMouseDragged:
         {
            NSPoint pos;
            NSPoint mouse_pos;
            apple                        = (cocoa_input_data_t*)input_driver_get_data();
            if (!apple)
               return;

			pos.x              = 0;
			pos.y              = 0;

            /* Relative */
            apple->mouse_rel_x = (int16_t)event.deltaX;
            apple->mouse_rel_y = (int16_t)event.deltaY;

            /* Absolute */
#if defined(HAVE_COCOA_METAL)
            pos = [apple_platform.renderView convertPoint:[event locationInWindow] fromView:nil];
#elif defined(HAVE_COCOA)
#endif
            apple->touches[0].screen_x = (int16_t)pos.x;
            apple->touches[0].screen_y = (int16_t)pos.y;

#if defined(HAVE_COCOA_METAL)
            mouse_pos = [apple_platform.renderView convertPoint:[event locationInWindow]  fromView:nil];
#elif defined(HAVE_COCOA)
            mouse_pos = [[CocoaView get] convertPoint:[event locationInWindow]  fromView:nil];
#endif
            apple->window_pos_x = (int16_t)mouse_pos.x;
            apple->window_pos_y = (int16_t)mouse_pos.y;
         }
         break;
#if defined(HAVE_COCOA_METAL)
        case NSEventTypeScrollWheel:
#elif defined(HAVE_COCOA)
        case NSScrollWheel:
#endif
         /* TODO/FIXME - properly implement. */
         break;
       case NSEventTypeLeftMouseDown:
       case NSEventTypeRightMouseDown:
       case NSEventTypeOtherMouseDown:
       {
#ifdef HAVE_COCOA_METAL
           NSPoint pos = [apple_platform.renderView convertPoint:[event locationInWindow] fromView:nil];
#else
           NSPoint pos = [[CocoaView get] convertPoint:[event locationInWindow] fromView:nil];
#endif
           apple = (cocoa_input_data_t*)input_driver_get_data();
           if (!apple || pos.y < 0)
               return;
           apple->mouse_buttons |= (1 << event.buttonNumber);
       }
           break;
      case NSEventTypeLeftMouseUp:
      case NSEventTypeRightMouseUp:
      case NSEventTypeOtherMouseUp:
         {
#ifdef HAVE_COCOA_METAL
            NSPoint pos = [apple_platform.renderView convertPoint:[event locationInWindow] fromView:nil];
#else
            NSPoint pos = [[CocoaView get] convertPoint:[event locationInWindow] fromView:nil];
#endif
            apple = (cocoa_input_data_t*)input_driver_get_data();
            if (!apple || pos.y < 0)
               return;
            apple->mouse_buttons &= ~(1 << event.buttonNumber);
            apple->touch_count = 0;
         }
         break;
      default:
         break;
   }
}

@end

static int waiting_argc;
static char** waiting_argv;

@implementation RetroArch_OSX

@synthesize window = _window;

#ifdef HAVE_COCOA_METAL
#else
#define NS_WINDOW_COLLECTION_BEHAVIOR_FULLSCREEN_PRIMARY (1 << 17)

- (void)dealloc
{
   [_window release];
   [super dealloc];
}
#endif

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   unsigned i;
#ifdef HAVE_COCOA_METAL
   apple_platform   = self;

   self.window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;

   _listener = [WindowListener new];

   [self.window setAcceptsMouseMovedEvents: YES];
   [self.window setNextResponder:_listener];
   self.window.delegate = _listener;

   [[self.window contentView] setAutoresizesSubviews:YES];
#else
   SEL selector     = NSSelectorFromString(BOXSTRING("setCollectionBehavior:"));
   SEL fsselector   = NSSelectorFromString(BOXSTRING("toggleFullScreen:"));
   apple_platform   = self;

   if ([self.window respondsToSelector:selector])
   {
       if ([self.window respondsToSelector:fsselector])
          [self.window setCollectionBehavior:NS_WINDOW_COLLECTION_BEHAVIOR_FULLSCREEN_PRIMARY];
   }

   [self.window setAcceptsMouseMovedEvents: YES];

   [[CocoaView get] setFrame: [[self.window contentView] bounds]];
   [[self.window contentView] setAutoresizesSubviews:YES];
   [[self.window contentView] addSubview:[CocoaView get]];
   [self.window makeFirstResponder:[CocoaView get]];
#endif

    for (i = 0; i < waiting_argc; i++)
    {
        if (string_is_equal(waiting_argv[i], "-NSDocumentRevisionsDebugMode"))
        {
            waiting_argv[i]   = NULL;
            waiting_argv[i+1] = NULL;
            waiting_argc -= 2;
        }
    }
   if (rarch_main(waiting_argc, waiting_argv, NULL))
      app_terminate();

   waiting_argc = 0;

#ifdef HAVE_COCOA_METAL
   [self.window makeMainWindow];
   [self.window makeKeyWindow];
#endif

   [self performSelectorOnMainThread:@selector(rarch_main) withObject:nil waitUntilDone:NO];
}

#pragma mark - ApplePlatform

#ifdef HAVE_COCOA_METAL
- (void)setViewType:(apple_view_type_t)vt {
   if (vt == _vt) {
      return;
   }

   RARCH_LOG("[Cocoa]: change view type: %d ? %d\n", _vt, vt);

   _vt = vt;
   if (_renderView != nil)
   {
      _renderView.wantsLayer = NO;
      _renderView.layer = nil;
      [_renderView removeFromSuperview];
      self.window.contentView = nil;
      _renderView = nil;
   }

   switch (vt) {
      case APPLE_VIEW_TYPE_VULKAN:
      case APPLE_VIEW_TYPE_METAL:
      {
         MetalView *v = [MetalView new];
         v.paused = YES;
         v.enableSetNeedsDisplay = NO;
         _renderView = v;
      }
      break;

      case APPLE_VIEW_TYPE_OPENGL:
      {
         _renderView = [CocoaView get];
         break;
      }

      case APPLE_VIEW_TYPE_NONE:
      default:
         return;
   }

   _renderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
   [_renderView setFrame: [[self.window contentView] bounds]];

   self.window.contentView = _renderView;
   self.window.contentView.nextResponder = _listener;
}

- (apple_view_type_t)viewType {
   return _vt;
}

- (id)renderView {
   return _renderView;
}

- (bool)hasFocus {
   return [NSApp isActive];
}

- (void)setVideoMode:(gfx_ctx_mode_t)mode {
   BOOL isFullScreen = (self.window.styleMask & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
   if (mode.fullscreen && !isFullScreen)
   {
      [self.window toggleFullScreen:self];
      return;
   }

   if (!mode.fullscreen && isFullScreen)
   {
      [self.window toggleFullScreen:self];
   }

   if (mode.width > 0)
   {
      // HACK(sgc): ensure MTKView posts a drawable resize event
      [self.window setContentSize:NSMakeSize(mode.width-1, mode.height)];
   }
   [self.window setContentSize:NSMakeSize(mode.width, mode.height)];
}

- (void)setCursorVisible:(bool)v {
   if (v)
      [NSCursor unhide];
   else
      [NSCursor hide];
}

- (bool)setDisableDisplaySleep:(bool)disable
{
   if (disable && _sleepActivity == nil)
   {
      _sleepActivity = [NSProcessInfo.processInfo beginActivityWithOptions:NSActivityIdleDisplaySleepDisabled reason:@"disable screen saver"];
   }
   else if (!disable && _sleepActivity != nil)
   {
      [NSProcessInfo.processInfo endActivity:_sleepActivity];
      _sleepActivity = nil;
   }
   return YES;
}
#endif

- (void) rarch_main
{
    do
    {
       int ret;
#ifdef HAVE_QT
       const ui_application_t *application = &ui_application_qt;
#else
       const ui_application_t *application = &ui_application_cocoa;
#endif
       if (application)
          application->process_events();

       ret = runloop_iterate();

       task_queue_check();

       while(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, FALSE) == kCFRunLoopRunHandledSource);
       if (ret == -1)
       {
#ifdef HAVE_QT
          ui_application_qt.quit();
#endif
          break;
       }
    }while(1);

    main_exit(NULL);
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
   return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
   NSApplicationTerminateReply reply = NSTerminateNow;

   if (rarch_ctl(RARCH_CTL_IS_INITED, NULL))
      reply = NSTerminateCancel;

   command_event(CMD_EVENT_QUIT, NULL);

   return reply;
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
	if ((filenames.count == 1) && [filenames objectAtIndex:0])
   {
      struct retro_system_info *system = runloop_get_libretro_system_info();
	  NSString *__core                 = [filenames objectAtIndex:0];
      const char *core_name            = system->library_name;

      if (core_name)
      {
         content_ctx_info_t content_info = {0};
         task_push_load_content_with_current_core_from_companion_ui(
               __core.UTF8String,
               &content_info,
               CORE_TYPE_PLAIN,
               NULL, NULL);
      }
      else
         path_set(RARCH_PATH_CONTENT, __core.UTF8String);

      [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
   }
   else
   {
      const ui_msg_window_t *msg_window = ui_companion_driver_get_msg_window_ptr();
      if (msg_window)
      {
         ui_msg_window_state msg_window_state;
         msg_window_state.text  = strdup("Cannot open multiple files");
         msg_window_state.title = strdup(msg_hash_to_str(MSG_PROGRAM));
         msg_window->information(&msg_window_state);

         free(msg_window_state.text);
         free(msg_window_state.title);
      }
      [sender replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
   }
}

static void open_core_handler(ui_browser_window_state_t *state, bool result)
{
   rarch_system_info_t *info      = runloop_get_system_info();
    if (!state)
        return;
    if (string_is_empty(state->result))
        return;
    if (!result)
        return;

    settings_t *settings = config_get_ptr();

    path_set(RARCH_PATH_CORE, state->result);
    ui_companion_event_command(CMD_EVENT_LOAD_CORE);

    if (info && info->load_no_content
          && settings->bools.set_supports_no_game_enable)
    {
        content_ctx_info_t content_info = {0};
        path_clear(RARCH_PATH_CONTENT);
        task_push_load_content_with_current_core_from_companion_ui(
                NULL,
                &content_info,
                CORE_TYPE_PLAIN,
                NULL, NULL);
    }
}

static void open_document_handler(ui_browser_window_state_t *state, bool result)
{
    if (!state)
        return;
    if (string_is_empty(state->result))
        return;
    if (!result)
        return;

    struct retro_system_info *system = runloop_get_libretro_system_info();
    const char            *core_name = system ? system->library_name : NULL;

    path_set(RARCH_PATH_CONTENT, state->result);

    if (core_name)
    {
        content_ctx_info_t content_info = {0};
        task_push_load_content_with_current_core_from_companion_ui(
                NULL,
                &content_info,
                CORE_TYPE_PLAIN,
                NULL, NULL);
    }
}

- (IBAction)openCore:(id)sender {
    const ui_browser_window_t *browser = ui_companion_driver_get_browser_window_ptr();

    if (browser)
    {
        ui_browser_window_state_t browser_state;
        settings_t *settings        = config_get_ptr();

        browser_state.filters       = strdup("dylib");
        browser_state.filters_title = strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS));
        browser_state.title         = strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST));
        browser_state.startdir      = strdup(settings->paths.directory_libretro);

        bool result = browser->open(&browser_state);
        open_core_handler(&browser_state, result);

        free(browser_state.filters);
        free(browser_state.filters_title);
        free(browser_state.title);
        free(browser_state.startdir);
    }
}

- (void)openDocument:(id)sender
{
   const ui_browser_window_t *browser = ui_companion_driver_get_browser_window_ptr();

    if (browser)
    {
        ui_browser_window_state_t browser_state = {{0}};
        settings_t *settings  = config_get_ptr();
        NSString *startdir    = BOXSTRING(settings->paths.directory_menu_content);

        if (!startdir.length)
            startdir           = BOXSTRING("/");

        browser_state.title    = strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST));
        browser_state.startdir = strdup([startdir UTF8String]);

        bool result = browser->open(&browser_state);
        open_document_handler(&browser_state, result);

        free(browser_state.startdir);
        free(browser_state.title);
    }
}

- (void)unloadingCore
{
}

- (IBAction)showCoresDirectory:(id)sender
{
   settings_t *settings = config_get_ptr();
   [[NSWorkspace sharedWorkspace] openFile:BOXSTRING(settings->paths.directory_libretro)];
}

- (IBAction)showPreferences:(id)sender
{
}

- (IBAction)basicEvent:(id)sender
{
   enum event_command cmd;
   unsigned sender_tag = (unsigned)[sender tag];

   switch (sender_tag)
   {
      case 1:
         cmd = CMD_EVENT_RESET;
         break;
      case 2:
         cmd = CMD_EVENT_LOAD_STATE;
         break;
      case 3:
         cmd = CMD_EVENT_SAVE_STATE;
         break;
      case 4:
         cmd = CMD_EVENT_DISK_EJECT_TOGGLE;
         break;
      case 5:
         cmd = CMD_EVENT_DISK_PREV;
         break;
      case 6:
         cmd = CMD_EVENT_DISK_NEXT;
         break;
      case 7:
         cmd = CMD_EVENT_GRAB_MOUSE_TOGGLE;
         break;
      case 8:
         cmd = CMD_EVENT_MENU_TOGGLE;
         break;
      case 9:
         cmd = CMD_EVENT_PAUSE_TOGGLE;
         break;
      case 20:
         cmd = CMD_EVENT_FULLSCREEN_TOGGLE;
         break;
      default:
         cmd = CMD_EVENT_NONE;
         break;
   }

   if (sender_tag >= 10 && sender_tag <= 19)
   {
      unsigned idx = (sender_tag - (10-1));
      rarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);
      cmd = CMD_EVENT_RESIZE_WINDOWED_SCALE;
   }

   ui_companion_event_command(cmd);
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(int32_t)returnCode contextInfo:(void *)contextInfo
{
   [[NSApplication sharedApplication] stopModal];
}

@end

int main(int argc, char *argv[])
{
   if (argc == 2)
   {
       if (argv[1] != '\0')
           if (!strncmp(argv[1], "-psn", 4))
               argc = 1;
   }

   waiting_argc = argc;
   waiting_argv = argv;

   return NSApplicationMain(argc, (const char **) argv);
}

typedef struct ui_companion_cocoa
{
   void *empty;
} ui_companion_cocoa_t;

static void ui_companion_cocoa_notify_content_loaded(void *data)
{
    (void)data;
}

static void ui_companion_cocoa_toggle(void *data, bool force)
{
   (void)data;
   (void)force;
}

static void ui_companion_cocoa_deinit(void *data)
{
   ui_companion_cocoa_t *handle = (ui_companion_cocoa_t*)data;

   app_terminate();

   if (handle)
      free(handle);
}

static void *ui_companion_cocoa_init(void)
{
   ui_companion_cocoa_t *handle = (ui_companion_cocoa_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   return handle;
}

static void ui_companion_cocoa_event_command(void *data, enum event_command cmd)
{
   (void)data;
   (void)cmd;
}

static void ui_companion_cocoa_notify_list_pushed(void *data,
    file_list_t *list, file_list_t *menu_list)
{
    (void)data;
    (void)list;
    (void)menu_list;
}

static void *ui_companion_cocoa_get_main_window(void *data)
{
    return (BRIDGE void *)((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]).window;
}

ui_companion_driver_t ui_companion_cocoa = {
   ui_companion_cocoa_init,
   ui_companion_cocoa_deinit,
   ui_companion_cocoa_toggle,
   ui_companion_cocoa_event_command,
   ui_companion_cocoa_notify_content_loaded,
   ui_companion_cocoa_notify_list_pushed,
   NULL, /* notify_refresh */
   NULL, /* msg_queue_push */
   NULL, /* render_messagebox */
   ui_companion_cocoa_get_main_window,
   NULL, /* log_msg */
   &ui_browser_window_cocoa,
   &ui_msg_window_cocoa,
   &ui_window_cocoa,
   &ui_application_cocoa,
   "cocoa",
};
