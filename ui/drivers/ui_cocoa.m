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

#include <defines/cocoa_defines.h>
#include "cocoa/cocoa_common.h"
#include "cocoa/apple_platform.h"

#if defined(HAVE_COCOA_METAL)
#include "../../gfx/common/metal_common.h"
#endif

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
#include "../../verbosity.h"

#include "ui_cocoa.h"

typedef struct ui_application_cocoa
{
   void *empty;
} ui_application_cocoa_t;

/* TODO/FIXME - static global variables */
static int waiting_argc;
static char **waiting_argv;

#if defined(HAVE_COCOA)
extern id apple_platform;
#endif

static void* ui_window_cocoa_init(void)
{
   return NULL;
}

static void ui_window_cocoa_destroy(void *data)
{
#if !defined(HAVE_COCOA_METAL)
    ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
    CocoaView *cocoa_view    = (CocoaView*)cocoa->data;
    [[cocoa_view window] release];
#endif
}

static void ui_window_cocoa_set_focused(void *data)
{
    ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
    CocoaView *cocoa_view    = (BRIDGE CocoaView*)cocoa->data;
    [[cocoa_view window] makeKeyAndOrderFront:nil];
}

static void ui_window_cocoa_set_visible(void *data,
        bool set_visible)
{
    ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
    CocoaView *cocoa_view    = (BRIDGE CocoaView*)cocoa->data;
    if (set_visible)
        [[cocoa_view window] makeKeyAndOrderFront:nil];
    else
        [[cocoa_view window] orderOut:nil];
}

static void ui_window_cocoa_set_title(void *data, char *buf)
{
   CocoaView *cocoa_view    = (BRIDGE CocoaView*)data;
   const char* const text   = buf; /* < Can't access buffer directly in the block */
   [[cocoa_view window] setTitle:[NSString stringWithCString:text encoding:NSUTF8StringEncoding]];
}

static void ui_window_cocoa_set_droppable(void *data, bool droppable)
{
   ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
   CocoaView *cocoa_view    = (BRIDGE CocoaView*)cocoa->data;

   if (droppable)
   {
#if defined(HAVE_COCOA_METAL)
      [[cocoa_view window] registerForDraggedTypes:@[NSPasteboardTypeColor, NSPasteboardTypeFileURL]];
#elif defined(HAVE_COCOA)
      [[cocoa_view window] registerForDraggedTypes:[NSArray arrayWithObjects:NSColorPboardType, NSFilenamesPboardType, nil]];
#endif
   }
   else
      [[cocoa_view window] unregisterDraggedTypes];
}

static bool ui_window_cocoa_focused(void *data)
{
   ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
   CocoaView *cocoa_view    = (BRIDGE CocoaView*)cocoa->data;
   return cocoa_view.window.isMainWindow;
}

static ui_window_t ui_window_cocoa = {
   ui_window_cocoa_init,
   ui_window_cocoa_destroy,
   ui_window_cocoa_set_focused,
   ui_window_cocoa_set_visible,
   ui_window_cocoa_set_title,
   ui_window_cocoa_set_droppable,
   ui_window_cocoa_focused,
   "cocoa"
};

static bool ui_browser_window_cocoa_open(ui_browser_window_state_t *state)
{
   NSOpenPanel *panel = [NSOpenPanel openPanel];

   if (!string_is_empty(state->filters))
   {
#ifdef HAVE_COCOA_METAL
      [panel setAllowedFileTypes:@[BOXSTRING(state->filters), BOXSTRING(state->filters_title)]];
#else
      NSArray *filetypes = [[NSArray alloc] initWithObjects:BOXSTRING(state->filters), BOXSTRING(state->filters_title), nil];
      [panel setAllowedFileTypes:filetypes];
#endif
   }

#if defined(MAC_OS_X_VERSION_10_5)
   [panel setMessage:BOXSTRING(state->title)];
   if ([panel runModalForDirectory:BOXSTRING(state->startdir) file:nil] != 1)
      return false;
#else
   panel.title                           = NSLocalizedString(BOXSTRING(state->title), BOXSTRING("open panel"));
   panel.directoryURL                    = [NSURL fileURLWithPath:BOXSTRING(state->startdir)];
   panel.canChooseDirectories            = NO;
   panel.canChooseFiles                  = YES;
   panel.allowsMultipleSelection         = NO;
   panel.treatsFilePackagesAsDirectories = NO;

   if ([panel runModal] != 1)
       return false;
#endif

   NSURL *url           = (NSURL*)panel.URL;
   const char *res_path = [url.path UTF8String];
   state->result        = strdup(res_path);

   return true;
}

