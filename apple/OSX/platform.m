/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <objc/runtime.h>

#import "../common/RetroArch_Apple.h"
#include "../../input/apple_input.h"
#include "../../frontend/menu/menu_common.h"

#include "../../file.h"

static void* const associated_core_key = (void*)&associated_core_key;

@interface RApplication : NSApplication
@end

@implementation RApplication

- (void)sendEvent:(NSEvent *)event
{
   [super sendEvent:event];

	apple_input_data_t *apple = (apple_input_data_t*)driver.input_data;
   NSEventType event_type = event.type;

   switch ((NSInteger)event_type)
   {
      case NSKeyDown:
      case NSKeyUp:
      {
         NSString* ch = (NSString*)event.characters;

         if (!ch || ch.length == 0)
            apple_input_keyboard_event(event_type == NSKeyDown, event.keyCode, 0, 0);
         else
         {
            apple_input_keyboard_event(event_type == NSKeyDown, event.keyCode, [ch characterAtIndex:0], event.modifierFlags);

            for (NSUInteger i = 1; i < ch.length; i ++)
               apple_input_keyboard_event(event_type == NSKeyDown, 0, [ch characterAtIndex:i], event.modifierFlags);
         }
      }
         break;
      case NSFlagsChanged:
      {
         static uint32_t old_flags = 0;
         uint32_t new_flags = event.modifierFlags;
         bool down = (new_flags & old_flags) == old_flags;
         old_flags = new_flags;

         apple_input_keyboard_event(down, event.keyCode, 0, event.modifierFlags);
      }
         break;
      case NSMouseMoved:
      case NSLeftMouseDragged:
      case NSRightMouseDragged:
      case NSOtherMouseDragged:
      {
         NSPoint pos;
         // Relative
         apple->mouse_delta[0] += event.deltaX;
         apple->mouse_delta[1] += event.deltaY;

         // Absolute
         pos = [[RAGameView get] convertPoint:[event locationInWindow] fromView:nil];
         apple->touches[0].screen_x = pos.x;
         apple->touches[0].screen_y = pos.y;
      }
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

   char support_path_buf[PATH_MAX + 1];
   CFArrayRef array = CFSearchPathForDirectoriesInDomains(CFApplicationSupportDirectory, CFUserDomainMask, YES);
   CFStringRef support_path = CFBridgingRetain(CFArrayGetValueAtIndex(array, 0));
   CFStringGetCString(support_path, support_path_buf, sizeof(support_path_buf), kCFStringEncodingUTF8);
   
   fill_pathname_join(g_defaults.core_dir, NSBundle.mainBundle.bundlePath.UTF8String, "Contents/Resources/modules", sizeof(g_defaults.core_dir));
   fill_pathname_join(g_defaults.menu_config_dir, support_path_buf, "RetroArch", sizeof(g_defaults.menu_config_dir));
   fill_pathname_join(g_defaults.config_path, g_defaults.menu_config_dir, "retroarch.cfg", sizeof(g_defaults.config_path));

   CFRelease(support_path);
   CFRelease(array);

   
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1070
   [self.window setCollectionBehavior:[self.window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary];
#endif
   
   [self.window setAcceptsMouseMovedEvents: YES];

   [[RAGameView get] setFrame: [[self.window contentView] bounds]];
   [[self.window contentView] setAutoresizesSubviews:YES];
   [[self.window contentView] addSubview:[RAGameView get]];
   [self.window makeFirstResponder:[RAGameView get]];

   self.settingsWindow = [[[NSWindowController alloc] initWithWindowNibName:BOXSTRING("Settings")] autorelease];

   // Warn if there are no cores present
   core_info_set_core_path();
   const core_info_list_t* core_list = (const core_info_list_t*)core_info_list_get();

   // Create core select list
   NSComboBox* cb = (NSComboBox*)[[self.coreSelectSheet contentView] viewWithTag:1];

   for (size_t i = 0; core_list && i < core_list->count; i ++)
   {
      NSString* desc = (NSString*)BOXSTRING(core_list->list[i].display_name);
#if defined(MAC_OS_X_VERSION_10_6)
	  /* FIXME - Rewrite this so that this is no longer an associated object - requires ObjC 2.0 runtime */
      objc_setAssociatedObject(desc, associated_core_key, BOXSTRING(core_list->list[i].path), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
#endif
	   [cb addItemWithObjectValue:desc];
   }

   apple_run_core(waiting_argc, waiting_argv, nil, 0);

   waiting_argc = 0;
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
   apple_start_iteration();
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
   apple_stop_iteration();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
   return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    NSApplicationTerminateReply reply = NSTerminateNow;

   if (g_extern.main_is_init)
       reply = NSTerminateCancel;
       
   g_extern.system.shutdown = true;

   return reply;
}


- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if (filenames.count == 1 && [filenames objectAtIndex:0])
   {
       NSString *__core = [filenames objectAtIndex:0];
       const char *core_name = g_extern.menu.info.library_name;
       strlcpy(g_extern.fullpath, __core.UTF8String, sizeof(g_extern.fullpath));
       
       if (core_name)
           rarch_main_command(RARCH_CMD_LOAD_CONTENT);
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
          NSURL *url = (NSURL*)panel.URL;
          NSString *__core = url.path;
          const char *core_name = g_extern.menu.info.library_name;
          strlcpy(g_extern.fullpath, __core.UTF8String, sizeof(g_extern.fullpath));
          
          if (core_name)
              rarch_main_command(RARCH_CMD_LOAD_CONTENT);
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
   NSComboBox* cb;
    
   [[NSApplication sharedApplication] stopModal];
   [[NSApplication sharedApplication] endSheet:self.coreSelectSheet returnCode:0];
   [self.coreSelectSheet orderOut:self];

   if (g_extern.system.shutdown)
      return;
   
   /* TODO - rewrite this. */

   cb = (NSComboBox*)[[self.coreSelectSheet contentView] viewWithTag:1];
#if defined(MAC_OS_X_VERSION_10_6)
	/* FIXME - Rewrite this so that this is no longer an associated object - requires ObjC 2.0 runtime */
   self.core = objc_getAssociatedObject(cb.objectValueOfSelectedItem, associated_core_key);
#endif
    
    if (!g_extern.main_is_init)
    {
       /* Load core/content here. */
    }
    else
        g_extern.system.shutdown = true;
}

#pragma mark RetroArch_Platform
- (void)loadingCore:(const NSString*)core withFile:(const char*)file
{
   if (file)
      [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:BOXSTRING(file)]];
}

- (void)unloadingCore
{
   [[NSApplication sharedApplication] terminate:nil];
}

#pragma mark Menus
- (IBAction)showCoresDirectory:(id)sender
{
   [[NSWorkspace sharedWorkspace] openFile:BOXSTRING(g_settings.libretro_directory)];
}

- (IBAction)showPreferences:(id)sender
{
   [NSApp runModalForWindow:[self.settingsWindow window]];
}

- (IBAction)basicEvent:(id)sender
{
   unsigned sender_tag, cmd;
   sender_tag = (unsigned)[sender tag];

   switch (sender_tag)
   {
      case 1:
         cmd = RARCH_CMD_RESET;
         break;
      case 2:
         cmd = RARCH_CMD_LOAD_STATE;
         break;
      case 3:
         cmd = RARCH_CMD_SAVE_STATE;
         break;
      default:
         cmd = RARCH_CMD_NONE;
         break;
   }

   rarch_main_command(cmd);
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
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

