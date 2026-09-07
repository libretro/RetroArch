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

#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>

#include <boolean.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <queues/task_queue.h>
#include <retro_timers.h>

#include <defines/cocoa_defines.h>
#include "cocoa/cocoa_common.h"
#include "cocoa/apple_platform.h"

/* The Carbon Process Manager's TransformProcessType(), the fallback for
 * promoting a bare-binary process to a foreground GUI app on a system
 * without -setActivationPolicy:.  See main() below.  Declared in every
 * SDK, deprecated since 10.9 and still present; the choice between the
 * two is made at runtime, not here. */
#include <ApplicationServices/ApplicationServices.h>

/* For NX_DEVICE*KEYMASK - the device-specific L/R modifier-key bits that
 * ride in NSEvent.modifierFlags alongside the coalesced high-order bits. */
#include <IOKit/hidsystem/IOLLEvent.h>

#ifdef HAVE_METAL
#include "../../gfx/drivers/metal.h"
#endif

#include "../ui_companion_driver.h"
#include "../../gfx/video_display_server.h"
#include "../../input/drivers/cocoa_input.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"
#include "../../frontend/frontend.h"
#include "../../configuration.h"
#include "../../paths.h"
#include "../../core.h"
#include "../../menu/menu_cbs.h"
#include "../../menu/menu_displaylist.h"
#include "../../retroarch.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#include "ui_cocoa.h"

#ifdef HAVE_SWIFT
#import "RetroArch-Swift.h"
#endif

#ifdef HAVE_MIST
#include "../../steam/steam.h"
#endif

typedef struct ui_application_cocoa
{
   void *empty;
} ui_application_cocoa_t;

/* TODO/FIXME - static global variables */
static int waiting_argc;
static char **waiting_argv;

extern id<ApplePlatform> apple_platform;

static void* ui_window_cocoa_init(void)
{
   return NULL;
}