static bool ui_browser_window_cocoa_save(ui_browser_window_state_t *state)
{
   return false;
}

static ui_browser_window_t ui_browser_window_cocoa = {
   ui_browser_window_cocoa_open,
   ui_browser_window_cocoa_save,
   "cocoa"
};

static enum ui_msg_window_response ui_msg_window_cocoa_dialog(ui_msg_window_state *state, enum ui_msg_window_type type)
{
#if defined(HAVE_COCOA_METAL)
   NSModalResponse response;
   NSAlert *alert = [NSAlert new];
#elif defined(HAVE_COCOA)
   NSInteger response;
   NSAlert* alert = [[NSAlert new] autorelease];
#endif

   if (!string_is_empty(state->title))
      [alert setMessageText:BOXSTRING(state->title)];
   [alert setInformativeText:BOXSTRING(state->text)];

   switch (state->buttons)
   {
      case UI_MSG_WINDOW_OK:
         [alert addButtonWithTitle:BOXSTRING("OK")];
         break;
      case UI_MSG_WINDOW_YESNO:
         [alert addButtonWithTitle:BOXSTRING("Yes")];
         [alert addButtonWithTitle:BOXSTRING("No")];
         break;
      case UI_MSG_WINDOW_OKCANCEL:
         [alert addButtonWithTitle:BOXSTRING("OK")];
         [alert addButtonWithTitle:BOXSTRING("Cancel")];
         break;
      case UI_MSG_WINDOW_YESNOCANCEL:
         [alert addButtonWithTitle:BOXSTRING("Yes")];
         [alert addButtonWithTitle:BOXSTRING("No")];
         [alert addButtonWithTitle:BOXSTRING("Cancel")];
         break;
   }

   switch (type)
   {
      case UI_MSG_WINDOW_TYPE_ERROR:
         [alert setAlertStyle:NSAlertStyleCritical];
         break;
      case UI_MSG_WINDOW_TYPE_WARNING:
         [alert setAlertStyle:NSAlertStyleWarning];
         break;
      case UI_MSG_WINDOW_TYPE_QUESTION:
         [alert setAlertStyle:NSAlertStyleInformational];
         break;
      case UI_MSG_WINDOW_TYPE_INFORMATION:
         [alert setAlertStyle:NSAlertStyleInformational];
         break;
   }

#if defined(HAVE_COCOA_METAL)
   [alert beginSheetModalForWindow:(BRIDGE NSWindow *)ui_companion_driver_get_main_window()
                 completionHandler:^(NSModalResponse returnCode) {
                    [[NSApplication sharedApplication] stopModalWithCode:returnCode];
                 }];
   response = [alert runModal];
#elif defined(HAVE_COCOA)
    [alert beginSheetModalForWindow:ui_companion_driver_get_main_window()
                      modalDelegate:apple_platform
                     didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
    response = [[NSApplication sharedApplication] runModalForWindow:[alert window]];
#endif

   switch (state->buttons)
   {
      case UI_MSG_WINDOW_OKCANCEL:
         if (response == NSAlertSecondButtonReturn)
            return UI_MSG_RESPONSE_CANCEL;
         /* fall-through */
      case UI_MSG_WINDOW_OK:
         if (response == NSAlertFirstButtonReturn)
            return UI_MSG_RESPONSE_OK;
         break;
      case UI_MSG_WINDOW_YESNOCANCEL:
         if (response == NSAlertThirdButtonReturn)
            return UI_MSG_RESPONSE_CANCEL;
         /* fall-through */
      case UI_MSG_WINDOW_YESNO:
         if (response == NSAlertFirstButtonReturn)
            return UI_MSG_RESPONSE_YES;
         if (response == NSAlertSecondButtonReturn)
            return UI_MSG_RESPONSE_NO;
         break;
   }

   return UI_MSG_RESPONSE_NA;
}

static enum ui_msg_window_response ui_msg_window_cocoa_error(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_ERROR);
}

