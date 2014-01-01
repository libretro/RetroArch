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

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"
#include "apple/common/apple_input.h"

#include "file.h"

static void* const associated_core_key = (void*)&associated_core_key;

@interface RApplication : NSApplication
@end

@implementation RApplication

- (void)sendEvent:(NSEvent *)event
{
   [super sendEvent:event];
   
   NSEventType event_type = [event type];
   
   if (event_type == NSKeyDown || event_type == NSKeyUp)
      apple_input_handle_key_event([event keyCode], event_type == GSEVENT_TYPE_KEYDOWN);
   else if (event_type == NSFlagsChanged)
   {
      static uint32_t old_flags = 0;
      uint32_t new_flags = [event modifierFlags];
      bool down = (new_flags & old_flags) == old_flags;
      old_flags = new_flags;
      
      apple_input_handle_key_event([event keyCode], down);
   }
   else if (event_type == NSMouseMoved || event_type == NSLeftMouseDragged ||
            event_type == NSRightMouseDragged || event_type == NSOtherMouseDragged)
   {
      // Relative
      g_current_input_data.mouse_delta[0] += [event deltaX];
      g_current_input_data.mouse_delta[1] += [event deltaY];

      // Absolute
      NSPoint pos = [[RAGameView get] convertPoint:[event locationInWindow] fromView:nil];
      g_current_input_data.touches[0].screen_x = pos.x;
      g_current_input_data.touches[0].screen_y = pos.y;
   }
   else if (event_type == NSLeftMouseDown || event_type == NSRightMouseDown || event_type == NSOtherMouseDown)
   {
      g_current_input_data.mouse_buttons |= 1 << [event buttonNumber];
      g_current_input_data.touch_count = 1;
   }
   else if (event_type == NSLeftMouseUp || event_type == NSRightMouseUp || event_type == NSOtherMouseUp)
   {
      g_current_input_data.mouse_buttons &= ~(1 << [event buttonNumber]);
      g_current_input_data.touch_count = 0;
   }
}

@end

@interface RetroArch_OSX()
@property (nonatomic, retain) NSWindowController* settingsWindow;
@property (nonatomic, retain) NSWindow IBOutlet* coreSelectSheet;
@property (nonatomic, copy) NSString* file;
@property (nonatomic, copy) NSString* core;
@end

@implementation RetroArch_OSX

@synthesize window = _window;
@synthesize configDirectory = _configDirectory;
@synthesize globalConfigFile = _globalConfigFile;
@synthesize coreDirectory = _coreDirectory;
@synthesize settingsWindow = _settingsWindow;
@synthesize coreSelectSheet = _coreSelectSheet;
@synthesize file = _file;
@synthesize core = _core;

