/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "cocoa/cocoa_common.h"
#include "../ui_companion_driver.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"
#include "../../frontend/frontend.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../system.h"
#include "../../tasks/tasks.h"

static id apple_platform;

void apple_rarch_exited(void)
{
   [[NSApplication sharedApplication] terminate:nil];
}

@interface RApplication : NSApplication
@end

@implementation RApplication

- (void)sendEvent:(NSEvent *)event
{
   NSEventType event_type;
   cocoa_input_data_t *apple = NULL;
   [super sendEvent:event];

   event_type = event.type;

   switch ((int32_t)event_type)
   {
      case NSKeyDown:
        case NSKeyUp:
         {
            NSString* ch = (NSString*)event.characters;
            uint32_t character = 0;
            uint32_t mod = 0;

            if (ch && ch.length != 0)
            {
               uint32_t i;
               character = [ch characterAtIndex:0];

               if (event.modifierFlags & NSAlphaShiftKeyMask)
                  mod |= RETROKMOD_CAPSLOCK;
               if (event.modifierFlags & NSShiftKeyMask)
                  mod |=  RETROKMOD_SHIFT;
               if (event.modifierFlags & NSControlKeyMask)
                  mod |=  RETROKMOD_CTRL;
               if (event.modifierFlags & NSAlternateKeyMask)
                  mod |= RETROKMOD_ALT;
               if (event.modifierFlags & NSCommandKeyMask)
                  mod |= RETROKMOD_META;
               if (event.modifierFlags & NSNumericPadKeyMask)
                  mod |=  RETROKMOD_NUMLOCK;

               for (i = 1; i < ch.length; i++)
                  apple_input_keyboard_event(event_type == NSKeyDown,
                        0, [ch characterAtIndex:i], mod, RETRO_DEVICE_KEYBOARD);
            }

            apple_input_keyboard_event(event_type == NSKeyDown,
                  event.keyCode, character, mod, RETRO_DEVICE_KEYBOARD);
         }
         break;
        case NSFlagsChanged:
         {
            static uint32_t old_flags = 0;
            uint32_t new_flags = event.modifierFlags;
            bool down = (new_flags & old_flags) == old_flags;
            old_flags = new_flags;

            apple_input_keyboard_event(down, event.keyCode,
                  0, event.modifierFlags, RETRO_DEVICE_KEYBOARD);
         }
         break;
        case NSMouseMoved:
  case NSLeftMouseDragged:
 case NSRightMouseDragged:
 case NSOtherMouseDragged:
         {
            NSPoint pos;
            NSPoint mouse_pos;
            apple                        = (cocoa_input_data_t*)input_driver_get_data();
            if (!apple)
               return;

            /* Relative */
            apple->mouse_rel_x = event.deltaX;
            apple->mouse_rel_y = event.deltaY;

            /* Absolute */
            pos = [[CocoaView get] convertPoint:[event locationInWindow] fromView:nil];
            apple->touches[0].screen_x = pos.x;
            apple->touches[0].screen_y = pos.y;

            mouse_pos = [[CocoaView get] convertPoint:[event locationInWindow]  fromView:nil];
            apple->window_pos_x = (int16_t)mouse_pos.x;
            apple->window_pos_y = (int16_t)mouse_pos.y;
         }
         break;
 case NSScrollWheel:
         /* TODO/FIXME - properly implement. */
         break;
 case NSLeftMouseDown:
case NSRightMouseDown:
case NSOtherMouseDown:
         apple = (cocoa_input_data_t*)input_driver_get_data();
         if (!apple)
            return;
         apple->mouse_buttons |= 1 << event.buttonNumber;
         apple->touch_count = 1;
         break;
case NSLeftMouseUp:
      case NSRightMouseUp:
      case NSOtherMouseUp:
         apple = (cocoa_input_data_t*)input_driver_get_data();
         if (!apple)
            return;
         apple->mouse_buttons &= ~(1 << event.buttonNumber);
         apple->touch_count = 0;
         break;
   }
}

@end

static int waiting_argc;
static char** waiting_argv;

@implementation RetroArch_OSX

@synthesize window = _window;

- (void)dealloc
{
   [_window release];
   [super dealloc];
}

#define NS_WINDOW_COLLECTION_BEHAVIOR_FULLSCREEN_PRIMARY (1 << 17)

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   unsigned i;
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
      apple_rarch_exited();

   waiting_argc = 0;

   [self performSelectorOnMainThread:@selector(rarch_main) withObject:nil waitUntilDone:NO];
}