static enum ui_msg_window_response ui_msg_window_cocoa_information(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_INFORMATION);
}

static enum ui_msg_window_response ui_msg_window_cocoa_question(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_QUESTION);
}

static enum ui_msg_window_response ui_msg_window_cocoa_warning(ui_msg_window_state *state)
{
   return ui_msg_window_cocoa_dialog(state, UI_MSG_WINDOW_TYPE_WARNING);
}

static ui_msg_window_t ui_msg_window_cocoa = {
   ui_msg_window_cocoa_error,
   ui_msg_window_cocoa_information,
   ui_msg_window_cocoa_question,
   ui_msg_window_cocoa_warning,
   "cocoa"
};

static void* ui_application_cocoa_initialize(void)
{
   return NULL;
}

static void ui_application_cocoa_process_events(void)
{
    for (;;)
    {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
        if (!event)
            break;
#ifndef HAVE_COCOA_METAL
        [event retain];
#endif
        [NSApp sendEvent: event];
#ifndef HAVE_COCOA_METAL
        [event retain];
#endif
    }
}

static ui_application_t ui_application_cocoa = {
   ui_application_cocoa_initialize,
   ui_application_cocoa_process_events,
   NULL,
   false,
   "cocoa"
};

@interface CommandPerformer : NSObject
{
   void *data;
   enum event_command cmd;
}
@end /* @interface CommandPerformer */

@implementation CommandPerformer

- (id)initWithData:(void *)userdata command:(enum event_command)command
{
   self = [super init];
   if (!self)
      return self;

   self->data = userdata;
   self->cmd  = command;

   return self;
}

- (void)perform
{
   command_event(self->cmd, self->data);
}

@end /* @implementation CommandPerformer */

#if defined(HAVE_COCOA_METAL)
@interface RAWindow : NSWindow
@end

@implementation RAWindow
#elif defined(HAVE_COCOA)
@interface RApplication : NSApplication
@end

@implementation RApplication
#endif

#ifdef HAVE_COCOA_METAL
#define CONVERT_POINT() [apple_platform.renderView convertPoint:[event locationInWindow] fromView:nil]
#else
#define CONVERT_POINT() [[CocoaView get] convertPoint:[event locationInWindow] fromView:nil]
#endif

- (void)keyDown:(NSEvent *)theEvent
{
   switch([theEvent keyCode])
   {
      case 0x35: /* Escape */
         break;
      default:
         [super keyDown:theEvent];
   }
}

