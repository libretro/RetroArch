/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include <pthread.h>
#include <string.h>

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"
#include "../RetroArch/apple_input.h"

// If USE_XATTR is defined any loaded file will get a com.RetroArch.Core extended attribute
// specifying which core was used to load.
//#define USE_XATTR

#if defined(USE_XATTR)
#include "sys/xattr.h"
#endif

#include "file.h"

@interface RApplication : NSApplication
@end

@implementation RApplication

- (void)sendEvent:(NSEvent *)event
{
   [super sendEvent:event];

   if (event.type == GSEVENT_TYPE_KEYDOWN || event.type == GSEVENT_TYPE_KEYUP)
      apple_input_handle_key_event(event.keyCode, event.type == GSEVENT_TYPE_KEYDOWN);
}

@end

@implementation RetroArch_OSX
{
   NSWindow IBOutlet* _coreSelectSheet;

   bool _isTerminating;
   bool _loaded;
   bool _wantReload;
   NSString* _file;
   RAModuleInfo* _core;
}

+ (RetroArch_OSX*)get
{
   return (RetroArch_OSX*)[[NSApplication sharedApplication] delegate];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   apple_platform = self;
   _loaded = true;

   [window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
   
   RAGameView.get.frame = [window.contentView bounds];
   [window.contentView setAutoresizesSubviews:YES];
   [window.contentView addSubview:RAGameView.get];   
   [window makeFirstResponder:RAGameView.get];
   
   // Create core select list
   NSComboBox* cb = (NSComboBox*)[_coreSelectSheet.contentView viewWithTag:1];
   
   for (RAModuleInfo* i in RAModuleInfo.getModules)
      [cb addItemWithObjectValue:i];

   if (cb.numberOfItems)
      [cb selectItemAtIndex:0];
   else
      apple_display_alert(@"No libretro cores were found.\nSelect \"Go->Cores Directory\" from the menu and place libretro dylib files there.", @"RetroArch");

   // Run RGUI if needed
   if (!_wantReload || apple_argv)
      apple_run_core(nil, 0);
   else
      [self chooseCore];

   _wantReload = false;
   
   extern void osx_pad_init();
   osx_pad_init();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication
{
   return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
   _isTerminating = true;

   if (apple_is_running)
      apple_frontend_post_event(apple_event_basic_command, (void*)QUIT);

   return apple_is_running ? NSTerminateCancel : NSTerminateNow;
}


- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if (filenames.count == 1 && filenames[0])
   {
      _file = filenames[0];
      
      if (!_loaded)
         _wantReload = true;
      else
         [self chooseCore];
      
      [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
   }
   else
   {
      apple_display_alert(@"Cannot open multiple files", @"RetroArch");
      [sender replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
   }
}

- (void)openDocument:(id)sender
{
   NSOpenPanel* panel = [NSOpenPanel openPanel];
   [panel beginSheetModalForWindow:window completionHandler:^(NSInteger result)
   {
      [NSApplication.sharedApplication stopModal];
   
      if (result == NSOKButton && panel.URL)
      {
         _file = panel.URL.path;
         [self performSelector:@selector(chooseCore) withObject:nil afterDelay:.5f];
      }
   }];
   [NSApplication.sharedApplication runModalForWindow:panel];
}

// This utility function will queue the _core and _file instance values for running.
// If the emulator thread is already running it will tell it to quit.
- (void)runCore
{
   _wantReload = apple_is_running;

   if (!apple_is_running)
      apple_run_core(_core, _file.UTF8String);
   else
      apple_frontend_post_event(apple_event_basic_command, (void*)QUIT);
}

- (void)chooseCore
{
#ifdef USE_XATTR
   char stored_name[PATH_MAX];
   if (getxattr(_file.UTF8String, "com.RetroArch.Core", stored_name, PATH_MAX, 0, 0) > 0)
   {
      for (RAModuleInfo* i in RAModuleInfo.getModules)
      {
         const char* core_name = i.path.lastPathComponent.UTF8String;
         if (strcmp(core_name, stored_name) == 0)
         {
            _core = i;
            [self runCore];
            return;
         }
      }
   }
#endif

   [NSApplication.sharedApplication beginSheet:_coreSelectSheet modalForWindow:window modalDelegate:nil didEndSelector:nil contextInfo:nil];
   [NSApplication.sharedApplication runModalForWindow:_coreSelectSheet];
}

- (IBAction)coreWasChosen:(id)sender
{
   [NSApplication.sharedApplication stopModal];
   [NSApplication.sharedApplication endSheet:_coreSelectSheet returnCode:0];
   [_coreSelectSheet orderOut:self];

   if (_isTerminating)
      return;

   NSComboBox* cb = (NSComboBox*)[_coreSelectSheet.contentView viewWithTag:1];
   _core = (RAModuleInfo*)cb.objectValueOfSelectedItem;

   [self runCore];
}

#pragma mark RetroArch_Platform
- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file
{
   if (file)
   {
      [NSDocumentController.sharedDocumentController noteNewRecentDocumentURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:file]]];
      
#ifdef USE_XATTR
      const char* core_name = core.path.lastPathComponent.UTF8String;
      setxattr(file, "com.RetroArch.Core", core_name, strlen(core_name) + 1, 0, 0);
#endif
   }
}

- (void)unloadingCore:(RAModuleInfo*)core
{
   if (_isTerminating)
      [NSApplication.sharedApplication terminate:nil];

   if (_wantReload)
      apple_run_core(_core, _file.UTF8String);
   else if(apple_use_tv_mode)
      apple_run_core(nil, 0);
   else
      [NSApplication.sharedApplication terminate:nil];
   
   _wantReload = false;
}

- (NSString*)configPath
{
   return [[self retroarchConfigPath] stringByAppendingPathComponent:@"retroarch.cfg"];
}

- (NSString*)retroarchConfigPath
{
   NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
   return [paths[0] stringByAppendingPathComponent:@"RetroArch"];
}

- (NSString*)corePath
{
   return [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"Contents/Resources/modules"];
}

#pragma mark Menus
- (IBAction)showCoresDirectory:(id)sender
{
   [[NSWorkspace sharedWorkspace] openFile:self.corePath];
}

- (IBAction)showPreferences:(id)sender
{
   NSWindowController* wc = [[NSWindowController alloc] initWithWindowNibName:@"Settings"];
   [NSApp runModalForWindow:wc.window];
}

- (IBAction)basicEvent:(id)sender
{
   if (apple_is_running)
      apple_frontend_post_event(&apple_event_basic_command, (void*)((NSMenuItem*)sender).tag);
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
   [NSApplication.sharedApplication stopModal];
}

@end

int main(int argc, char *argv[])
{
   uint32_t current_argc = 0;

   for (int i = 0; i != argc; i ++)
   {
      if (strcmp(argv[i], "--") == 0)
      {
         current_argc = 1;
         apple_argv = malloc(sizeof(char*) * (argc + 1));
         memset(apple_argv, 0, sizeof(char*) * (argc + 1));
         apple_argv[0] = argv[0];
      }
      else if (current_argc)
      {
         apple_argv[current_argc ++] = argv[i];
      }
   }

   return NSApplicationMain(argc, (const char **) argv);
}