static void poll_iteration(void)
{
    NSEvent *event = NULL;
    
    do
    {
        event = [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
        
        [NSApp sendEvent: event];
    }while(event != nil);
}

- (void) rarch_main
{
    int ret = 0;
    while (ret != -1)
    {
       unsigned sleep_ms = 0;
       poll_iteration();
       ret = runloop_iterate(&sleep_ms);
       if (ret == 1 && sleep_ms > 0)
          retro_sleep(sleep_ms);
       runloop_ctl(RUNLOOP_CTL_DATA_ITERATE, NULL);
       while(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, FALSE) == kCFRunLoopRunHandledSource);
    }
    
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

   ui_companion_event_command(EVENT_CMD_QUIT);

   return reply;
}


extern void action_ok_push_quick_menu(void);

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if (filenames.count == 1 && [filenames objectAtIndex:0])
   {
      struct retro_system_info         *system = NULL;
      NSString *__core = [filenames objectAtIndex:0];
      const char *core_name = NULL;

      menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET, &system);
      
      if (system)
         core_name = system->library_name;
		
      runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)__core.UTF8String);

      if (core_name)
         ui_companion_event_command(EVENT_CMD_LOAD_CONTENT);

      [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
   }
   else
   {
      apple_display_alert("Cannot open multiple files", "RetroArch");
      [sender replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
   }
}

static void open_core_handler(NSOpenPanel *panel, NSInteger result)
{
    switch (result)
    {
        case NSOKButton:
            if (panel.URL)
            {
                settings_t *settings = config_get_ptr();
                NSURL *url           = (NSURL*)panel.URL;
                NSString *__core     = url.path;
                
                if (!__core)
                    return;
                
                runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)__core.UTF8String);
                ui_companion_event_command(EVENT_CMD_LOAD_CORE);
                
                if (menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL) && settings->set_supports_no_game_enable)
                {
                    runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
                    if (rarch_task_push_content_load_default(
                             NULL, NULL, false, CORE_TYPE_PLAIN,
                             NULL, NULL))
                        action_ok_push_quick_menu();
                }
            }
            break;
        case NSCancelButton:
            break;
    }
}

static void open_document_handler(NSOpenPanel *panel, NSInteger result)
{
    switch (result)
    {
        case NSOKButton:
            if (panel.URL)
            {
                struct retro_system_info *system = NULL;
                NSURL                       *url = (NSURL*)panel.URL;
                NSString                 *__core = url.path;
                const char            *core_name = NULL;
                
                menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET, &system);
                
                if (system)
                    core_name = system->library_name;
                
                runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)__core.UTF8String);
                
                if (core_name)
                    ui_companion_event_command(EVENT_CMD_LOAD_CONTENT);
            }
            break;
        case NSCancelButton:
            break;
    }
}

- (IBAction)openCore:(id)sender {
    NSOpenPanel* panel = (NSOpenPanel*)[NSOpenPanel openPanel];
    settings_t *settings = config_get_ptr();
    NSString *startdir   = BOXSTRING(settings->libretro_directory);
	NSArray *filetypes   = [[NSArray alloc] initWithObjects:BOXSTRING("dylib"), BOXSTRING("Core"), nil];
	[panel setAllowedFileTypes:filetypes];
#if defined(MAC_OS_X_VERSION_10_6)
    [panel setMessage:BOXSTRING("Load Core")];
    [panel setDirectoryURL:[NSURL fileURLWithPath:startdir]];
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result)
     {
         [[NSApplication sharedApplication] stopModal];
         open_core_handler(panel, result);
     }];
    [[NSApplication sharedApplication] runModalForWindow:panel];
#else
	[panel setTitle:NSLocalizedString(BOXSTRING("Load Core"), BOXSTRING("open panel"))];
	[panel setDirectory:startdir];
	[panel setCanChooseDirectories:NO];
	[panel setCanChooseFiles:YES];
	[panel setAllowsMultipleSelection:NO];
	[panel setTreatsFilePackagesAsDirectories:NO];
	NSInteger result = [panel runModal];
	if (result == 1)
       open_core_handler(panel, result);
#endif
    [g_context makeCurrentContext];
}