- (void)sendEvent:(NSEvent *)event {
   NSEventType event_type = event.type;

   [super sendEvent:event];

   switch ((int32_t)event_type)
   {
      case NSEventTypeKeyDown:
      case NSEventTypeKeyUp:
         {
            uint32_t i;
            NSString* ch              = event.characters;
            uint32_t mod              = 0;
            const char *inputTextUTF8 = ch.UTF8String;
            uint32_t character        = inputTextUTF8[0];
            NSUInteger mods           = event.modifierFlags;
            uint16_t keycode          = event.keyCode;

            if (mods & NSEventModifierFlagCapsLock)
               mod |= RETROKMOD_CAPSLOCK;
            if (mods & NSEventModifierFlagShift)
               mod |=  RETROKMOD_SHIFT;
            if (mods & NSEventModifierFlagControl)
               mod |=  RETROKMOD_CTRL;
            if (mods & NSEventModifierFlagOption)
               mod |= RETROKMOD_ALT;
            if (mods & NSEventModifierFlagCommand)
               mod |= RETROKMOD_META;
            if (mods & NSEventModifierFlagNumericPad)
               mod |=  RETROKMOD_NUMLOCK;

            for (i = 1; i < ch.length; i++)
               apple_input_keyboard_event(event_type == NSEventTypeKeyDown,
                     0, inputTextUTF8[i], mod, RETRO_DEVICE_KEYBOARD);

            apple_input_keyboard_event(event_type == NSEventTypeKeyDown,
                  keycode, character, mod, RETRO_DEVICE_KEYBOARD);
         }
         break;
#if defined(HAVE_COCOA_METAL)
        case NSEventTypeFlagsChanged:
#elif defined(HAVE_COCOA)
        case NSFlagsChanged:
#endif
         {
            static NSUInteger old_flags           = 0;
            NSUInteger new_flags                  = event.modifierFlags;
            bool down                             = (new_flags & old_flags) == old_flags;
            uint16_t keycode                      = event.keyCode;

            old_flags                             = new_flags;

            apple_input_keyboard_event(down, keycode,
                  0, (uint32_t)new_flags, RETRO_DEVICE_KEYBOARD);
         }
         break;
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
         {
            CGFloat delta_x             = event.deltaX;
            CGFloat delta_y             = event.deltaY;
            NSPoint pos                 = CONVERT_POINT();
            cocoa_input_data_t 
               *apple                   = (cocoa_input_data_t*)
               input_state_get_ptr()->current_data;
            if (!apple)
               return;
            /* Relative */
            apple->mouse_rel_x         += (int16_t)delta_x;
            apple->mouse_rel_y         += (int16_t)delta_y;

            /* Absolute */
            apple->touches[0].screen_x  = (int16_t)pos.x;
            apple->touches[0].screen_y  = (int16_t)pos.y;
            apple->window_pos_x         = (int16_t)pos.x;
            apple->window_pos_y         = (int16_t)pos.y;
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
           NSInteger number      = event.buttonNumber;
           NSPoint pos           = CONVERT_POINT();
           cocoa_input_data_t 
              *apple             = (cocoa_input_data_t*)
              input_state_get_ptr()->current_data;
           if (!apple || pos.y < 0)
               return;
           apple->mouse_buttons |= (1 << number);
           apple->touch_count    = 1;
       }
           break;
      case NSEventTypeLeftMouseUp:
      case NSEventTypeRightMouseUp:
      case NSEventTypeOtherMouseUp:
         {
            NSInteger number      = event.buttonNumber;
            NSPoint pos           = CONVERT_POINT();
            cocoa_input_data_t 
              *apple              = (cocoa_input_data_t*)
              input_state_get_ptr()->current_data;
            if (!apple || pos.y < 0)
               return;
            apple->mouse_buttons &= ~(1 << number);
            apple->touch_count    = 0;
         }
         break;
      default:
         break;
   }
}

@end

@implementation RetroArch_OSX

@synthesize window = _window;

#ifndef HAVE_COCOA_METAL
- (void)dealloc
{
   [_window release];
   [super dealloc];
}
#endif

#define NS_WINDOW_COLLECTION_BEHAVIOR_FULLSCREEN_PRIMARY (1 << 7)

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   unsigned i;
   apple_platform   = self;
   [self.window setAcceptsMouseMovedEvents: YES];

#if MAC_OS_X_VERSION_10_7
   self.window.collectionBehavior = NS_WINDOW_COLLECTION_BEHAVIOR_FULLSCREEN_PRIMARY;
#endif

#ifdef HAVE_COCOA_METAL
   _listener = [WindowListener new];

   [self.window setNextResponder:_listener];
   self.window.delegate = _listener;
#else
   [[CocoaView get] setFrame: [[self.window contentView] bounds]];
#endif
   [[self.window contentView] setAutoresizesSubviews:YES];

#ifndef HAVE_COCOA_METAL
   [[self.window contentView] addSubview:[CocoaView get]];
   [self.window makeFirstResponder:[CocoaView get]];
#endif

   for (i = 0; i < waiting_argc; i++)
   {
      if (string_is_equal(waiting_argv[i], "-NSDocumentRevisionsDebugMode"))
      {
         waiting_argv[i]   = NULL;
         waiting_argv[i+1] = NULL;
         waiting_argc     -= 2;
      }
   }
   if (rarch_main(waiting_argc, waiting_argv, NULL))
      [[NSApplication sharedApplication] terminate:nil];

   waiting_argc = 0;

#ifdef HAVE_COCOA_METAL
   [self setupMainWindow];
#endif

   [self performSelectorOnMainThread:@selector(rarch_main) withObject:nil waitUntilDone:NO];
}

#pragma mark - ApplePlatform