- (void)dealloc
{
   [_window release];
   [_configDirectory release];
   [_globalConfigFile release];
   [_coreDirectory release];
   [_coreSelectSheet release];
   [_settingsWindow release];
   [_file release];
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
   _loaded = true;

   NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
   self.configDirectory = [[paths objectAtIndex:0] stringByAppendingPathComponent:@"RetroArch"];
   self.globalConfigFile = [NSString stringWithFormat:@"%@/retroarch.cfg", self.configDirectory];
   self.coreDirectory = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents/Resources/modules"];

   [self.window setAcceptsMouseMovedEvents: YES];
   
   [[RAGameView get] setFrame: [[self.window contentView] bounds]];
   [[self.window contentView] setAutoresizesSubviews:YES];
   [[self.window contentView] addSubview:RAGameView.get];
   [self.window makeFirstResponder:[RAGameView get]];
   
   self.settingsWindow = [[[NSWindowController alloc] initWithWindowNibName:@"Settings"] autorelease];
   
   // Create core select list
   NSComboBox* cb = (NSComboBox*)[[self.coreSelectSheet contentView] viewWithTag:1];

   apple_core_info_set_core_path([self.coreDirectory UTF8String]);
   apple_core_info_set_config_path([self.configDirectory UTF8String]);
   const core_info_list_t* cores = apple_core_info_list_get();
   for (int i = 0; cores && i != cores->count; i ++)
   {
      NSString* desc = BOXSTRING(cores->list[i].display_name);
      objc_setAssociatedObject(desc, associated_core_key, apple_get_core_id(&cores->list[i]), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      [cb addItemWithObjectValue:desc];
   }

   if ([cb numberOfItems])
      [cb selectItemAtIndex:0];
   else
      apple_display_alert(@"No libretro cores were found.\nSelect \"Go->Cores Directory\" from the menu and place libretro dylib files there.", @"RetroArch");
   
   // Run RGUI if needed
   if (!_wantReload)//TODO || apple_argv)
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
      apple_event_basic_command(QUIT);

   return apple_is_running ? NSTerminateCancel : NSTerminateNow;
}


- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if ([filenames count] == 1 && [filenames objectAtIndex:0])
   {
      self.file = [filenames objectAtIndex:0];
      
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
   [panel beginSheetModalForWindow:self.window completionHandler:^(NSInteger result)
   {
      [[NSApplication sharedApplication] stopModal];
   
      if (result == NSOKButton && [panel URL])
      {
         self.file = [[panel URL] path];
         [self performSelector:@selector(chooseCore) withObject:nil afterDelay:.5f];
      }
   }];
   [[NSApplication sharedApplication] runModalForWindow:panel];
}

// This utility function will queue the self.core and self.file instance values for running.
- (void)runCore
{
   _wantReload = apple_is_running;

   if (!apple_is_running)
      apple_run_core(self.core, [self.file UTF8String]);
   else
      apple_event_basic_command(QUIT);
}

- (void)chooseCore
{
   [[NSApplication sharedApplication] beginSheet:self.coreSelectSheet modalForWindow:self.window modalDelegate:nil didEndSelector:nil contextInfo:nil];
   [[NSApplication sharedApplication] runModalForWindow:self.coreSelectSheet];
}

- (IBAction)coreWasChosen:(id)sender
{
   [[NSApplication sharedApplication] stopModal];
   [[NSApplication sharedApplication] endSheet:self.coreSelectSheet returnCode:0];
   [self.coreSelectSheet orderOut:self];

   if (_isTerminating)
      return;

   NSComboBox* cb = (NSComboBox*)[[self.coreSelectSheet contentView] viewWithTag:1];
   self.core = objc_getAssociatedObject([cb objectValueOfSelectedItem], associated_core_key);

   [self runCore];
}

#pragma mark RetroArch_Platform
- (void)loadingCore:(const NSString*)core withFile:(const char*)file
{
   if (file)
      [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[NSURL fileURLWithPath:BOXSTRING(file)]];

   apple_start_iteration();
}

- (void)unloadingCore:(const NSString*)core
{
   apple_stop_iteration();

   if (_isTerminating)
      [[NSApplication sharedApplication] terminate:nil];

   if (_wantReload)
      apple_run_core(self.core, [self.file UTF8String]);
   else if(apple_use_tv_mode)
      apple_run_core(nil, 0);
   else
      [[NSApplication sharedApplication] terminate:nil];
   
   _wantReload = false;
}

#pragma mark Menus
- (IBAction)showCoresDirectory:(id)sender
{
   [[NSWorkspace sharedWorkspace] openFile:self.coreDirectory];
}

- (IBAction)showPreferences:(id)sender
{
   [NSApp runModalForWindow:[self.settingsWindow window]];
}

- (IBAction)basicEvent:(id)sender
{
   if (apple_is_running)
      apple_event_basic_command([sender tag]);
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
   [[NSApplication sharedApplication] stopModal];
}

@end

int main(int argc, char *argv[])
{
/* TODO
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
*/

   return NSApplicationMain(argc, (const char **) argv);
}