static void ui_window_cocoa_destroy(void *data)
{
   /* The window is owned by the application delegate for the life of
    * the process; there is nothing to tear down per view here. */
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

/* data is video_st->display_userdata, which cocoa_common.m sets to the
 * CocoaView itself - the raw object, not a ui_window_cocoa_t around it -
 * so it is cast straight to the view. The other entry points in this
 * table expect the wrapper; only Win32 calls them, with its own struct,
 * and nothing calls them here. */
static void ui_window_cocoa_set_title(void *data, char *buf)
{
   CocoaView *cocoa_view = (BRIDGE CocoaView*)data;
   NSString  *title;

   if (!cocoa_view || !buf)
      return;

   /* buf is video_st->window_title, shared and rewritten by the next
    * frame, so the string is made here, on the calling thread. Owned
    * rather than autoreleased: under threaded video this runs on the
    * video thread, which has no autorelease pool, and an autoreleased
    * object there is simply leaked under MRC. The title is content and
    * core names, which can carry a filename in whatever encoding a
    * foreign filesystem wrote it in; a byte sequence that is not UTF-8
    * yields nil, and -[NSWindow setTitle:] throws on nil, so fall back
    * to Latin-1, which accepts any bytes. */
   title = [[NSString alloc] initWithUTF8String:buf];
   if (!title)
      title = [[NSString alloc] initWithCString:buf
            encoding:NSISOLatin1StringEncoding];
   if (!title)
      return;

   /* AppKit is main-thread-only. Under threaded video this is reached
    * from the video thread (gl3_frame -> video_driver_update_title), so
    * the set is marshalled onto the main thread without waiting - the
    * video thread's lock must never be held while waiting on main.
    * performSelectorOnMainThread: is the Foundation way, on every OS X
    * back to 10.0, so there is no SDK gate and no GCD; it costs one
    * run-loop source signal, the same as a dispatch to the main queue,
    * for a title that changes a few times a session. The target is the
    * view, whose -setWindowTitle: does the -window lookup on main where
    * that AppKit state belongs. The perform retains title until it has
    * run, so the alloc above is released here under MRC either way; a
    * no-op under ARC. */
   if ([NSThread isMainThread])
      [cocoa_view setWindowTitle:title];
   else
      [cocoa_view performSelectorOnMainThread:@selector(setWindowTitle:)
            withObject:title waitUntilDone:NO];
   RARCH_RELEASE(title);
}

static void ui_window_cocoa_set_droppable(void *data, bool droppable)
{
   ui_window_cocoa_t *cocoa = (ui_window_cocoa_t*)data;
   CocoaView *cocoa_view    = (BRIDGE CocoaView*)cocoa->data;

   if (droppable)
   {
      /* The same two types CocoaView registers for itself and reads
       * back in -draggingEntered:.  The 10.6 NSPasteboardTypeColor
       * and 10.13 NSPasteboardTypeFileURL are different type strings,
       * which the drop handler does not look for. */
      NSArray *types = [[NSArray alloc] initWithObjects:
            RARCH_PBOARD_TYPE_COLOR, RARCH_PBOARD_TYPE_FILENAMES, nil];
      [[cocoa_view window] registerForDraggedTypes:types];
      RARCH_RELEASE(types);
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

   if (state->filters && *state->filters)
   {
      /* Spelled out rather than as an @[...] literal so the file
       * builds with GCC as well as clang; the +1 from alloc/init is
       * released once the panel has taken its own reference (a no-op
       * under ARC, required under MRC or the array leaks every time
       * the picker opens with a filter). */
      NSArray *filetypes = [[NSArray alloc] initWithObjects:BOXSTRING(state->filters), BOXSTRING(state->filters_title), nil];
      [panel setAllowedFileTypes:filetypes];
      RARCH_RELEASE(filetypes);
   }

   /* The panel's directory and result moved from paths to URLs in 10.6
    * (-setDirectoryURL: / -URL for -runModalForDirectory:file: /
    * -filename). Which generation this system has is asked at runtime,
    * not decided by the build SDK, and each generation's calls are sent
    * in a form the compiler accepts whether or not the SDK declares
    * them: performSelector: for an object argument or result,
    * objc_msgSend for an integer result. Everything else the panel is
    * told is 10.0 API. */
   {
      NSString *startdir = BOXSTRING(state->startdir);
      NSString *path     = nil;
      NSInteger response;

      [panel setTitle:NSLocalizedString(BOXSTRING(state->title), BOXSTRING("open panel"))];
      [panel setCanChooseDirectories:NO];
      [panel setCanChooseFiles:YES];
      [panel setAllowsMultipleSelection:NO];
      [panel setTreatsFilePackagesAsDirectories:NO];

      if ([panel respondsToSelector:@selector(setDirectoryURL:)])
      {
         [panel performSelector:@selector(setDirectoryURL:)
               withObject:[NSURL fileURLWithPath:startdir]];
         response = [panel runModal];
         if (response == 1)
            path = [[panel performSelector:@selector(URL)] path];
      }
      else
      {
         response = ((NSInteger (*)(id, SEL, id, id))objc_msgSend)(panel,
               @selector(runModalForDirectory:file:), startdir, nil);
         if (response == 1)
            path = ((id (*)(id, SEL))objc_msgSend)(panel, @selector(filename));
      }

      if (response != 1 || !path)
         return false;
      state->result = strdup([path UTF8String]);
   }

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
   NSInteger response;
   NSWindow *main_window = (BRIDGE NSWindow *)ui_companion_driver_get_main_window();
   NSAlert *alert        = [NSAlert new];
   RARCH_AUTORELEASE(alert);

   if (state->title && *state->title)
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

   /* The sheet is run as a modal loop either way.  The block form of
    * -beginSheetModalForWindow: is 10.9; it needs a compiler with
    * blocks and an SDK that declares it, and the system is then asked
    * whether it has it.  Everything else takes the 10.3 delegate form,
    * which every release still honours: -alertDidEnd:returnCode:
    * contextInfo: below stops the modal loop and -runModalForWindow:
    * returns the button. */
#if defined(__BLOCKS__) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= 1090)
   if ([alert respondsToSelector:@selector(beginSheetModalForWindow:completionHandler:)])
   {
      [alert beginSheetModalForWindow:main_window
                    completionHandler:^(NSModalResponse returnCode) {
                       [[NSApplication sharedApplication] stopModalWithCode:returnCode];
                    }];
      response = [alert runModal];
   }
   else
#endif
   {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      [alert beginSheetModalForWindow:main_window
                        modalDelegate:apple_platform
                       didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                          contextInfo:nil];
#pragma GCC diagnostic pop
      response = [[NSApplication sharedApplication] runModalForWindow:[alert window]];
   }

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
   /* Intentionally empty. The CFRunLoopObserver (kCFRunLoopBeforeWaiting)
    * fires after the run loop has already processed pending events, so
    * manual event polling here is unnecessary and just adds overhead. */
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

/* RAWindow : NSWindow override.  This is the NSWindow-level sendEvent:
 * hook that AppKit calls unconditionally for every event destined for
 * this window.  Doing it here (rather than subclassing NSApplication)
 * means we don't depend on NSPrincipalClass being set in Info.plist,
 * and every video driver shares one event-dispatch architecture. */
@interface RAWindow : NSWindow
@end

@implementation RAWindow

/* A borderless NSWindow (no NSWindowStyleMaskTitled) cannot become
 * the key window by default - titled is an implicit prerequisite
 * unless this returns YES explicitly.  The windowed-mode RAWindow
 * is titled so this has no effect there, but the borderless
 * full-screen RAWindow made by -[RetroArch_OSX
 * enterBorderlessFullScreen] needs it to receive keystrokes. */
- (BOOL)canBecomeKeyWindow { return YES; }

#define CONVERT_POINT() [[apple_platform renderView] convertPoint:[event locationInWindow] fromView:nil]

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
   /* Bracket syntax throughout - GCC 4.0 on the pre-Obj-C-2.0 10.5
    * SDK doesn't accept dot-syntax on NSEvent's plain getter methods
    * (they aren't declared as @property there).  Modern clang emits
    * identical code for either form. */
   NSEventType event_type = [event type];

   [super sendEvent:event];

   switch ((int32_t)event_type)
   {
      case NSEventTypeKeyDown:
      case NSEventTypeKeyUp:
         {
            uint32_t i;
            NSString* ch              = [event characters];
            uint32_t mod              = 0;
            const char *inputTextUTF8 = [ch UTF8String];
            uint32_t character        = inputTextUTF8[0];
            NSUInteger mods           = [event modifierFlags];
            uint16_t keycode          = [event keyCode];

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

            for (i = 1; i < [ch length]; i++)
               apple_input_keyboard_event(event_type == NSEventTypeKeyDown,
                     0, inputTextUTF8[i], mod, RETRO_DEVICE_KEYBOARD);

            apple_input_keyboard_event(event_type == NSEventTypeKeyDown,
                  keycode, character, mod, RETRO_DEVICE_KEYBOARD);
            if ((mod & RETROKMOD_META) && (event_type == NSEventTypeKeyDown))
               apple_input_keyboard_event(false,
                     keycode, character, mod, RETRO_DEVICE_KEYBOARD);
         }
         break;
      case NSEventTypeFlagsChanged:
         {
            /* Bits we treat as modifier-key transitions: the eight device-
             * specific L/R masks from IOKit plus CapsLock (which has no
             * device-specific bit).  Everything else in modifierFlags is
             * metadata - notably kCGEventFlagMaskNonCoalesced (0x100) - and
             * must not be interpreted as a key press, or we end up
             * synthesising phantom events (a toggle of 0x100 with kc=0 used
             * to translate to MAC_NATIVE_TO_HID[0]==KEY_A, stuck forever). */
            static const NSUInteger mod_mask =
                    NX_DEVICELCTLKEYMASK
                  | NX_DEVICELSHIFTKEYMASK
                  | NX_DEVICERSHIFTKEYMASK
                  | NX_DEVICELCMDKEYMASK
                  | NX_DEVICERCMDKEYMASK
                  | NX_DEVICELALTKEYMASK
                  | NX_DEVICERALTKEYMASK
                  | NX_DEVICERCTLKEYMASK
                  | NSEventModifierFlagCapsLock;
            static NSUInteger old_flags           = 0;
            NSUInteger new_flags                  = [event modifierFlags];
            NSUInteger changed_flags              = (new_flags ^ old_flags) & mod_mask;
            uint16_t keycode                      = [event keyCode];
            bool down                             = false;

            /* Determine if the changed modifier is being pressed or released
             * by checking if it's set in the new flags */
            if (changed_flags != 0)
            {
               /* Find which specific modifier changed and its new state */
               NSUInteger single_change = changed_flags & -changed_flags; /* Isolate rightmost bit */
               down = (new_flags & single_change) != 0;

               apple_input_keyboard_event(down, keycode,
                     0, (uint32_t)new_flags, RETRO_DEVICE_KEYBOARD);
            }

            old_flags = new_flags;
         }
         break;
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
         {
            CGFloat delta_x             = [event deltaX];
            CGFloat delta_y             = [event deltaY];
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

            if (apple->mouse_grabbed)
            {
               apple->window_pos_x      += (int16_t)delta_x;
               apple->window_pos_y      += (int16_t)delta_y;
            }
            else
            {
               apple->window_pos_x       = (int16_t)pos.x;
               apple->window_pos_y       = (int16_t)pos.y;
            }
         }
         break;
      case NSEventTypeScrollWheel:
         /* TODO/FIXME - properly implement. */
         break;
       case NSEventTypeLeftMouseDown:
       case NSEventTypeRightMouseDown:
       case NSEventTypeOtherMouseDown:
       {
           NSInteger number      = [event buttonNumber];
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
            NSInteger number      = [event buttonNumber];
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

@implementation WindowListener

@synthesize window = _window;

/* Similarly to SDL, we'll respond to key events
 * by doing nothing so we don't beep.
 */
- (void)flagsChanged:(NSEvent *)event { }
- (void)keyDown:(NSEvent *)event { }
- (void)keyUp:(NSEvent *)event { }

/* Another window of this app took the keyboard - the desktop companion,
 * a panel. Every key that is down right now will be released into that
 * window, so this window never sees the key-up: the key stays "held" in
 * apple_key_state, and a held key is exactly what the menu's flush-and-
 * wait-for-release (menu_st->input_driver_flushing_input) waits on -
 * for ever, since the release never arrives. That was the desktop
 * companion's "keyboard does not come back after closing it": the hotkey
 * that opened it was still down when the companion took the keyboard.
 * Same treatment as losing the application's active state. */
- (void)windowDidResignKey:(NSNotification *)notification
{
   apple_input_keyboard_reset();
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
   settings_t *settings             = config_get_ptr();
   video_display_server_set_window_opacity(settings->uints.video_window_opacity);
   video_display_server_set_window_decorations(settings->bools.video_window_show_decorations);
}

/* Records the windowed geometry for the remember-position setting.
 * Bracket syntax throughout: -styleMask and -frame are plain methods
 * on the 10.5 SDK, where GCC 4.0 has no dot syntax for them.  The
 * full-screen bit is tested as a number for the same reason (see
 * RARCH_NSWINDOWSTYLEMASK_FULLSCREEN); the pre-10.7 borderless
 * full-screen mode never moves or resizes this window, so it needs no
 * test here. */
- (void)rememberWindowGeometry
{
   settings_t *settings             = config_get_ptr();
   bool window_save_positions       = settings->bools.video_window_save_positions;
   NSRect contentRect;

   if (!window_save_positions)
      return;
   if ([_window styleMask] & RARCH_NSWINDOWSTYLEMASK_FULLSCREEN)
      return;

   contentRect                            = [_window contentRectForFrameRect:[_window frame]];
   settings->uints.window_position_x      = (unsigned)contentRect.origin.x;
   settings->uints.window_position_y      = (unsigned)contentRect.origin.y;
   settings->uints.window_position_width  = (unsigned)contentRect.size.width;
   settings->uints.window_position_height = (unsigned)contentRect.size.height;
}

- (void)windowDidMove:(NSNotification *)notification   { [self rememberWindowGeometry]; }
- (void)windowDidResize:(NSNotification *)notification { [self rememberWindowGeometry]; }

@end


@implementation RetroArch_OSX

/* Written out rather than synthesized: a synthesized retain property
 * calls objc_setProperty / objc_getProperty, which the 10.4 runtime
 * does not have, and this is what keeps a 10.4-target binary from
 * launching on Tiger. */
- (NSWindow *)window { return _window; }

- (void)setWindow:(NSWindow *)window
{
   if (window == _window)
      return;
   (void)RARCH_RETAIN(window);
   RARCH_RELEASE(_window);
   _window = window;
}

#if !__has_feature(objc_arc)
/* ARC auto-generates -dealloc from strong ivars and forbids explicit
 * overrides that just call [super dealloc].  On MRR we have to release
 * _window manually.  See cocoa_defines.h for the ARC/MRR macro story. */
- (void)dealloc
{
   /* Make sure any outstanding NSProcessInfo activity is ended and
    * released.  Without this, quitting while the screensaver is
    * suspended would leak both the activity assertion (leaving
    * display-sleep disabled system-wide until reboot) and the
    * retained token itself. */
   if (_sleepActivity)
   {
      NSProcessInfo *pi = [NSProcessInfo processInfo];
      id token          = _sleepActivity;
      _sleepActivity    = nil;
      if ([pi respondsToSelector:@selector(endActivity:)])
         [pi performSelector:@selector(endActivity:) withObject:token];
      RARCH_RELEASE(token);
   }
   /* _renderView is kept at +1 by setViewType:; release the balance
    * here so the last view-type's render view does not leak at
    * shutdown.  Safe when nil. */
   RARCH_RELEASE(_renderView);
   /* _listener was created with +new (+1) in applicationDidFinishLaunching:
    * and installed as the window's delegate / nextResponder, which are
    * unretained relationships.  Release our own retain so the listener
    * does not leak at shutdown. */
   RARCH_RELEASE(_listener);
   RARCH_RELEASE(_fullscreenWindow);
   RARCH_RELEASE(_window);
   RARCH_SUPER_DEALLOC();
}
#endif

#define NS_WINDOW_COLLECTION_BEHAVIOR_FULLSCREEN_PRIMARY (1 << 7)

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
   int i;
   apple_platform   = self;
   [self.window setAcceptsMouseMovedEvents: YES];

   /* Full-screen-primary is 10.7; the property is on NSWindow from
    * 10.5, so ask whether this system knows the behaviour rather than
    * whether the build SDK declared the constant, which is spelled out
    * above. Setting it on 10.5 or 10.6 would leave a window the system
    * cannot full-screen anyway; there the bit is left alone. */
   /* NSAppKitVersionNumber10_6 is 1038; 10.7 is 1138. */
   if (     [self.window respondsToSelector:@selector(setCollectionBehavior:)]
         && NSAppKitVersionNumber >= 1138.0)
      [self.window setCollectionBehavior:
            NS_WINDOW_COLLECTION_BEHAVIOR_FULLSCREEN_PRIMARY];

   _listener = [WindowListener new];
   [_listener setWindow:self.window];

   [self.window setNextResponder:_listener];
   [self.window setDelegate:_listener];
   [[self.window contentView] setAutoresizesSubviews:YES];

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

   [self setupMainWindow];

#if HAVE_SWIFT
   if (@available(macOS 13.0, *)) {
      [RetroArchAppShortcuts updateAppShortcuts];
   }
#endif

#ifdef HAVE_QT
   /* I think the draw observer should be absolutely fine for qt but I'm not testing it;
    * whoever does test it and confirm it works can just delete this */
   [self performSelectorOnMainThread:@selector(rarch_main) withObject:nil waitUntilDone:NO];
#else
   rarch_start_draw_observer();
#endif
}

#pragma mark - ApplePlatform

- (void)setupMainWindow
{
   [self.window makeMainWindow];
   [self.window makeKeyWindow];
}

- (NSWindow *)hostWindow
{
   if (_fullscreenWindow)
      return _fullscreenWindow;
   return self.window;
}

- (void)setViewType:(apple_view_type_t)vt
{
   NSWindow *host = [self hostWindow];

   if (vt == _vt)
      return;

   _vt                              = vt;

   if (_renderView)
   {
      [_renderView setWantsLayer:NO];
      [_renderView setLayer:nil];
      [_renderView removeFromSuperview];
      [host setContentView:nil];
      /* _renderView holds a +1 retain regardless of which path created
       * it below (see the RARCH_RETAIN on the [CocoaView get] paths,
       * and the inherent +1 from +new on the Metal path).  Release it
       * here so the ownership invariant is balanced before we nil the
       * ivar.  Under ARC this is a no-op and the implicit __strong
       * ivar handles the release when _renderView is assigned nil. */
      RARCH_RELEASE(_renderView);
      _renderView                   = nil;
   }

   switch (vt)
   {
#ifdef HAVE_VULKAN
      case APPLE_VIEW_TYPE_VULKAN:
         {
            /* [CocoaView get] returns an unretained pointer to a
             * singleton.  Retain explicitly so _renderView's +1
             * ownership invariant matches the Metal path below. */
            CAMetalLayer *metal_layer   = [[CAMetalLayer alloc] init];
            _renderView                 = RARCH_RETAIN([CocoaView get]);
            metal_layer.device          = MTLCreateSystemDefaultDevice();
            metal_layer.framebufferOnly = YES;
            metal_layer.contentsScale   = [[NSScreen mainScreen] backingScaleFactor];
            /* CALayer.layer is a strong reference, so the view takes
             * its own retain.  Autorelease our +1 from +alloc/+init
             * so we don't leak the layer on every view-type switch. */
            [_renderView setLayer:metal_layer];
            RARCH_AUTORELEASE(metal_layer);
            [_renderView setWantsLayer:YES];
         }
         break;
#endif
#ifdef HAVE_METAL
      case APPLE_VIEW_TYPE_METAL:
         {
            /* +new returns a +1 object; that retain transfers into
             * _renderView and satisfies the ivar's ownership
             * invariant directly.  No extra RARCH_RETAIN needed. */
            MetalView *v            = [MetalView new];
            v.paused                = YES;
            v.enableSetNeedsDisplay = NO;
            _renderView             = v;
         }
         break;
#endif
      case APPLE_VIEW_TYPE_OPENGL:
         /* Same singleton-retain story as the VULKAN case. */
         _renderView                = RARCH_RETAIN([CocoaView get]);
         break;
      case APPLE_VIEW_TYPE_NONE:
      default:
         return;
   }

   /* Sized from the window's content rectangle rather than from the
    * previous content view, which is gone by now. */
   {
      NSRect content = [host contentRectForFrameRect:[host frame]];
      [_renderView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      [_renderView setFrame:NSMakeRect(0, 0, content.size.width, content.size.height)];
   }

   [host setContentView:_renderView];
   [_renderView setNextResponder:_listener];
   [host makeFirstResponder:_renderView];
}

- (apple_view_type_t)viewType { return _vt; }
- (id)renderView { return _renderView; }
- (bool)hasFocus { return [NSApp isActive]; }

/* Native full-screen (-toggleFullScreen:) is 10.7.  The system in
 * front of us is asked, not the build SDK; the window was given the
 * full-screen-primary collection behaviour in
 * -applicationDidFinishLaunching: under the same test. */
- (BOOL)hasNativeFullScreen
{
   return NSAppKitVersionNumber >= 1138.0
      && [self.window respondsToSelector:@selector(toggleFullScreen:)];
}

- (BOOL)isFullScreen
{
   if (_fullscreenWindow)
      return YES;
   return ([self.window styleMask] & RARCH_NSWINDOWSTYLEMASK_FULLSCREEN) != 0;
}

/* Pre-10.7 full-screen: a borderless RAWindow covering the chosen
 * screen, hosting the render view above the menu bar.
 *
 * -[NSView enterFullScreenMode:withOptions:] is not used because it
 * captures the displays and moves the view into an AppKit-made plain
 * NSWindow, so -[RAWindow sendEvent:], which feeds keyboard and mouse
 * events to cocoa_input, stops firing.  A window of our own class
 * keeps it firing; SDL and GLFW take the same route for pre-Lion
 * full-screen.  The main window's style mask cannot be toggled between
 * titled and borderless instead: -[NSWindow setStyleMask:] is 10.6. */
- (void)enterBorderlessFullScreen
{
   NSScreen *screen        = (BRIDGE NSScreen *)cocoa_screen_get_chosen();
   NSRect    screen_frame  = [screen frame];
   NSView   *view          = _renderView;

   if (_fullscreenWindow || !view)
      return;

   /* NSBorderlessWindowMask is 0 on every release, so the style is
    * spelled out rather than named.  The level above the menu bar is
    * belt and braces once the bar is hidden below. */
   _fullscreenWindow = [[RAWindow alloc]
         initWithContentRect:screen_frame
                   styleMask:0
                     backing:NSBackingStoreBuffered
                       defer:NO];
   [_fullscreenWindow setLevel:NSMainMenuWindowLevel + 1];
   [_fullscreenWindow setOpaque:YES];
   [_fullscreenWindow setHidesOnDeactivate:YES];
   [_fullscreenWindow setReleasedWhenClosed:NO];
   [_fullscreenWindow setNextResponder:_listener];

   /* Hide the menu bar and Dock only when going full screen on the
    * screen that owns the menu bar; hiding it for a secondary screen
    * would mangle the primary one. */
   if ([[NSScreen screens] count] > 0
         && [screen isEqual:[[NSScreen screens] objectAtIndex:0]])
      [NSMenu setMenuBarVisible:NO];

   /* Move the render view over.  The view is retained across the move
    * so that giving up its slot in the main window cannot drop the
    * last reference; the content view of a window fills it, so no
    * frame bookkeeping is needed in either direction. */
   (void)RARCH_RETAIN(view);
   [self.window setContentView:nil];
   [_fullscreenWindow setContentView:view];
   RARCH_RELEASE(view);

   [self.window orderOut:nil];
   [_fullscreenWindow makeKeyAndOrderFront:nil];
   [_fullscreenWindow makeFirstResponder:view];
}

- (void)exitBorderlessFullScreen
{
   NSView *view = _renderView;

   if (!_fullscreenWindow)
      return;

   (void)RARCH_RETAIN(view);
   [_fullscreenWindow setContentView:nil];
   [self.window setContentView:view];
   RARCH_RELEASE(view);

   [NSMenu setMenuBarVisible:YES];

   [_fullscreenWindow orderOut:nil];
   RARCH_RELEASE(_fullscreenWindow);
   _fullscreenWindow = nil;

   [self.window makeKeyAndOrderFront:nil];
   [self.window makeFirstResponder:view];
}

- (void)setVideoMode:(gfx_ctx_mode_t)mode
{
   BOOL is_fullscreen = [self isFullScreen];

   if (mode.fullscreen)
   {
      if (!is_fullscreen)
      {
         if ([self hasNativeFullScreen])
         {
            /* Sent through objc_msgSend so the 10.5 and 10.6 SDKs,
             * which do not declare -toggleFullScreen:, still build. */
            ((void (*)(id, SEL, id))objc_msgSend)(self.window,
                  @selector(toggleFullScreen:), self);
            [self.window setAlphaValue:1];
         }
         else
            [self enterBorderlessFullScreen];
         return;
      }
   }
   else
   {
      if (is_fullscreen)
      {
         if (_fullscreenWindow)
            [self exitBorderlessFullScreen];
         else
            ((void (*)(id, SEL, id))objc_msgSend)(self.window,
                  @selector(toggleFullScreen:), self);
      }
      [self updateWindowedSize:mode];
   }

   /* HACK(sgc): ensure MTKView posts a drawable resize event */
   if (mode.width > 0)
       [self.window setContentSize:NSMakeSize(mode.width-1, mode.height)];
   [self.window setContentSize:NSMakeSize(mode.width, mode.height)];
   [self.window displayIfNeeded];
}

- (void)updateWindowedSize:(gfx_ctx_mode_t)mode
{
   settings_t *settings             = config_get_ptr();
   BOOL is_fullscreen               = [self isFullScreen];
   bool windowed_full               = settings->bools.video_fullscreen && settings->bools.video_windowed_fullscreen;
   bool window_save_positions       = settings->bools.video_window_save_positions;

   if (is_fullscreen || windowed_full)
       return;

   if (window_save_positions)
   {
      NSRect contentRect;
      NSRect frame;
      contentRect.origin.x    = settings->uints.window_position_x;
      contentRect.origin.y    = settings->uints.window_position_y;
      contentRect.size.width  = settings->uints.window_position_width;
      contentRect.size.height = settings->uints.window_position_height;
      frame                   = [self.window frameRectForContentRect:contentRect];
      [self.window setFrame:frame display:YES];
   }
   else
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
   /* beginActivityWithOptions:reason: / endActivity: were added in
    * macOS 10.9.  Guard at compile time so builds against older SDKs
    * (notably 10.5 via Xcode 3.1 / GCC 4.0, which predate both App
    * Nap and ARC) still compile, and guard at runtime so a binary
    * built against a 10.9+ SDK but deployed onto 10.5-10.8 gracefully
    * no-ops instead of crashing on an unrecognized selector. */
   NSProcessInfo *pi = [NSProcessInfo processInfo];
   /* beginActivityWithOptions:reason: and endActivity: are 10.9. The
    * system is asked whether it has them; the options constant is an
    * integer (NSActivityIdleDisplaySleepDisabled is 1 << 40) and the
    * calls go through objc_msgSend and performSelector:, so no SDK
    * needs to declare either for this to compile. */
   if (![pi respondsToSelector:@selector(beginActivityWithOptions:reason:)])
      return NO;

   if (disable)
   {
      if (_sleepActivity == nil)
      {
         /* The token comes back autoreleased and owned by the system.
          * It MUST be retained or, under MRC, it is deallocated when
          * the current pool drains, leaving _sleepActivity dangling;
          * the next endActivity: then messages freed memory. Under
          * ARC a plain id ivar is __strong and RARCH_RETAIN is a
          * no-op. */
         id token = ((id (*)(id, SEL, unsigned long long, id))objc_msgSend)(
               pi, @selector(beginActivityWithOptions:reason:),
               (unsigned long long)(1ULL << 40),
               @"disable screen saver");
         _sleepActivity = RARCH_RETAIN(token);
      }
   }
   else
   {
      if (_sleepActivity)
      {
         /* Captured and nil'd BEFORE ending it, so a re-entrant call -
          * from a menu write handler while AppKit is dispatching, say -
          * cannot see a stale pointer and end the same activity twice. */
         id token       = _sleepActivity;
         _sleepActivity = nil;
         [pi performSelector:@selector(endActivity:) withObject:token];
         RARCH_RELEASE(token);
      }
   }
   return YES;
}

#ifdef HAVE_QT
- (void) rarch_main
{
    for (;;)
    {
       int ret;
#ifdef HAVE_QT
       const ui_application_t *application = uico_state_get_ptr()->drv->application;
#else
       const ui_application_t *application = &ui_application_cocoa;
#endif
       if (application)
          application->process_events();
       ui_companion_driver_wimp_iterate();

       ret = runloop_iterate();

       task_queue_check();

#ifdef HAVE_MIST
       steam_poll();
#endif

       while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.002, FALSE)
             == kCFRunLoopRunHandledSource);
       if (ret == -1 || ui_companion_driver_wimp_exiting())
       {
          ui_companion_driver_wimp_quit();
          break;
       }
    }

    ui_companion_driver_wimp_deinit();
    main_exit(NULL);
}
#endif

- (void)applicationDidBecomeActive:(NSNotification *)notification  { }
- (void)applicationWillResignActive:(NSNotification *)notification
{
   apple_input_keyboard_reset();
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication { return YES; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
   NSApplicationTerminateReply reply = NSTerminateNow;

   command_event(CMD_EVENT_QUIT, NULL);

   rarch_stop_draw_observer();

   return reply;
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
   if ((filenames.count == 1) && [filenames objectAtIndex:0])
   {
      struct retro_system_info *sysinfo = &runloop_state_get_ptr()->system.info;
      NSString *__core                  = [filenames objectAtIndex:0];
      const char *core_name             = sysinfo->library_name;

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
   rarch_system_info_t *sys_info    = &runloop_state_get_ptr()->system;
   settings_t           *settings   = config_get_ptr();
   bool set_supports_no_game_enable =
      settings->bools.set_supports_no_game_enable;
   if (!state || !state->result || !*state->result)
      return;
   if (!result)
      return;

   path_set(RARCH_PATH_CORE, state->result);
   ui_companion_event_command(CMD_EVENT_LOAD_CORE);

   if (     sys_info
         && sys_info->load_no_content
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
   struct retro_system_info *sysinfo = &runloop_state_get_ptr()->system.info;
   const char            *core_name  = sysinfo ? sysinfo->library_name : NULL;

   if (!state || !state->result || !*state->result)
      return;
   if (!result)
      return;

#ifdef HAVE_LIBRETRODB
   if (filebrowser_get_type() == FILEBROWSER_SCAN_FILE)
      action_scan_file(state->result, NULL, 0, 0);
   else
#endif
   {
      path_set(RARCH_PATH_CONTENT, state->result);

      if (core_name && *core_name)
      {
         content_ctx_info_t content_info = {0};
         task_push_load_content_with_current_core_from_companion_ui(
                                                                    NULL,
                                                                    &content_info,
                                                                    CORE_TYPE_PLAIN,
                                                                    NULL, NULL);
      }
      else
         cocoa_file_load_with_detect_core(state->result);
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

#pragma mark - Programmatic Menu Creation

static NSMenuItem *cocoa_menu_item_with_action(NSString *title,
      SEL action, NSString *keyEquiv, NSUInteger mods, id target, NSInteger tag)
{
   NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:action
                                           keyEquivalent:keyEquiv];
   if (mods)
      [item setKeyEquivalentModifierMask:mods];
   if (target)
      [item setTarget:target];
   if (tag)
      [item setTag:tag];
   RARCH_AUTORELEASE(item);
   return item;
}

static NSMenu *cocoa_create_app_menu(id delegate)
{
   NSMenu *menu = [[NSMenu alloc] initWithTitle:@"RetroArch"];

   [menu addItem:cocoa_menu_item_with_action(@"About RetroArch",
         @selector(orderFrontStandardAboutPanel:), @"", 0, NSApp, 0)];
   [menu addItem:[NSMenuItem separatorItem]];

   NSMenuItem *servicesItem = [[NSMenuItem alloc] initWithTitle:@"Services"
                                                          action:nil
                                                   keyEquivalent:@""];
   NSMenu *servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
   [NSApp setServicesMenu:servicesMenu];
   [servicesItem setSubmenu:servicesMenu];
   [menu addItem:servicesItem];
   RARCH_RELEASE(servicesItem);
   RARCH_RELEASE(servicesMenu);

   [menu addItem:[NSMenuItem separatorItem]];
   [menu addItem:cocoa_menu_item_with_action(@"Hide RetroArch",
         @selector(hide:), @"h", NSEventModifierFlagCommand, nil, 0)];
   [menu addItem:cocoa_menu_item_with_action(@"Hide Others",
         @selector(hideOtherApplications:), @"h",
         NSEventModifierFlagOption | NSEventModifierFlagCommand, nil, 0)];
   [menu addItem:cocoa_menu_item_with_action(@"Show All",
         @selector(unhideAllApplications:), @"", 0, nil, 0)];
   [menu addItem:[NSMenuItem separatorItem]];
   [menu addItem:cocoa_menu_item_with_action(@"Quit RetroArch",
         @selector(terminate:), @"q", NSEventModifierFlagCommand, NSApp, 0)];

   RARCH_AUTORELEASE(menu);
   return menu;
}

static NSMenu *cocoa_create_file_menu(id delegate)
{
   NSMenu *menu = [[NSMenu alloc] initWithTitle:@"File"];

   [menu addItem:cocoa_menu_item_with_action(@"Load Core...",
         @selector(openCore:), @"", 0, delegate, 0)];
   [menu addItem:cocoa_menu_item_with_action(@"Load Content...",
         @selector(openDocument:), @"o", NSEventModifierFlagCommand, nil, 0)];

   NSMenuItem *recentItem = [[NSMenuItem alloc] initWithTitle:@"Open Recent"
                                                        action:nil
                                                 keyEquivalent:@""];
   NSMenu *recentMenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
   [recentItem setSubmenu:recentMenu];
   [menu addItem:recentItem];
   NSMenuItem *clearItem = cocoa_menu_item_with_action(@"Clear Menu",
         @selector(clearRecentDocuments:), @"", 0, nil, 0);
   [recentMenu addItem:clearItem];
   RARCH_RELEASE(recentItem);
   RARCH_RELEASE(recentMenu);

   [menu addItem:[NSMenuItem separatorItem]];
   [menu addItem:cocoa_menu_item_with_action(@"Close",
         @selector(performClose:), @"w", NSEventModifierFlagCommand, nil, 0)];

   RARCH_AUTORELEASE(menu);
   return menu;
}

static NSMenu *cocoa_create_command_menu(id delegate)
{
   NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Command"];

   /* Audio Options submenu */
   NSMenuItem *audioItem = [[NSMenuItem alloc] initWithTitle:@"Audio Options"
                                                       action:nil
                                                keyEquivalent:@""];
   NSMenu *audioMenu = [[NSMenu alloc] initWithTitle:@"Audio Options"];
   [audioMenu addItem:cocoa_menu_item_with_action(@"Mute Toggle",
         @selector(basicEvent:), @"", 0, delegate, 22)];
   [audioItem setSubmenu:audioMenu];
   [menu addItem:audioItem];
   RARCH_RELEASE(audioItem);
   RARCH_RELEASE(audioMenu);

   /* Disk Options submenu */
   NSMenuItem *diskItem = [[NSMenuItem alloc] initWithTitle:@"Disk Options"
                                                      action:nil
                                               keyEquivalent:@""];
   NSMenu *diskMenu = [[NSMenu alloc] initWithTitle:@"Disk Options"];
   [diskMenu addItem:cocoa_menu_item_with_action(@"Cycle Tray",
         @selector(basicEvent:), @"", 0, delegate, 4)];
   [diskMenu addItem:cocoa_menu_item_with_action(@"Next Disk",
         @selector(basicEvent:), @"", 0, delegate, 6)];
   [diskMenu addItem:cocoa_menu_item_with_action(@"Previous Disk",
         @selector(basicEvent:), @"", 0, delegate, 5)];
   [diskItem setSubmenu:diskMenu];
   [menu addItem:diskItem];
   RARCH_RELEASE(diskItem);
   RARCH_RELEASE(diskMenu);

   /* Mouse Options submenu */
   NSMenuItem *mouseItem = [[NSMenuItem alloc] initWithTitle:@"Mouse Options"
                                                       action:nil
                                                keyEquivalent:@""];
   NSMenu *mouseMenu = [[NSMenu alloc] initWithTitle:@"Mouse Options"];
   [mouseMenu addItem:cocoa_menu_item_with_action(@"Mouse Grab Toggle",
         @selector(basicEvent:), @"", 0, delegate, 7)];
   [mouseItem setSubmenu:mouseMenu];
   [menu addItem:mouseItem];
   RARCH_RELEASE(mouseItem);
   RARCH_RELEASE(mouseMenu);

   /* Save State Options submenu */
   NSMenuItem *stateItem = [[NSMenuItem alloc] initWithTitle:@"Save State Options"
                                                       action:nil
                                                keyEquivalent:@""];
   NSMenu *stateMenu = [[NSMenu alloc] initWithTitle:@"Save State Options"];
   [stateMenu addItem:cocoa_menu_item_with_action(@"Load State",
         @selector(basicEvent:), @"", 0, delegate, 2)];
   [stateMenu addItem:cocoa_menu_item_with_action(@"Save State",
         @selector(basicEvent:), @"", 0, delegate, 3)];
   [stateItem setSubmenu:stateMenu];
   [menu addItem:stateItem];
   RARCH_RELEASE(stateItem);
   RARCH_RELEASE(stateMenu);

   [menu addItem:cocoa_menu_item_with_action(@"Reset",
         @selector(basicEvent:), @"", 0, delegate, 1)];
   [menu addItem:cocoa_menu_item_with_action(@"Menu Toggle",
         @selector(basicEvent:), @"", 0, delegate, 8)];
   [menu addItem:cocoa_menu_item_with_action(@"Pause Toggle",
         @selector(basicEvent:), @"", 0, delegate, 9)];
   [menu addItem:cocoa_menu_item_with_action(@"Take Screenshot",
         @selector(basicEvent:), @"", 0, delegate, 21)];

   RARCH_AUTORELEASE(menu);
   return menu;
}

static NSMenu *cocoa_create_paths_menu(id delegate)
{
   NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Paths"];
   [menu addItem:cocoa_menu_item_with_action(@"Core Directory",
         @selector(showCoresDirectory:), @"", 0, delegate, 0)];
   RARCH_AUTORELEASE(menu);
   return menu;
}

static NSMenu *cocoa_create_window_menu(id delegate)
{
   NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Window"];

   [menu addItem:cocoa_menu_item_with_action(@"Minimize",
         @selector(performMiniaturize:), @"m", NSEventModifierFlagCommand, nil, 0)];
   [menu addItem:cocoa_menu_item_with_action(@"Zoom",
         @selector(performZoom:), @"", 0, nil, 0)];

   /* Windowed Scale submenu */
   NSMenuItem *scaleItem = [[NSMenuItem alloc] initWithTitle:@"Windowed Scale"
                                                       action:nil
                                                keyEquivalent:@""];
   NSMenu *scaleMenu = [[NSMenu alloc] initWithTitle:@"Windowed Scale"];
   int i;
   for (i = 1; i <= 10; i++)
   {
      NSString *title = [NSString stringWithFormat:@"%dx", i];
      [scaleMenu addItem:cocoa_menu_item_with_action(title,
            @selector(basicEvent:), @"", 0, delegate, 9 + i)];
   }
   [scaleItem setSubmenu:scaleMenu];
   [menu addItem:scaleItem];
   RARCH_RELEASE(scaleItem);
   RARCH_RELEASE(scaleMenu);

   [menu addItem:cocoa_menu_item_with_action(@"Enter Full Screen",
         @selector(toggleFullScreen:), @"f",
         NSEventModifierFlagControl | NSEventModifierFlagCommand, nil, 0)];
   [menu addItem:cocoa_menu_item_with_action(@"Toggle Exclusive Full Screen",
         @selector(basicEvent:), @"", 0, delegate, 20)];
   [menu addItem:[NSMenuItem separatorItem]];
   [menu addItem:cocoa_menu_item_with_action(@"Bring All to Front",
         @selector(arrangeInFront:), @"", 0, nil, 0)];

   [NSApp setWindowsMenu:menu];

   RARCH_AUTORELEASE(menu);
   return menu;
}

static NSMenu *cocoa_create_help_menu(void)
{
   NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Help"];
   [menu addItem:cocoa_menu_item_with_action(@"RetroArch Help",
         @selector(showHelp:), @"?", NSEventModifierFlagCommand, nil, 0)];
   /* -[NSApplication setHelpMenu:] is 10.6+.  Runtime-guard so we
    * don't crash with "unrecognized selector" on 10.5 Leopard.  The
    * help menu still appears in the menu bar via setMainMenu; this
    * call is only about telling AppKit which one to route Spotlight-
    * for-Help into. */
   if ([NSApp respondsToSelector:@selector(setHelpMenu:)])
      [NSApp setHelpMenu:menu];
   RARCH_AUTORELEASE(menu);
   return menu;
}

static void cocoa_create_menu_bar(id delegate)
{
   NSMenu *menubar = [[NSMenu alloc] init];
   NSMenuItem *item;
   NSMenu *submenu;

   /* RetroArch (Apple) menu */
   item = [[NSMenuItem alloc] init];
   submenu = cocoa_create_app_menu(delegate);
   [item setSubmenu:submenu];
   [menubar addItem:item];
   RARCH_RELEASE(item);

   /* File menu */
   item = [[NSMenuItem alloc] init];
   [item setSubmenu:cocoa_create_file_menu(delegate)];
   [menubar addItem:item];
   RARCH_RELEASE(item);

   /* Command menu */
   item = [[NSMenuItem alloc] init];
   [item setSubmenu:cocoa_create_command_menu(delegate)];
   [menubar addItem:item];
   RARCH_RELEASE(item);

   /* Paths menu */
   item = [[NSMenuItem alloc] init];
   [item setSubmenu:cocoa_create_paths_menu(delegate)];
   [menubar addItem:item];
   RARCH_RELEASE(item);

   /* Window menu */
   item = [[NSMenuItem alloc] init];
   [item setSubmenu:cocoa_create_window_menu(delegate)];
   [menubar addItem:item];
   RARCH_RELEASE(item);

   /* Help menu */
   item = [[NSMenuItem alloc] init];
   [item setSubmenu:cocoa_create_help_menu()];
   [menubar addItem:item];
   RARCH_RELEASE(item);

   [NSApp setMainMenu:menubar];
   RARCH_RELEASE(menubar);
}

static NSWindow *cocoa_create_main_window(void)
{
   NSUInteger style = NSWindowStyleMaskTitled
                    | NSWindowStyleMaskClosable
                    | NSWindowStyleMaskMiniaturizable
                    | NSWindowStyleMaskResizable;
   NSRect frame = NSMakeRect(0, 0, 480, 360);

   /* RAWindow's -sendEvent: override is what feeds keyboard/mouse
    * events into the cocoa_input driver. */
   NSWindow *window = [[RAWindow alloc] initWithContentRect:frame
                                                  styleMask:style
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
   [window setTitle:@"RetroArch"];
   [window setReleasedWhenClosed:NO];
   [window setAllowsToolTipsWhenApplicationIsInactive:NO];
   [window center];
   return window;
}

int main(int argc, char *argv[])
{
   RetroArch_OSX *delegate;
   NSWindow *window;

#ifndef NDEBUG
   task_set_exception_ports(mach_task_self(), EXC_MASK_BAD_ACCESS, MACH_PORT_NULL, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
#endif

   if (argc == 2)
   {
       if (argv[1])
           if (!strncmp(argv[1], "-psn", 4))
               argc = 1;
   }

   waiting_argc = argc;
   waiting_argv = argv;

   RARCH_AUTORELEASEPOOL_BEGIN
      [NSApplication sharedApplication];
      /* A bare-binary build - no .app bundle, no Info.plist for the
       * WindowServer to consult - starts as a background-only process
       * and NEVER RECEIVES KEYSTROKES: mouse events reach the window,
       * key events go to whatever is actually frontmost.  It has to be
       * promoted to a regular GUI app.  -setActivationPolicy: is the
       * official way from 10.6; before that the Carbon Process Manager
       * call TransformProcessType() does the same, from 10.3, and
       * still exists today.  Asked at runtime rather than decided by
       * the build SDK, so one binary does the right thing on whatever
       * it lands on.  The message is sent through objc_msgSend with the
       * enum's value spelled out, so the file does not need the SDK to
       * declare the method; NSApplicationActivationPolicyRegular is 0. */
      if ([NSApp respondsToSelector:@selector(setActivationPolicy:)])
         ((void (*)(id, SEL, NSInteger))objc_msgSend)(NSApp,
               @selector(setActivationPolicy:), (NSInteger)0);
      else
      {
         ProcessSerialNumber psn = { 0, kCurrentProcess };
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
         TransformProcessType(&psn, kProcessTransformToForegroundApplication);
#pragma GCC diagnostic pop
      }

      delegate = [[RetroArch_OSX alloc] init];
      window = cocoa_create_main_window();
      [delegate setWindow:window];
      [NSApp setDelegate:delegate];

      cocoa_create_menu_bar(delegate);

      [window makeKeyAndOrderFront:nil];
      [NSApp activateIgnoringOtherApps:YES];
      [NSApp run];
   RARCH_AUTORELEASEPOOL_END
   return 0;
}

static void ui_companion_cocoa_deinit(void *data)
{
   [[NSApplication sharedApplication] terminate:nil];
}

static void *ui_companion_cocoa_init(void) { return (void*)-1; }
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
static void *ui_companion_cocoa_get_main_window(void *data)
{
    return (BRIDGE void *)((RetroArch_OSX*)[[NSApplication sharedApplication] delegate]).window;
}

ui_companion_driver_t ui_companion_cocoa = {
   ui_companion_cocoa_init,
   ui_companion_cocoa_deinit,
   ui_companion_cocoa_toggle,
   NULL, /* iterate */
   ui_companion_cocoa_event_command,
   NULL, /* notify_refresh */
   NULL, /* msg_queue_push */
   NULL, /* render_messagebox */
   ui_companion_cocoa_get_main_window,
   NULL, /* log_msg */
   NULL, /* is_active */
   NULL, /* get_app_icons */
   NULL, /* set_app_icon */
   NULL, /* get_app_icon_texture */
   &ui_browser_window_cocoa,
   &ui_msg_window_cocoa,
   &ui_window_cocoa,
   &ui_application_cocoa,
   "cocoa",
};