#ifdef HAVE_COCOA_METAL
- (void)setupMainWindow
{
   [self.window makeMainWindow];
   [self.window makeKeyWindow];
}

- (void)setViewType:(apple_view_type_t)vt
{
   if (vt == _vt)
      return;

   _vt                              = vt;

   if (_renderView)
   {
      _renderView.wantsLayer        = NO;
      _renderView.layer             = nil;
      [_renderView removeFromSuperview];
      self.window.contentView       = nil;
      _renderView                   = nil;
   }

   switch (vt)
   {
      case APPLE_VIEW_TYPE_VULKAN:
      case APPLE_VIEW_TYPE_METAL:
         {
            MetalView *v            = [MetalView new];
            v.paused                = YES;
            v.enableSetNeedsDisplay = NO;
            _renderView             = v;
         }
         break;
      case APPLE_VIEW_TYPE_OPENGL:
         _renderView                = [CocoaView get];
         break;
      case APPLE_VIEW_TYPE_NONE:
      default:
         return;
   }

   _renderView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
   [_renderView setFrame: [[self.window contentView] bounds]];

   self.window.contentView               = _renderView;
   self.window.contentView.nextResponder = _listener;
}

- (apple_view_type_t)viewType { return _vt; }
- (id)renderView { return _renderView; }
- (bool)hasFocus { return [NSApp isActive]; }

- (void)setVideoMode:(gfx_ctx_mode_t)mode
{
   BOOL is_fullscreen = (self.window.styleMask 
         & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen;
   if (mode.fullscreen)
   {
      if (!is_fullscreen)
      {
         [self.window toggleFullScreen:self];
         return;
      }
   }
   else
   {
      if (is_fullscreen)
         [self.window toggleFullScreen:self];
   }

   /* HACK(sgc): ensure MTKView posts a drawable resize event */
   if (mode.width > 0)
      [self.window setContentSize:NSMakeSize(mode.width-1, mode.height)];
   [self.window setContentSize:NSMakeSize(mode.width, mode.height)];
}

- (void)setCursorVisible:(bool)v
{
   if (v)
      [NSCursor unhide];
   else
      [NSCursor hide];
}

- (bool)setDisableDisplaySleep:(bool)disable
{
   if (disable)
   {
      if (_sleepActivity == nil)
         _sleepActivity = [NSProcessInfo.processInfo beginActivityWithOptions:NSActivityIdleDisplaySleepDisabled reason:@"disable screen saver"];
   }
   else
   {
      if (_sleepActivity)
      {
         [NSProcessInfo.processInfo endActivity:_sleepActivity];
         _sleepActivity = nil;
      }
   }
   return YES;
}
#endif

- (void) rarch_main
{
    for (;;)
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

       while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, FALSE) 
             == kCFRunLoopRunHandledSource);
       if (ret == -1)
       {
#ifdef HAVE_QT
          ui_application_qt.quit();
#endif
          break;
       }
    }

    main_exit(NULL);
}

- (void)applicationDidBecomeActive:(NSNotification *)notification  { }
- (void)applicationWillResignActive:(NSNotification *)notification { }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication { return YES; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
   NSApplicationTerminateReply reply = NSTerminateNow;
   uint32_t runloop_flags            = runloop_get_flags();

   if (runloop_flags & RUNLOOP_FLAG_IS_INITED)
      reply = NSTerminateCancel;

   command_event(CMD_EVENT_QUIT, NULL);

   return reply;
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if ((filenames.count == 1) && [filenames objectAtIndex:0])
   {
      struct retro_system_info *system = &runloop_state_get_ptr()->system.info;
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
      const ui_msg_window_t *msg_window = 
         ui_companion_driver_get_msg_window_ptr();
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
   rarch_system_info_t *info        = &runloop_state_get_ptr()->system;
   settings_t           *settings   = config_get_ptr();
   bool set_supports_no_game_enable = 
      settings->bools.set_supports_no_game_enable;
   if (!state || string_is_empty(state->result))
      return;
   if (!result)
      return;

   path_set(RARCH_PATH_CORE, state->result);
   ui_companion_event_command(CMD_EVENT_LOAD_CORE);

   if (     info
         && info->load_no_content
         && set_supports_no_game_enable)
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

static void open_document_handler(
      ui_browser_window_state_t *state, bool result)
{
   struct retro_system_info *system = &runloop_state_get_ptr()->system.info;
   const char            *core_name = system ? system->library_name : NULL;

   if (!state || string_is_empty(state->result))
      return;
   if (!result)
      return;

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

- (IBAction)openCore:(id)sender
{
   const ui_browser_window_t *browser = 
      ui_companion_driver_get_browser_window_ptr();

   if (browser)
   {
      ui_browser_window_state_t browser_state;
      bool result                   = false;
      settings_t *settings          = config_get_ptr();
      const char *path_dir_libretro = settings->paths.directory_libretro;

      browser_state.filters         = strdup("dylib");
      browser_state.filters_title   = strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS));
      browser_state.title           = strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST));
      browser_state.startdir        = strdup(path_dir_libretro);

      result                        = browser->open(&browser_state);
      open_core_handler(&browser_state, result);

      free(browser_state.filters);
      free(browser_state.filters_title);
      free(browser_state.title);
      free(browser_state.startdir);
   }
}