- (void)openDocument:(id)sender
{
   NSOpenPanel* panel    = (NSOpenPanel*)[NSOpenPanel openPanel];
   settings_t *settings  = config_get_ptr();
   NSString *startdir    = BOXSTRING(settings->menu_content_directory);
    
   if (!startdir.length)
      startdir           = BOXSTRING("/");
#if defined(MAC_OS_X_VERSION_10_6)
   [panel setMessage:BOXSTRING("Load Content")];
   [panel setDirectoryURL:[NSURL fileURLWithPath:startdir]];
   [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result)
   {
      [[NSApplication sharedApplication] stopModal];
      open_document_handler(panel, result);
   }];
   [[NSApplication sharedApplication] runModalForWindow:panel];
#else
    [panel setTitle:NSLocalizedString(BOXSTRING("Load Content"), BOXSTRING("open panel"))];
    [panel setDirectory:startdir];
    [panel setCanChooseDirectories:NO];
    [panel setCanChooseFiles:YES];
    [panel setAllowsMultipleSelection:NO];
    [panel setTreatsFilePackagesAsDirectories:NO];
    NSInteger result = [panel runModal];
    if (result == 1)
        open_document_handler(panel, result);
#endif
    [g_context makeCurrentContext];
}

- (void)unloadingCore
{
}

- (IBAction)showCoresDirectory:(id)sender
{
   settings_t *settings = config_get_ptr();
   [[NSWorkspace sharedWorkspace] openFile:BOXSTRING(settings->libretro_directory)];
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
         cmd = EVENT_CMD_RESET;
         break;
      case 2:
         cmd = EVENT_CMD_LOAD_STATE;
         break;
      case 3:
         cmd = EVENT_CMD_SAVE_STATE;
         break;
      case 4:
         cmd = EVENT_CMD_DISK_EJECT_TOGGLE;
         break;
      case 5:
         cmd = EVENT_CMD_DISK_PREV;
         break;
      case 6:
         cmd = EVENT_CMD_DISK_NEXT;
         break;
      case 7:
         cmd = EVENT_CMD_GRAB_MOUSE_TOGGLE;
         break;
      case 8:
         cmd = EVENT_CMD_MENU_TOGGLE;
         break;
      case 9:
         cmd = EVENT_CMD_PAUSE_TOGGLE;
         break;
      case 20:
         cmd = EVENT_CMD_FULLSCREEN_TOGGLE;
         break;
      default:
         cmd = EVENT_CMD_NONE;
         break;
   }
   
   if (sender_tag >= 10 && sender_tag <= 19)
   {
      unsigned idx = (sender_tag - (10-1));
      runloop_ctl(RUNLOOP_CTL_SET_WINDOWED_SCALE, &idx);
      cmd = EVENT_CMD_RESIZE_WINDOWED_SCALE;
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

void apple_display_alert(const char *message, const char *title)
{
    NSAlert* alert = [[NSAlert new] autorelease];
    
    [alert setMessageText:(*title) ? BOXSTRING(title) : BOXSTRING("RetroArch")];
    [alert setInformativeText:BOXSTRING(message)];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert beginSheetModalForWindow:((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]).window
                      modalDelegate:apple_platform
                     didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
    [[NSApplication sharedApplication] runModalForWindow:[alert window]];
}

typedef struct ui_companion_cocoa
{
   void *empty;
} ui_companion_cocoa_t;

static void ui_companion_cocoa_notify_content_loaded(void *data)
{
    (void)data;
}

static void ui_companion_cocoa_toggle(void *data)
{
   (void)data;
}

static int ui_companion_cocoa_iterate(void *data, unsigned action)
{
   (void)data;

   return 0;
}

static void ui_companion_cocoa_deinit(void *data)
{
   ui_companion_cocoa_t *handle = (ui_companion_cocoa_t*)data;

   apple_rarch_exited();

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
   event_cmd_ctl(cmd, NULL);
}

static void ui_companion_cocoa_notify_list_pushed(void *data,
    file_list_t *list, file_list_t *menu_list)
{
    (void)data;
    (void)list;
    (void)menu_list;
}

const ui_companion_driver_t ui_companion_cocoa = {
   ui_companion_cocoa_init,
   ui_companion_cocoa_deinit,
   ui_companion_cocoa_iterate,
   ui_companion_cocoa_toggle,
   ui_companion_cocoa_event_command,
   ui_companion_cocoa_notify_content_loaded,
   ui_companion_cocoa_notify_list_pushed,
   NULL,
   NULL,
   NULL,
   "cocoa",
};
