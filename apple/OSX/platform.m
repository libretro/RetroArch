/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <string.h>

#import "../common/RetroArch_Apple.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../frontend/frontend.h"
#include "../../retroarch.h"

#include <objc/objc-runtime.h>

id<RetroArch_Platform> apple_platform;

void apple_rarch_exited(void);

void apple_rarch_exited(void)
{
   [apple_platform unloadingCore];
}

@interface RApplication : NSApplication
@end

@implementation RApplication

- (void)sendEvent:(NSEvent *)event
{
   NSEventType event_type;
   cocoa_input_data_t *apple = NULL;
   driver_t *driver = driver_get_ptr();
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
         pos = [[RAGameView get] convertPoint:[event locationInWindow] fromView:nil];
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

@interface RetroArch_OSX()
@property (nonatomic, retain) NSWindowController* settingsWindow;
@property (nonatomic, retain) NSWindow IBOutlet* coreSelectSheet;
@property (nonatomic, copy) NSString* core;
@end

@implementation RetroArch_OSX

@synthesize window = _window;
@synthesize settingsWindow = _settingsWindow;
@synthesize coreSelectSheet = _coreSelectSheet;
@synthesize core = _core;

- (void)dealloc
{
   [_window release];
   [_coreSelectSheet release];
   [_settingsWindow release];
   [_core release];
   [super dealloc];
}

+ (RetroArch_OSX*)get
{
   return (RetroArch_OSX*)[[NSApplication sharedApplication] delegate];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   apple_platform = self;

   
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
   [self.window setCollectionBehavior:[self.window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary];
#endif
   
   [self.window setAcceptsMouseMovedEvents: YES];

   [[RAGameView get] setFrame: [[self.window contentView] bounds]];
   [[self.window contentView] setAutoresizesSubviews:YES];
   [[self.window contentView] addSubview:[RAGameView get]];
   [self.window makeFirstResponder:[RAGameView get]];

   self.settingsWindow = [[[NSWindowController alloc] initWithWindowNibName:BOXSTRING("Settings")] autorelease];

   [apple_platform loadingCore:nil withFile:nil];
   
   if (rarch_main(waiting_argc, waiting_argv))
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

   event_command(EVENT_CMD_QUIT);

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
         event_command(EVENT_CMD_LOAD_CONTENT);
      else
         [self chooseCore];

      [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
   }
   else
   {
      apple_display_alert("Cannot open multiple files", "RetroArch");
      [sender replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
   }
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
            event_command(EVENT_CMD_LOAD_CONTENT);
         else
            [self performSelector:@selector(chooseCore) withObject:nil afterDelay:.5f];
      }
   }];
#else
   [panel beginSheetForDirectory:nil file:nil modalForWindopw:[self window] modalDelegate:self didEndSelector:@selector(didEndSaveSheet:returnCode:contextInfo:) contextInfo:NULL];
#endif
   [[NSApplication sharedApplication] runModalForWindow:panel];
}

- (void)chooseCore
{
   [[NSApplication sharedApplication] beginSheet:self.coreSelectSheet modalForWindow:self.window modalDelegate:nil didEndSelector:nil contextInfo:nil];
   [[NSApplication sharedApplication] runModalForWindow:self.coreSelectSheet];
}

- (IBAction)coreWasChosen:(id)sender
{
   global_t *global = global_get_ptr();

   [[NSApplication sharedApplication] stopModal];
   [[NSApplication sharedApplication] endSheet:self.coreSelectSheet returnCode:0];
   [self.coreSelectSheet orderOut:self];

   if (global && global->system.shutdown)
      return;

   if (global && !global->main_is_init)
   {
      /* TODO/FIXME: Set core/content here. */
      event_command(EVENT_CMD_LOAD_CORE);
      event_command(EVENT_CMD_LOAD_CONTENT);
   }
   else
      event_command(EVENT_CMD_QUIT);
}

- (void)loadingCore:(const NSString*)core withFile:(const char*)file
{
   if (file)
      [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:BOXSTRING(file)]];
}

- (void)unloadingCore
{
   [[NSApplication sharedApplication] terminate:nil];
}

- (IBAction)showCoresDirectory:(id)sender
{
   settings_t *settings = config_get_ptr();
   [[NSWorkspace sharedWorkspace] openFile:BOXSTRING(settings->libretro_directory)];
}

- (IBAction)showPreferences:(id)sender
{
   [NSApp runModalForWindow:[self.settingsWindow window]];
}

- (IBAction)basicEvent:(id)sender
{
   unsigned cmd;
   unsigned sender_tag = (unsigned)[sender tag];
   global_t *global = global_get_ptr();
   
   RARCH_LOG("Gets here, sender tag is: %d\n", sender_tag);

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

   event_command(cmd);
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
      if (strcmp(argv[i], "--") == 0)
      {
         waiting_argc = argc - i;
         waiting_argv = argv + i;
         break;
      }
   }

   return NSApplicationMain(argc, (const char **) argv);
}