- (void)openDocument:(id)sender
{
   const ui_browser_window_t *browser = 
      ui_companion_driver_get_browser_window_ptr();

   if (browser)
   {
      ui_browser_window_state_t
         browser_state                  = {NULL};
      bool result                       = false;
      settings_t *settings              = config_get_ptr();
      const char *path_dir_menu_content = settings->paths.directory_menu_content;
      NSString *startdir                = BOXSTRING(path_dir_menu_content);

      if (!startdir.length)
         startdir                      = BOXSTRING("/");

      browser_state.title               = strdup(msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST));
      browser_state.startdir            = strdup([startdir UTF8String]);

      result                            = browser->open(&browser_state);
      open_document_handler(&browser_state, result);

      free(browser_state.startdir);
      free(browser_state.title);
   }
}

- (void)unloadingCore { }
- (IBAction)showPreferences:(id)sender { }

- (IBAction)showCoresDirectory:(id)sender
{
   settings_t          *settings = config_get_ptr();
   const char *path_dir_libretro = settings->paths.directory_libretro;
   [[NSWorkspace sharedWorkspace] openFile:BOXSTRING(path_dir_libretro)];
}

- (IBAction)basicEvent:(id)sender
{
   enum event_command cmd = CMD_EVENT_NONE;
   unsigned    sender_tag = (unsigned)[sender tag];

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
      case 21:
         cmd = CMD_EVENT_TAKE_SCREENSHOT;
         break;
      case 22:
         cmd = CMD_EVENT_AUDIO_MUTE_TOGGLE;
         break;
      default:
         break;
   }

   if (sender_tag >= 10 && sender_tag <= 19)
   {
      unsigned idx = (sender_tag - (10-1));
      retroarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);
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
       if (argv[1])
           if (!strncmp(argv[1], "-psn", 4))
               argc = 1;
   }

   waiting_argc = argc;
   waiting_argv = argv;

   return NSApplicationMain(argc, (const char **) argv);
}

static void ui_companion_cocoa_deinit(void *data)
{
   [[NSApplication sharedApplication] terminate:nil];
}

static void *ui_companion_cocoa_init(void) { return (void*)-1; }
static void ui_companion_cocoa_notify_content_loaded(void *data) { }
static void ui_companion_cocoa_toggle(void *data, bool force) { }
static void ui_companion_cocoa_event_command(void *data, enum event_command cmd)
{
   switch (cmd)
   {
      case CMD_EVENT_SHADERS_APPLY_CHANGES:
      case CMD_EVENT_SHADER_PRESET_LOADED:
         break;
      default: {
         id performer = [[CommandPerformer alloc] initWithData:data command:cmd];
         [performer performSelectorOnMainThread:@selector(perform) withObject:nil waitUntilDone:NO];
         RELEASE(performer);
      }
      break;
   }
}
static void ui_companion_cocoa_notify_list_pushed(void *data, file_list_t *a, file_list_t *b) { }

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
   NULL, /* is_active */
   &ui_browser_window_cocoa,
   &ui_msg_window_cocoa,
   &ui_window_cocoa,
   &ui_application_cocoa,
   "cocoa",
};
