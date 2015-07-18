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

#include "cocoa/cocoa_common.h"
#include "../ui_companion_driver.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../frontend/frontend.h"
#include "../../runloop_data.h"

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
   driver_t *driver = driver_get_ptr();
   if (!driver)
      return;
   [super sendEvent:event];

   apple = (cocoa_input_data_t*)driver->input_data;
   event_type = event.type;
   
   if (!apple)
      return;

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
               cocoa_input_keyboard_event(event_type == NSKeyDown,
                     0, [ch characterAtIndex:i], mod, RETRO_DEVICE_KEYBOARD);
         }
         
         cocoa_input_keyboard_event(event_type == NSKeyDown,
               event.keyCode, character, mod, RETRO_DEVICE_KEYBOARD);
      }
         break;
      case NSFlagsChanged:
      {
         static uint32_t old_flags = 0;
         uint32_t new_flags = event.modifierFlags;
         bool down = (new_flags & old_flags) == old_flags;
         old_flags = new_flags;

         cocoa_input_keyboard_event(down, event.keyCode,
               0, event.modifierFlags, RETRO_DEVICE_KEYBOARD);
      }
         break;
      case NSMouseMoved:
      case NSLeftMouseDragged:
      case NSRightMouseDragged:
      case NSOtherMouseDragged:
      {
         NSPoint pos;
         /* Relative */
         apple->mouse_x = event.deltaX;
         apple->mouse_y = event.deltaY;

         /* Absolute */
         pos = [[CocoaView get] convertPoint:[event locationInWindow] fromView:nil];
         apple->touches[0].screen_x = pos.x;
         apple->touches[0].screen_y = pos.y;
      }
         break;
       case NSScrollWheel:
           /* TODO/FIXME - properly implement. */
           break;
      case NSLeftMouseDown:
      case NSRightMouseDown:
      case NSOtherMouseDown:
         apple->mouse_buttons |= 1 << event.buttonNumber;
         apple->touch_count = 1;
         break;
      case NSLeftMouseUp:
      case NSRightMouseUp:
      case NSOtherMouseUp:
         apple->mouse_buttons &= ~(1 << event.buttonNumber);
         apple->touch_count = 0;
         break;
   }
}

@end

static int waiting_argc;
static char** waiting_argv;

@implementation RetroArch

@synthesize window = _window;

- (void)dealloc
{
   [_window release];
   [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   apple_platform = self;
   
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
   [self.window setCollectionBehavior:[self.window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary];
#endif
   
   [self.window setAcceptsMouseMovedEvents: YES];

   [[CocoaView get] setFrame: [[self.window contentView] bounds]];
   [[self.window contentView] setAutoresizesSubviews:YES];
   [[self.window contentView] addSubview:[CocoaView get]];
   [self.window makeFirstResponder:[CocoaView get]];

   if (rarch_main(waiting_argc, waiting_argv, NULL))
      apple_rarch_exited();

   waiting_argc = 0;
}

static void poll_iteration(void)
{
    NSEvent *event = NULL;
    driver_t *driver = driver_get_ptr();
    cocoa_input_data_t *apple = (cocoa_input_data_t*)driver->input_data;

    if (!apple)
      return;

    apple->mouse_x = 0;
    apple->mouse_y = 0;
    
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
        poll_iteration();
        ret = rarch_main_iterate();
        rarch_main_data_iterate();
        while(CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, FALSE) == kCFRunLoopRunHandledSource);
    }
    
    main_exit(NULL);
}

- (void) apple_start_iteration
{
    [self performSelectorOnMainThread:@selector(rarch_main) withObject:nil waitUntilDone:NO];
}

- (void) apple_stop_iteration
{
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
   [self apple_start_iteration];
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
   [self apple_stop_iteration];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
   return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
   NSApplicationTerminateReply reply = NSTerminateNow;
   global_t *global = global_get_ptr();

   if (global && global->main_is_init)
      reply = NSTerminateCancel;

   ui_companion_event_command(EVENT_CMD_QUIT);

   return reply;
}


- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if (filenames.count == 1 && [filenames objectAtIndex:0])
   {
      global_t *global = global_get_ptr();
      NSString *__core = [filenames objectAtIndex:0];
      const char *core_name = global ? global->menu.info.library_name : NULL;
		
		if (global)
         strlcpy(global->fullpath, __core.UTF8String, sizeof(global->fullpath));

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

- (IBAction)openCore:(id)sender {
    NSOpenPanel* panel = (NSOpenPanel*)[NSOpenPanel openPanel];
#if defined(MAC_OS_X_VERSION_10_6)
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result)
     {
         [[NSApplication sharedApplication] stopModal];
         
         if (result == NSOKButton && panel.URL)
         {
             menu_handle_t *menu  = menu_driver_get_ptr();
             global_t *global     = global_get_ptr();
             settings_t *settings = config_get_ptr();
             NSURL *url = (NSURL*)panel.URL;
             NSString *__core = url.path;
             
             if (__core)
             {
                 strlcpy(settings->libretro, __core.UTF8String, sizeof(settings->libretro));
                 ui_companion_event_command(EVENT_CMD_LOAD_CORE);
                 
                 if (menu->load_no_content && settings->core.set_supports_no_game_enable)
                 {
                     *global->fullpath = '\0';
                     menu_common_load_content(false, CORE_TYPE_PLAIN);
                 }
             }
             else
                 [self performSelector:@selector(chooseCore) withObject:nil afterDelay:.5f];
         }
     }];
#else
    [panel beginSheetForDirectory:nil file:nil modalForWindopw:[self window] modalDelegate:self didEndSelector:@selector(didEndSaveSheet:returnCode:contextInfo:) contextInfo:NULL];
#endif
    [[NSApplication sharedApplication] runModalForWindow:panel];
}

- (void)openDocument:(id)sender
{
   NSOpenPanel* panel = (NSOpenPanel*)[NSOpenPanel openPanel];
#if defined(MAC_OS_X_VERSION_10_6)
   [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result)
   {
      [[NSApplication sharedApplication] stopModal];

      if (result == NSOKButton && panel.URL)
      {
         global_t *global = global_get_ptr();
         NSURL *url = (NSURL*)panel.URL;
         NSString *__core = url.path;
         const char *core_name = global ? global->menu.info.library_name : NULL;
			
			if (global)
            strlcpy(global->fullpath, __core.UTF8String, sizeof(global->fullpath));

         if (core_name)
            ui_companion_event_command(EVENT_CMD_LOAD_CONTENT);
         else
            [self performSelector:@selector(chooseCore) withObject:nil afterDelay:.5f];
      }
   }];
#else
   [panel beginSheetForDirectory:nil file:nil modalForWindopw:[self window] modalDelegate:self didEndSelector:@selector(didEndSaveSheet:returnCode:contextInfo:) contextInfo:NULL];
#endif
   [[NSApplication sharedApplication] runModalForWindow:panel];
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
   global_t *global = global_get_ptr();
   
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
      global->pending.windowed_scale = idx;
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
   int i;
   for (i = 0; i < argc; i ++)
   {
      if (!strcmp(argv[i], "--"))
      {
         waiting_argc = argc - i;
         waiting_argv = argv + i;
         break;
      }
   }

   return NSApplicationMain(argc, (const char **) argv);
}

void apple_display_alert(const char *message, const char *title)
{
    NSAlert* alert = [[NSAlert new] autorelease];
    
    [alert setMessageText:(*title) ? BOXSTRING(title) : BOXSTRING("RetroArch")];
    [alert setInformativeText:BOXSTRING(message)];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert beginSheetModalForWindow:((RetroArch*)[[NSApplication sharedApplication] delegate]).window
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
   event_command(cmd);
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
   "cocoa",
};
