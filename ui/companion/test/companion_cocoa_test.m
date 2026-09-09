/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* Regression harness for the Cocoa companion (ui/drivers/
 * ui_cocoa_companion.m), run on Linux against GNUstep's AppKit under
 * Xvfb by tools/companion_cocoa_test.sh. The real driver file is
 * compiled and linked - the same code that runs on macOS, minus what
 * differs between the two AppKits - together with the real companion
 * core and the core test's stubs and fixtures.
 *
 * The harness drives the driver the way RetroArch does: init, iterate
 * until the playlist lands, then acts on the controller exactly as a
 * user would through the widgets (the view popup, the File Browser
 * tab, closing the window) and asserts on the AppKit objects
 * themselves: is the grid the scroll view's document, does it have a
 * frame and a count, does the folder table hand back names, does
 * closing make RetroArch's window key with the render view as first
 * responder.
 *
 * What GNUstep cannot stand in for: Metal, the real WindowServer's
 * key-window arbitration, fonts on a Mac. What it can: the whole
 * AppKit object graph, layout, table data sources, first responder
 * plumbing - which is where every bug so far has been. */

#import <AppKit/AppKit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../../../configuration.h"
#include "../../../version.h"
#include "../../../ui/ui_companion_driver.h"
#include "../../../ui/companion/companion_core.h"
#include "../../../runloop.h"
#include "../../../core_option_manager.h"
#include <file/file_path.h>
extern runloop_state_t test_runloop;
#include "../../../ui/drivers/cocoa/cocoa_common.h"
#include "../../../ui/drivers/cocoa/apple_platform.h"

/* --- the platform, stubbed ----------------------------------------------- */

/* The harness's stand-in for RetroArch_OSX: a window with a render view,
 * so the companion's "hand the keyboard back" path has a real target
 * to make key and first responder. */
/* RetroArch's RAWindow reads the keyboard in -sendEvent:, which AppKit
 * calls on the KEY window for key events. This stand-in counts the key
 * events that reach it: after the companion closes, a posted keystroke
 * must arrive here or RetroArch has no keyboard. */
@interface HarnessWindow : NSWindow
{
@public
   int keyEvents;
}
@end
@implementation HarnessWindow
- (void)sendEvent:(NSEvent*)event
{
   if ([event type] == NSKeyDown || [event type] == NSKeyUp)
      keyEvents++;
   [super sendEvent:event];
}
@end

/* The render view, as CocoaView / the Metal view: takes first responder. */
@interface HarnessRenderView : NSView
@end
@implementation HarnessRenderView
- (BOOL)acceptsFirstResponder { return YES; }
@end

@interface HarnessPlatform : NSObject
{
   HarnessWindow *win;
   NSView   *rv;
}
- (NSWindow *)hostWindow;
- (id)renderView;
@end

@implementation HarnessPlatform
- (id)init
{
   if ((self = [super init]))
   {
      win = [[HarnessWindow alloc] initWithContentRect:NSMakeRect(50, 50, 640, 480)
         styleMask:(NSTitledWindowMask | NSClosableWindowMask)
         backing:NSBackingStoreBuffered defer:NO];
      rv  = [[HarnessRenderView alloc] initWithFrame:NSMakeRect(0, 0, 640, 480)];
      [win setContentView:rv];
      [win setTitle:@"RetroArch (harness)"];
   }
   return self;
}
- (NSWindow *)hostWindow { return win; }
- (id)renderView { return rv; }
@end

id<ApplePlatform> apple_platform = nil;

/* RetroArch's keyboard state (input/drivers/cocoa_input.m): showing the
 * companion must forget held keys, or a key released into the
 * companion stays "down" for RetroArch's menu. */
int stub_keyboard_resets;
void apple_input_keyboard_reset(void) { stub_keyboard_resets++; }

/* CocoaView: the driver reaches for +get; return a view in no window,
 * exactly the situation on a Metal build that broke focus before. */
@implementation CocoaView
+ (CocoaView*)get
{
   static CocoaView *v = nil;
   if (!v)
      v = [[CocoaView alloc] initWithFrame:NSMakeRect(0, 0, 8, 8)];
   return v;
}
@end

extern settings_t test_settings;
extern size_t stub_core_count;

/* The driver under test (its global driver struct). */
extern ui_companion_driver_t ui_companion_wimp_cocoa;

/* Peek into the driver's private state: the struct is private to the
 * .m, but its first two fields are stable and documented there. */
struct wimp_peek { companion_core_t *core; void *controller; };

/* The controller's dock methods this harness drives, declared as an
 * informal category so the calls compile against the private class. */
@interface NSObject (RACompanionDockTesting)
- (int)paneViews:(int)pane into:(NSView**)views max:(int)max;
- (void)dockMouseDown:(NSPoint)pt clicks:(NSInteger)clicks;
- (void)dockMouseDragged:(NSPoint)pt;
- (void)dockMouseUp:(NSPoint)pt;
- (void)redockPane:(int)pane;
- (void)floatDragUpdate:(int)pane screenPoint:(NSPoint)sp;
- (void)floatDragEnd:(int)pane;
- (void)showClosedDock:(id)sender;
- (void)menuNeedsUpdate:(NSMenu*)menu;
@end

/* The first view of dock pane @pane (companion_dock_id), through the
 * controller's -paneViews:into:max:. */
static NSView *pane_view(id ctrl, int pane)
{
   NSView *vs[8];
   int n = [ctrl paneViews:pane into:vs max:8];
   return n > 0 ? vs[0] : nil;
}

/* A press, moves and a release on the dock view, in its coordinates. */
static void dock_drag(id ctrl, NSPoint from, NSPoint to)
{
   [ctrl dockMouseDown:from clicks:1];
   [ctrl dockMouseDragged:NSMakePoint(from.x + 20, from.y + 20)];
   [ctrl dockMouseDragged:to];
   [ctrl dockMouseUp:to];
}

static void dock_click(id ctrl, NSPoint at)
{
   [ctrl dockMouseDown:at clicks:1];
   [ctrl dockMouseUp:at];
}

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } else { printf("[ok] "); printf(__VA_ARGS__); printf("\n"); } fflush(stdout); } while (0)

/* Pump AppKit and the driver for a while (the core's async work lands
 * through iterate; AppKit's deferred layout through the run loop). */
static void pump(void *data, int ms)
{
   int i;
   for (i = 0; i < ms / 5; i++)
   {
      NSAutoreleasePool *p = [[NSAutoreleasePool alloc] init];
      NSEvent *ev;
      ui_companion_wimp_cocoa.iterate(data);
      while ((ev = [NSApp nextEventMatchingMask:NSAnyEventMask
               untilDate:[NSDate dateWithTimeIntervalSinceNow:0.001]
               inMode:NSDefaultRunLoopMode dequeue:YES]))
         [NSApp sendEvent:ev];
      [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.004]];
      [p drain];
   }
}

/* Find a subview of a class, depth-first. */
static id find_view(NSView *root, Class cls)
{
   NSUInteger i;
   if ([root isKindOfClass:cls])
      return root;
   for (i = 0; i < [[root subviews] count]; i++)
   {
      id r = find_view([[root subviews] objectAtIndex:i], cls);
      if (r)
         return r;
   }
   return nil;
}

/* The controller's scroll views, by which document they hold. */
static NSScrollView *scroll_holding(NSView *root, Class docCls)
{
   NSUInteger i;
   if ([root isKindOfClass:[NSScrollView class]]
         && [[(NSScrollView*)root documentView] isKindOfClass:docCls])
      return (NSScrollView*)root;
   for (i = 0; i < [[root subviews] count]; i++)
   {
      NSScrollView *r = scroll_holding([[root subviews] objectAtIndex:i], docCls);
      if (r)
         return r;
   }
   return nil;
}

extern void companion_test_setup_fixtures(char *root, size_t len);
extern void companion_test_teardown_fixtures(const char *root);

#include <signal.h>
static void on_alarm(int sig) { (void)sig; printf("FAIL: harness timed out (hang)\n"); fflush(stdout); _exit(3); }

/* A fault (SIGTRAP is what a libdispatch or runtime assertion raises on
 * macOS; SIGSEGV / SIGBUS / SIGABRT the rest): print a backtrace so the
 * CI log says where, then fail. */
#include <execinfo.h>
static void on_fault(int sig)
{
   void *frames[40];
   int n = backtrace(frames, 40);
   printf("FAIL: fault (signal %d); backtrace:\n", sig);
   fflush(stdout);
   backtrace_symbols_fd(frames, n, 1);
   _exit(5);
}

/* An uncaught Objective-C exception is a trap with no message on macOS
 * ("Trace/BPT trap"): print what it was and where, then fail. */
static void on_exception(NSException *e)
{
   printf("FAIL: uncaught exception %s: %s\n",
         [[e name] UTF8String], [[e reason] UTF8String]);
   if ([e respondsToSelector:@selector(callStackSymbols)])
   {
      NSArray *syms = [e performSelector:@selector(callStackSymbols)];
      NSUInteger i;
      for (i = 0; i < [syms count] && i < 25; i++)
         printf("   %s\n", [[syms objectAtIndex:i] UTF8String]);
   }
   fflush(stdout);
   _exit(4);
}

int main(int argc, char **argv)
{
   char root[512];
   void *data;
   struct wimp_peek *peek;
   id ctrl;
   NSWindow *win;
   NSView *content;
   NSPopUpButton *viewPopup = nil;
   NSTabView *tabs = nil;
   NSTableView *leftTable = nil;
   NSScrollView *entriesScroll = nil;
   Class gridCls;
   (void)argc; (void)argv;

   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   signal(SIGALRM, on_alarm);
   alarm(90);
   NSSetUncaughtExceptionHandler(on_exception);
   signal(SIGTRAP, on_fault);
   signal(SIGSEGV, on_fault);
   signal(SIGBUS,  on_fault);
   signal(SIGABRT, on_fault);
   signal(SIGILL,  on_fault);
   [NSApplication sharedApplication];
   /* A plain executable: become a regular, activatable app (Apple's
    * AppKit will not give a background process a key window). */
   if ([NSApp respondsToSelector:@selector(setActivationPolicy:)])
      [NSApp setActivationPolicy:0 /* NSApplicationActivationPolicyRegular */];
   [NSApp finishLaunching];
   [NSApp activateIgnoringOtherApps:YES];
   apple_platform = (id<ApplePlatform>)[[HarnessPlatform alloc] init];
   [[(id)apple_platform hostWindow] makeKeyAndOrderFront:nil];

   companion_test_setup_fixtures(root, sizeof(root));
   test_settings.bools.ui_companion_toggle    = true;
   test_settings.uints.desktop_menu_view_type = 0;          /* start in list view */

   data = ui_companion_wimp_cocoa.init();
   CHECK(data != NULL, "driver init (window built)");
   if (!data)
      return 1;
   peek = (struct wimp_peek*)data;
   ctrl = (id)peek->controller;
   win  = [ctrl performSelector:@selector(window)];
   CHECK(win != nil, "controller has a window");
   /* The panes and the content view live in the docks' surface (a
    * RACompanionDockView under the content view). */
   content = [ctrl valueForKey:@"dockView"];
   CHECK(content != nil && [content superview] == [win contentView], "dock view holds the panes");
   {
      /* showing the companion takes the keyboard: RetroArch's held keys
       * must be forgotten, or a release into this window leaves the
       * menu waiting on a key that is "still down" */
      int before = stub_keyboard_resets;
      ui_companion_wimp_cocoa.toggle(data, true);
      pump(data, 200);
      CHECK(stub_keyboard_resets > before, "showing the companion resets RetroArch's keyboard state (apple_input_keyboard_reset)");
   }

   /* Playlist lands through iterate */
   pump(data, 400);
   CHECK(companion_core_playlist_count(peek->core) >= 3, "playlists listed (%u)", (unsigned)companion_core_playlist_count(peek->core));
   companion_core_select_playlist(peek->core, 2);           /* NES */
   pump(data, 600);
   CHECK(companion_core_entry_count(peek->core) == 3, "playlist entries landed (%u)", (unsigned)companion_core_entry_count(peek->core));

   /* --- the widgets the user drives --- */
   {
      NSUInteger i;
      for (i = 0; i < [[content subviews] count]; i++)
      {
         id v = [[content subviews] objectAtIndex:i];
         /* by action, not item count: the core popup also has two items
          * (Ask, Load Core...) when no core is installed */
         if ([v isKindOfClass:[NSPopUpButton class]]
               && [(NSPopUpButton*)v action] == NSSelectorFromString(@"viewChanged:"))
            viewPopup = v;                                   /* List / Icons */
         if ([v isKindOfClass:[NSTabView class]])
            tabs = v;
      }
   }
   CHECK(viewPopup != nil, "view popup found");
   CHECK(tabs != nil && [tabs numberOfTabViewItems] == 2, "Playlists | File Browser tabs");
   gridCls = NSClassFromString(@"RACompanionGrid");
   CHECK(gridCls != Nil, "grid class present");
   entriesScroll = nil;
   {
      NSUInteger i;
      for (i = 0; i < [[content subviews] count]; i++)
      {
         id v = [[content subviews] objectAtIndex:i];
         if ([v isKindOfClass:[NSScrollView class]])
         {
            id doc = [(NSScrollView*)v documentView];
            if ([doc isKindOfClass:[NSTableView class]]
                  && [[(NSTableView*)doc tableColumns] count] >= 2
                  && [[[[(NSTableView*)doc tableColumns] objectAtIndex:0] identifier] isEqualToString:@"name"]
                  && [[[[(NSTableView*)doc tableColumns] objectAtIndex:1] identifier] isEqualToString:@"core"])
               entriesScroll = v;
         }
      }
   }
   leftTable = nil;
   {
      /* the left table: a table whose first column is the icon column */
      NSUInteger i;
      for (i = 0; i < [[content subviews] count]; i++)
      {
         id v = [[content subviews] objectAtIndex:i];
         if ([v isKindOfClass:[NSScrollView class]])
         {
            id doc = [(NSScrollView*)v documentView];
            if ([doc isKindOfClass:[NSTableView class]]
                  && [[[[(NSTableView*)doc tableColumns] objectAtIndex:0] identifier] isEqualToString:@"icon"])
               leftTable = doc;
         }
      }
   }
   CHECK(leftTable != nil, "playlist / folder table found");
   CHECK([leftTable numberOfRows] >= 3, "playlist table has rows (%ld)", (long)[leftTable numberOfRows]);
   CHECK([[leftTable superview] superview] != nil && ![[[leftTable superview] superview] isHidden], "playlist table is in the window and visible");

   /* --- Icons view --- */
   [viewPopup selectItemAtIndex:1];
   printf("  popup: items=%ld selected=%ld\n", (long)[viewPopup numberOfItems], (long)[viewPopup indexOfSelectedItem]); fflush(stdout);
   [viewPopup sendAction:[viewPopup action] to:[viewPopup target]];
   printf("  action returned\n"); fflush(stdout);
   pump(data, 300);
   printf("  pumped\n"); fflush(stdout);
   printf("  entries scroll document class: %s\n", [[[[entriesScroll documentView] class] description] UTF8String]);
   {
      NSScrollView *gs = scroll_holding(content, gridCls);
      CHECK(gs != nil, "Icons: the grid is a scroll view's document");
      if (gs)
      {
         NSView *grid = [gs documentView];
         NSRect f = [grid frame];
         NSInteger n = (NSInteger)[[grid valueForKey:@"count"] integerValue];
         CHECK(f.size.width > 100 && f.size.height > 50, "grid has a frame (%.0f x %.0f)", f.size.width, f.size.height);
         CHECK([grid isKindOfClass:gridCls], "document is the grid");
         CHECK(n == 3, "grid count is the playlist's 3 entries (got %ld)", (long)n);
         /* paint it: the draw path must run without faulting */
         [grid setNeedsDisplay:YES];
         [grid displayIfNeeded];
         pump(data, 200);
         CHECK(1, "grid painted");
      }
   }

   /* --- the icon grid's prefetch must not read past the list ---
    * iconTick asks for a screen either side of the visible rows, so it
    * reaches rows that do not exist; thumbNone is only as long as the
    * list. Under ASan (and Apple's Guard Malloc) reading past it is a
    * crash - this is what took the app down on macOS. Drive iconTick
    * repeatedly with a short list, and hit thumbWant: directly with
    * rows either side of the range. */
   {
      SEL sTick = NSSelectorFromString(@"iconTick");
      SEL sWant = NSSelectorFromString(@"thumbWant:urgent:");
      NSInteger n = (NSInteger)companion_core_entry_count(peek->core);
      NSInteger k;
      CHECK([ctrl respondsToSelector:sTick] && [ctrl respondsToSelector:sWant], "iconTick / thumbWant: present");
      for (k = 0; k < 8; k++)
      {
         [ctrl performSelector:sTick];
         pump(data, 20);
      }
      for (k = -3; k <= n + 3; k++)
      {
         NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[ctrl methodSignatureForSelector:sWant]];
         BOOL urgent = YES;
         [inv setSelector:sWant]; [inv setTarget:ctrl];
         [inv setArgument:&k atIndex:2];
         [inv setArgument:&urgent atIndex:3];
         [inv invoke];
      }
      pump(data, 100);
      CHECK(1, "prefetch and out-of-range rows leave the thumbnail marks alone (%ld entries)", (long)n);
   }

   /* --- File Browser tab --- */
   [tabs selectTabViewItemAtIndex:1];
   pump(data, 800);
   {
      const char *dir = companion_core_browse_dir(peek->core);
      CHECK(dir && *dir, "browse listing landed (dir=%s)", dir ? dir : "-");
      CHECK(companion_core_browse_count(peek->core) >= 2, "browse count %u", (unsigned)companion_core_browse_count(peek->core));
      CHECK([leftTable numberOfRows] == (NSInteger)companion_core_browse_dir_count(peek->core), "folder pane rows = dir_count (%ld vs %u)", (long)[leftTable numberOfRows], (unsigned)companion_core_browse_dir_count(peek->core));
      CHECK(![[[leftTable superview] superview] isHidden] && [[leftTable superview] superview] != nil, "folder table still visible under the File Browser tab");
      if ([leftTable numberOfRows] > 1)
      {
         id ds = [leftTable dataSource];
         NSTableColumn *nameCol = [[leftTable tableColumns] objectAtIndex:1];
         id v0 = [ds tableView:leftTable objectValueForTableColumn:nameCol row:0];
         id v1 = [ds tableView:leftTable objectValueForTableColumn:nameCol row:1];
         CHECK([v0 isKindOfClass:[NSString class]] && [(NSString*)v0 length] > 0, "folder row 0 has text: '%s'", [[v0 description] UTF8String]);
         CHECK([v1 isKindOfClass:[NSString class]] && [(NSString*)v1 length] > 0, "folder row 1 has text: '%s'", [[v1 description] UTF8String]);
         CHECK([[v0 description] isEqualToString:@".."], "row 0 is '..' (got '%s')", [[v0 description] UTF8String]);
      }
      {
         NSTableView *files = (NSTableView*)[entriesScroll documentView];
         CHECK([files isKindOfClass:[NSTableView class]], "browser content is the table");
         CHECK([files numberOfRows] == (NSInteger)companion_core_browse_count(peek->core), "content rows = browse count (%ld)", (long)[files numberOfRows]);
      }
   }

   /* --- audit: every user action the controller implements, fired in
    *     the state a user would fire it from, on the real objects ---
    * Reported, not asserted: the point is the gap list. */
   if (getenv("COMPANION_AUDIT"))
   {
      struct { const char *sel; const char *note; } acts[] = {
         { "refreshPlaylists:", "F5 / reload playlists" },
         { "viewList:",         "View > List" },
         { "viewIcons:",        "View > Icons" },
         { "zoomChanged:",      "zoom slider" },
         { "thumbTypeChanged:", "thumbnail type popup" },
         { "boxartTypeChanged:","boxart segment" },
         { "toggleInfo:",       "View > Core Info dock" },
         { "toggleBoxart:",     "View > Boxart dock" },
         { "toggleLog:",        "View > Log dock" },
         { "focusSearch:",      "Edit > Search" },
         { "searchChanged:",    "search field edited" },
         { "clearSearch:",      "search Clear" },
         { "browseFiles:",      "File Browser tab" },
         { "browseUp:",         "browser Up" },
         { "browseStart:",      "browser Start Directory" },
         { "browseDownloads:",  "browser Downloads" },
         { "playlistsDoubleClick:", "double-click playlist / folder" },
         { "corePopupChanged:", "core popup changed" },
         { "runSelected:",      "Run" },
         { "runWithPopup:",     "run with the popup's core" },
         { "startCore:",        "Start Core (no content)" },
         { "loadCore:",         "File > Load Core (picker)" },
         { "loadSelectedCore:", "picker: Load" },
         { "cancelLoadCore:",   "picker: Cancel" },
         { "scanDirectory:",    "Scan Directory" },
         { "deleteEntry:",      "context: delete entry" },
         { "associateCore:",    "context: associate core" },
         { "openDocs:",         "Help > Documentation" },
         { "aboutRetroArch:",   "Help > About (modal alert)" },
         { "stopContent:",      "Stop button" },
         { "unloadCore:",       "File > Unload Core" },
         { "quitRetroArch:",    "File > Exit RetroArch" },
         { "loadContent:",      "load content (file)" },
         { NULL, NULL }
      };
      int k;
      printf("\n=== Cocoa companion action audit ===\n");
      for (k = 0; acts[k].sel; k++)
      {
         SEL s = NSSelectorFromString([NSString stringWithUTF8String:acts[k].sel]);
         BOOL has = [ctrl respondsToSelector:s];
         const char *dlg = strstr(acts[k].sel, "loadCore") || strstr(acts[k].sel, "scanDirectory")
                        || strstr(acts[k].sel, "aboutRetroArch") || strstr(acts[k].sel, "quitRetroArch")
                        || strstr(acts[k].sel, "openDoc") || strstr(acts[k].sel, "loadContent")
                        || strstr(acts[k].sel, "startCore") || strstr(acts[k].sel, "run")
                        || strstr(acts[k].sel, "deleteEntry") || strstr(acts[k].sel, "associateCore")
                        ? " (skipped: modal / launches / destructive)" : "";
         printf("  %-24s %-36s %s%s\n", acts[k].sel, acts[k].note,
               has ? "implemented" : "MISSING", has ? dlg : "");
         if (has && !*dlg)
         {
            [ctrl performSelector:s withObject:nil];
            pump(data, 60);
         }
      }
      printf("  (state after audit: iconView doc=%s, browseMode=%s, entries rows=%ld)\n",
            [[[[entriesScroll documentView] class] description] UTF8String],
            [[ctrl valueForKey:@"browseMode"] boolValue] ? "yes" : "no",
            (long)[(NSTableView*)[entriesScroll documentView] numberOfRows]);
      fflush(stdout);
   }

   /* --- the menu / button wiring added for Qt parity --- */
   {
      extern int stub_calls_command;
      int before = stub_calls_command;
      SEL s;
      s = NSSelectorFromString(@"stopContent:");
      CHECK([ctrl respondsToSelector:s], "Stop implemented");
      [ctrl performSelector:s withObject:nil];
      pump(data, 50);
      CHECK(stub_calls_command > before, "Stop reaches the core (command_event called)");
      CHECK([ctrl respondsToSelector:NSSelectorFromString(@"unloadCore:")], "Unload Core implemented");
      CHECK([ctrl respondsToSelector:NSSelectorFromString(@"quitRetroArch:")], "Exit RetroArch implemented");
      CHECK([ctrl respondsToSelector:NSSelectorFromString(@"aboutRetroArch:")], "About implemented");
   }

   /* --- rename / add files / thumbnail drop, through the controller --- */
   {
      char p1[600], p2[600], img[600], out[600];
      NSMutableArray *paths = [NSMutableArray array];
      size_t before, after;
      SEL s;
      /* back to the playlists tab and the Genesis playlist (row 3) */
      [tabs selectTabViewItemAtIndex:0];
      pump(data, 200);
      companion_core_select_playlist(peek->core, 3);
      pump(data, 500);
      before = companion_core_entry_count(peek->core);
      CHECK(before == 2, "Genesis has 2 entries (got %u)", (unsigned)before);
      snprintf(p1, sizeof(p1), "%s/content/a.nes", root);
      snprintf(p2, sizeof(p2), "%s/content/sub", root);
      [paths addObject:[NSString stringWithUTF8String:p1]];
      [paths addObject:[NSString stringWithUTF8String:p2]];
      s = NSSelectorFromString(@"addPaths:");
      CHECK([ctrl respondsToSelector:s], "addPaths: implemented");
      {
         /* size_t return: NSInvocation, not performSelector (which is
          * for id returns) */
         NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[ctrl methodSignatureForSelector:s]];
         size_t added = 0;
         [inv setSelector:s]; [inv setTarget:ctrl];
         [inv setArgument:&paths atIndex:2];
         [inv invoke];
         [inv getReturnValue:&added];
         printf("  addPaths: returned %u\n", (unsigned)added); fflush(stdout);
      }
      pump(data, 600);
      after = companion_core_entry_count(peek->core);
      CHECK(after == before + 2, "drop of a file and a directory added 2 (got %u)", (unsigned)after);

      /* thumbnail drop onto the pane for the selected entry */
      snprintf(img, sizeof(img), "%s/drop2.tga", root);
      {
         FILE *f = fopen(img, "wb");
         uint8_t hdr[18]; int i;
         memset(hdr, 0, 18); hdr[2] = 2; hdr[12] = 8; hdr[14] = 8; hdr[16] = 32; hdr[17] = 0x28;
         fwrite(hdr, 1, 18, f);
         for (i = 0; i < 64; i++) { uint8_t px[4] = { 0x33, 0x66, 0x99, 0xff }; fwrite(px, 1, 4, f); }
         fclose(f);
      }
      [(NSTableView*)[entriesScroll documentView] selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
      pump(data, 100);
      s = NSSelectorFromString(@"installThumbnailFromPath:pane:");
      CHECK([ctrl respondsToSelector:s], "installThumbnailFromPath:pane: implemented");
      {
         NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[ctrl methodSignatureForSelector:s]];
         const char *ip = img;
         int pane = 0;
         BOOL ok = NO;
         [inv setSelector:s]; [inv setTarget:ctrl];
         [inv setArgument:&ip atIndex:2];
         [inv setArgument:&pane atIndex:3];
         [inv invoke];
         [inv getReturnValue:&ok];
         CHECK(ok, "thumbnail installed from a dropped image");
      }
      {
         const struct playlist_entry *e = companion_core_entry(peek->core, 0);
         char db[128];
         strlcpy(db, e->db_name, sizeof(db)); path_remove_extension(db);
         companion_core_thumbnail_path(peek->core, db, COMPANION_THUMB_BOXART, e->label, e->path, out, sizeof(out));
         CHECK(path_is_valid(out), "png exists at the repository path: %s", out);
      }

      /* rename through the core-backed method */
      s = NSSelectorFromString(@"renamePlaylistAtRow:to:");
      CHECK([ctrl respondsToSelector:s], "rename implemented");
      {
         NSMethodSignature *sig = [ctrl methodSignatureForSelector:s];
         NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
         NSInteger row = 3; const char *nm = "Sega - Genesis Renamed"; BOOL ok = NO;
         [inv setSelector:s]; [inv setTarget:ctrl];
         [inv setArgument:&row atIndex:2]; [inv setArgument:&nm atIndex:3];
         [inv invoke]; [inv getReturnValue:&ok];
         CHECK(ok, "rename accepted");
      }
      pump(data, 300);
      CHECK(string_is_equal(companion_core_playlist_name(peek->core, 3), "Sega - Genesis Renamed"), "list shows the new name (got %s)", companion_core_playlist_name(peek->core, 3));
      CHECK([leftTable numberOfRows] == 4, "playlist table reloaded (%ld rows)", (long)[leftTable numberOfRows]);
   }

   /* --- Core Options and Shader Parameters windows --- */
   {
      static struct retro_core_option_v2_definition defs[3];
      struct retro_core_options_v2 v2;
      char cfg[600];
      NSTableView *ot, *st;
      NSWindow *ow, *sw;
      id ds;
      extern int stub_calls_shader_apply;
      memset(defs, 0, sizeof(defs));
      defs[0].key = "test_speed"; defs[0].desc = "Speed";
      defs[0].values[0].value = "slow"; defs[0].values[0].label = "Slow";
      defs[0].values[1].value = "fast"; defs[0].values[1].label = "Fast";
      defs[0].default_value = "fast";
      defs[1].key = "test_color"; defs[1].desc = "Colour";
      defs[1].values[0].value = "rgb"; defs[1].values[1].value = "mono";
      defs[1].default_value = "rgb";
      v2.categories = NULL; v2.definitions = defs;
      snprintf(cfg, sizeof(cfg), "%s/core.opt", root);
      test_runloop.core_options = core_option_manager_new(cfg, NULL, &v2, false);

      [ctrl performSelector:NSSelectorFromString(@"showCoreOptions:") withObject:nil];
      pump(data, 200);
      ow = [ctrl valueForKey:@"optsWindow"];
      ot = [ctrl valueForKey:@"optsTable"];
      CHECK(ow && [ow isVisible] && ot, "Core Options window shown");
      CHECK([ot numberOfRows] == 2, "2 options listed (%ld)", (long)[ot numberOfRows]);
      ds = [ot dataSource];
      {
         id v = [ds tableView:ot objectValueForTableColumn:[ot tableColumnWithIdentifier:@"val"] row:0];
         CHECK([[v description] isEqualToString:@"Fast"], "value shows the default label (got %s)", [[v description] UTF8String]);
      }
      [ot selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
      [ctrl performSelector:NSSelectorFromString(@"optionCycle:") withObject:nil];
      {
         id v = [ds tableView:ot objectValueForTableColumn:[ot tableColumnWithIdentifier:@"val"] row:0];
         CHECK([[v description] isEqualToString:@"Slow"], "double-click cycles the value (got %s)", [[v description] UTF8String]);
      }
      [ctrl performSelector:NSSelectorFromString(@"optionReset:") withObject:nil];
      CHECK(companion_core_option_current(peek->core, 0) == 1, "Reset restores the default");
      [ow orderOut:nil];

      [ctrl performSelector:NSSelectorFromString(@"showShaderParams:") withObject:nil];
      pump(data, 200);
      sw = [ctrl valueForKey:@"shpWindow"];
      st = [ctrl valueForKey:@"shpTable"];
      CHECK(sw && [sw isVisible] && st, "Shader Parameters window shown");
      CHECK([st numberOfRows] == 2, "2 parameters listed (%ld)", (long)[st numberOfRows]);
      ds = [st dataSource];
      {
         id v = [ds tableView:st objectValueForTableColumn:[st tableColumnWithIdentifier:@"range"] row:0];
         CHECK([[v description] isEqualToString:@"0 .. 1 (step 0.05)"], "range column (got %s)", [[v description] UTF8String]);
      }
      /* edit the Value cell as the table would */
      [ds tableView:st setObjectValue:@"0.75" forTableColumn:[st tableColumnWithIdentifier:@"pval"] row:0];
      CHECK(companion_core_shader_param_current(peek->core, 0) == 0.75f, "editing the cell sets the parameter");
      {
         int before = stub_calls_shader_apply;
         [ctrl performSelector:NSSelectorFromString(@"shaderApply:") withObject:nil];
         CHECK(stub_calls_shader_apply == before + 1, "Apply fires the shader-apply command");
      }
      [st selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
      [ctrl performSelector:NSSelectorFromString(@"shaderReset:") withObject:nil];
      CHECK(companion_core_shader_param_current(peek->core, 0) == 0.5f, "Reset restores the initial value");
      [sw orderOut:nil];
      core_option_manager_free(test_runloop.core_options);
      test_runloop.core_options = NULL;
   }

   /* --- Options window --- */
   {
      NSWindow *ow; NSTableView *ot; id ds;
      [ctrl performSelector:NSSelectorFromString(@"showOptions:") withObject:nil];
      pump(data, 200);
      ow = [ctrl valueForKey:@"setWindow"]; ot = [ctrl valueForKey:@"setTable"];
      CHECK(ow && [ow isVisible] && ot, "Options window shown");
      CHECK([ot numberOfRows] == 13, "13 settings listed (%ld)", (long)[ot numberOfRows]);
      ds = [ot dataSource];
      test_settings.bools.desktop_menu_save_geometry = false;
      [ot selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
      [ctrl performSelector:NSSelectorFromString(@"settingActivate:") withObject:nil];
      CHECK(test_settings.bools.desktop_menu_save_geometry, "double-click toggles a bool");
      {
         id v = [ds tableView:ot objectValueForTableColumn:[ot tableColumnWithIdentifier:@"sval"] row:0];
         CHECK([[v description] isEqualToString:@"Yes"], "bool shows Yes (got %s)", [[v description] UTF8String]);
      }
      [ds tableView:ot setObjectValue:@"512" forTableColumn:[ot tableColumnWithIdentifier:@"sval"] row:7];
      CHECK(test_settings.uints.desktop_menu_thumbnail_cache_limit == 512, "editing a number sets it");
      [ds tableView:ot setObjectValue:@"Dark" forTableColumn:[ot tableColumnWithIdentifier:@"sval"] row:2];
      CHECK(test_settings.uints.desktop_menu_theme == 1, "theme set by label");
      [ow orderOut:nil];
   }

   /* --- hide / unhide, new / delete playlists --- */
   {
      NSInteger before = [leftTable numberOfRows];
      SEL s;
      const char *p3;
      char keep[512];
      [tabs selectTabViewItemAtIndex:0];
      pump(data, 200);
      p3 = companion_core_playlist_path(peek->core, 3);
      strlcpy(keep, p3 ? p3 : "", sizeof(keep));
      [leftTable selectRowIndexes:[NSIndexSet indexSetWithIndex:3] byExtendingSelection:NO];
      s = NSSelectorFromString(@"hidePlaylist:");
      CHECK([ctrl respondsToSelector:s], "Hide implemented");
      [ctrl performSelector:s withObject:nil];
      pump(data, 300);
      CHECK([leftTable numberOfRows] == before - 1, "hidden: the row is gone (%ld -> %ld)", (long)before, (long)[leftTable numberOfRows]);
      CHECK(companion_core_hidden_count(peek->core) == 1, "one hidden");
      /* the submenu builds and puts it back */
      {
         NSMenu *hm = [ctrl valueForKey:@"hiddenMenu"];
         CHECK(hm != nil, "hidden submenu exists");
         CHECK([hm numberOfItems] == 1, "one entry in it, rebuilt with the listing (%ld)", (long)[hm numberOfItems]);
         if ([hm numberOfItems] == 1)
         {
            NSMenuItem *it = [hm itemAtIndex:0];
            CHECK([[it title] length] > 0, "entry has a name (%s)", [[it title] UTF8String]);
            [ctrl performSelector:NSSelectorFromString(@"unhidePlaylist:") withObject:it];
            pump(data, 300);
         }
      }
      CHECK([leftTable numberOfRows] == before, "unhidden: the row is back (%ld)", (long)[leftTable numberOfRows]);
      CHECK(string_is_equal(companion_core_playlist_path(peek->core, 3), keep), "and in its place");

      s = NSSelectorFromString(@"createPlaylistNamed:");
      CHECK([ctrl respondsToSelector:s], "New Playlist implemented");
      {
         NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[ctrl methodSignatureForSelector:s]];
         const char *nm = "Harness List";
         BOOL ok = NO;
         [inv setSelector:s]; [inv setTarget:ctrl];
         [inv setArgument:&nm atIndex:2];
         [inv invoke]; [inv getReturnValue:&ok];
         CHECK(ok, "created");
      }
      pump(data, 300);
      CHECK([leftTable numberOfRows] == before + 1, "listed (%ld)", (long)[leftTable numberOfRows]);
      {
         NSInteger r, found = -1;
         for (r = 0; r < [leftTable numberOfRows]; r++)
            if (string_is_equal(companion_core_playlist_name(peek->core, (size_t)r), "Harness List"))
               found = r;
         CHECK(found >= 0, "found at row %ld", (long)found);
         if (found >= 0)
         {
            SEL sd = NSSelectorFromString(@"deletePlaylistAtPath:");
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[ctrl methodSignatureForSelector:sd]];
            const char *pp = companion_core_playlist_path(peek->core, (size_t)found);
            BOOL ok = NO;
            [inv setSelector:sd]; [inv setTarget:ctrl];
            [inv setArgument:&pp atIndex:2];
            [inv invoke]; [inv getReturnValue:&ok];
            CHECK(ok, "deleted");
            pump(data, 300);
            CHECK([leftTable numberOfRows] == before, "listing back to %ld", (long)before);
         }
      }
   }

   /* --- closing hands the keyboard back --- */
   {
      NSWindow *host = [(id)apple_platform hostWindow];
      [win makeKeyAndOrderFront:nil];
      pump(data, 100);
      CHECK([[win delegate] windowShouldClose:win] == YES, "windowShouldClose: lets AppKit close (YES)");
      [win close];
      pump(data, 200);
      CHECK(![win isVisible], "companion window hidden after close");
      CHECK([host isKeyWindow], "RetroArch's window is key again (keyboard goes to RAWindow -sendEvent:)");
      CHECK([host isMainWindow], "RetroArch's window is main again");
      CHECK([host firstResponder] == (NSResponder*)[(id)apple_platform renderView], "RetroArch's render view is first responder again");
      /* the real close: AppKit's own close after windowShouldClose: YES */
      [win makeKeyAndOrderFront:nil];
      pump(data, 50);
      [win performClose:nil];
      pump(data, 200);
      CHECK(![win isVisible], "companion window closed via performClose");
      CHECK([host isKeyWindow] && [host isMainWindow], "after AppKit's close: RetroArch's window key and main");
      CHECK([NSApp keyWindow] == host, "NSApp's key window is RetroArch's (got %s)",
            [NSApp keyWindow] == host ? "host" : ([NSApp keyWindow] ? [[[NSApp keyWindow] title] UTF8String] : "none"));
      /* The real question: does a keystroke reach RetroArch's window's
       * -sendEvent: now? AppKit routes key events to the key window. */
      {
         HarnessWindow *hw = (HarnessWindow*)host;
         NSEvent *kd, *ku;
         int before = hw->keyEvents;
         kd = [NSEvent keyEventWithType:NSKeyDown location:NSMakePoint(10, 10) modifierFlags:0
               timestamp:0 windowNumber:[host windowNumber] context:nil
               characters:@"x" charactersIgnoringModifiers:@"x" isARepeat:NO keyCode:7];
         ku = [NSEvent keyEventWithType:NSKeyUp location:NSMakePoint(10, 10) modifierFlags:0
               timestamp:0 windowNumber:[host windowNumber] context:nil
               characters:@"x" charactersIgnoringModifiers:@"x" isARepeat:NO keyCode:7];
         [NSApp postEvent:kd atStart:NO];
         [NSApp postEvent:ku atStart:NO];
         pump(data, 300);
         CHECK(hw->keyEvents > before, "a keystroke after the close reaches RetroArch's window (-sendEvent: saw %d key events)", hw->keyEvents - before);
      }
      /* and the driver can show it again after a real close */
      ui_companion_wimp_cocoa.toggle(data, true);
      pump(data, 200);
      CHECK([win isVisible], "toggle shows the closed window again");
      CHECK([[ctrl valueForKey:@"playlists"] numberOfRows] == 4, "and it still lists the playlists");
      [win close];
      pump(data, 100);
   }

   /* The Core popup's "Load Core..." item is an action, as in Qt: picking
    * it opens the Load Core window, and dismissing that puts the popup
    * back on its last real pick. */
   {
      NSPopUpButton *pop = [ctrl valueForKey:@"corePopup"];
      NSInteger n        = [pop numberOfItems];
      NSWindow *cw;
      stub_core_count = 12; /* a dozen fixture cores to size the picker to */
      CHECK(n >= 2 && [[pop itemAtIndex:n - 1] tag] == COMPANION_LAUNCH_LOAD_CORE,
            "popup's last item is Load Core... (%ld items)", (long)n);
      [pop selectItemAtIndex:n - 2];
      [ctrl performSelector:NSSelectorFromString(@"corePopupChanged:") withObject:pop];
      [pop selectItemAtIndex:n - 1];
      [ctrl performSelector:NSSelectorFromString(@"corePopupChanged:") withObject:pop];
      pump(data, 200);
      cw = [ctrl valueForKey:@"coresWindow"];
      CHECK(cw && [cw isVisible], "picking Load Core... opens the Load Core window");
      if (cw)
      {
         /* Sized to its list and kept on the screen: no smaller than
          * the 420x400 floor, inside the visible frame, every row
          * visible (the table no taller than its clip view), centred
          * over the companion. */
         NSRect f   = [cw frame];
         NSRect vis = [([cw screen] ? [cw screen] : [NSScreen mainScreen]) visibleFrame];
         NSRect wf  = [win frame];
         NSTableView *ct = [ctrl valueForKey:@"coresTable"];
         NSRect tf  = [ct frame];
         NSRect cf  = [[ct superview] frame];
         CHECK(f.size.width >= 420 && f.size.height >= 400, "picker no smaller than 420x400 (%.0fx%.0f)", f.size.width, f.size.height);
         CHECK(NSContainsRect(vis, f), "picker inside the visible frame");
         CHECK([ct numberOfRows] == 12 && tf.size.height <= cf.size.height + 1,
               "12 rows shown with no scrolling (table %.0f in clip %.0f)", tf.size.height, cf.size.height);
         CHECK([[[ct tableColumns] objectAtIndex:0] width] + [[[ct tableColumns] objectAtIndex:1] width] <= cf.size.width + 1,
               "columns fit the table (%.0f + %.0f in %.0f)", [[[ct tableColumns] objectAtIndex:0] width], [[[ct tableColumns] objectAtIndex:1] width], cf.size.width);
         /* Qt's chrome: no Load / Cancel buttons, a "Load Custom
          * Core..." button and a "<version> - <core>" status text. */
         {
            NSArray *subs = [[cw contentView] subviews];
            NSUInteger k;
            BOOL sawLoad = NO, sawCancel = NO, sawCustom = NO;
            NSTextField *st = [ctrl valueForKey:@"coresStatus"];
            for (k = 0; k < [subs count]; k++)
            {
               id v = [subs objectAtIndex:k];
               if (![v isKindOfClass:[NSButton class]])
                  continue;
               if ([[v title] isEqualToString:@"Load"])   sawLoad   = YES;
               if ([[v title] isEqualToString:@"Cancel"]) sawCancel = YES;
               if ([[v title] isEqualToString:[NSString stringWithUTF8String:
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE)]])
                  sawCustom = YES;
            }
            CHECK(!sawLoad && !sawCancel && sawCustom, "no Load / Cancel, a Load Custom Core... button");
            CHECK([ctrl respondsToSelector:NSSelectorFromString(@"loadCustomCore:")], "loadCustomCore: implemented");
            CHECK(st && [[st stringValue] hasPrefix:[NSString stringWithUTF8String:PACKAGE_VERSION " - "]], "status shows '<version> - <core>' (%s)", [[st stringValue] UTF8String]);
         }
         CHECK(fabs((f.origin.x + f.size.width / 2) - (wf.origin.x + wf.size.width / 2)) <= 8
               || f.origin.x <= vis.origin.x + 16 || f.origin.x + f.size.width >= vis.origin.x + vis.size.width - 16,
               "picker centred over the companion (mid %.0f vs %.0f)", f.origin.x + f.size.width / 2, wf.origin.x + wf.size.width / 2);
      }
      [ctrl performSelector:NSSelectorFromString(@"cancelLoadCore:") withObject:nil];
      pump(data, 100);
      /* Eighty cores on an 800-tall screen: the picker is clamped to the
       * visible frame and the list scrolls. */
      stub_core_count = 80;
      [ctrl performSelector:NSSelectorFromString(@"loadCore:") withObject:nil];
      pump(data, 200);
      cw = [ctrl valueForKey:@"coresWindow"];
      if (cw)
      {
         NSRect f   = [cw frame];
         NSRect vis = [([cw screen] ? [cw screen] : [NSScreen mainScreen]) visibleFrame];
         NSTableView *ct = [ctrl valueForKey:@"coresTable"];
         CHECK([cw isVisible] && NSContainsRect(vis, f), "80 rows: picker clamped to the visible frame (%.0f tall in %.0f)", f.size.height, vis.size.height);
         CHECK([ct numberOfRows] == 80 && [ct frame].size.height > [[ct superview] frame].size.height,
               "80 rows: the list scrolls instead");
      }
      stub_core_count = 0;
      [ctrl performSelector:NSSelectorFromString(@"cancelLoadCore:") withObject:nil];
      pump(data, 100);
      CHECK(!(cw && [cw isVisible]), "Cancel hides it");
      CHECK([pop indexOfSelectedItem] == n - 2, "popup back on its last pick (%ld)", (long)[pop indexOfSelectedItem]);
   }

   /* Docks: the panes are docks as Qt's are - the shared model laid out
    * by the driver. The default is written back as the rows Qt writes
    * (thumbnails tabbed above Core Info on the right, Boxart raised,
    * the log hidden); and a second driver built from a saved layout -
    * the one Tatsuya79 arranged in Qt - opens to exactly it. */
   {
      NSRect fi, fb;
      ui_companion_wimp_cocoa.toggle(data, true);
      pump(data, 200);
      test_settings.bools.desktop_menu_save_dock_positions = true;
      [ctrl performSelector:NSSelectorFromString(@"layoutViews")];
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_boxart, "right,1,", 8) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_boxart, ",-,1,0") != NULL,
            "boxart row: raised tab, slot 0 (%s)", test_settings.arrays.desktop_menu_dock_boxart);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_title, "right,1,0,0,", 12) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_title, ",boxart,0,0") != NULL,
            "title row tabbed onto boxart, carrying no size (%s)", test_settings.arrays.desktop_menu_dock_title);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,1,", 8) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,1") != NULL,
            "Core Info row: shown, right, slot 1 (%s)", test_settings.arrays.desktop_menu_dock_core_info);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_log, "bottom,0,", 9) == 0,
            "log row hidden (%s)", test_settings.arrays.desktop_menu_dock_log);
      fi = [[ctrl valueForKey:@"infoScroll"] frame];
      fb = [pane_view(ctrl, 3) frame];
      CHECK(fb.origin.y + fb.size.height <= fi.origin.y, "thumbnails above Core Info by default, as Qt's (flipped: %.0f <= %.0f)", fb.origin.y + fb.size.height, fi.origin.y);
      CHECK([pane_view(ctrl, 4) isHidden], "Title Screen is a tab behind Boxart: not shown");
      [win close];
      pump(data, 100);
   }
   ui_companion_wimp_cocoa.deinit(data);
   {
      void *d2;
      struct wimp_peek *p2;
      id c2;
      NSWindow *w2;
      NSRect r, fi, fb, fl, fp, ft;
      int want_h;
      strlcpy(test_settings.arrays.desktop_menu_dock_search,     "left,1,340,60,-,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_playlists,  "left,1,340,500,-,0,1", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_core,       "left,1,340,40,-,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_boxart,     "right,0,0,0,-,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_title,      "right,1,300,210,-,0,1", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_screenshot, "right,1,300,420,-,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_logo,       "right,0,0,0,boxart,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_core_info,  "right,0,0,0,-,0,3", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_log,        "bottom,1,0,150,-,0,0", 64);
      test_settings.bools.desktop_menu_save_geometry = true;
      test_settings.uints.desktop_menu_window_x      = 20;
      test_settings.uints.desktop_menu_window_y      = 30;
      test_settings.uints.desktop_menu_window_width  = 1000;
      test_settings.uints.desktop_menu_window_height = 700;
      d2 = ui_companion_wimp_cocoa.init();
      CHECK(d2 != NULL, "second driver init from a saved layout");
      if (d2)
      {
         p2 = (struct wimp_peek*)d2;
         c2 = (id)p2->controller;
         w2 = [c2 valueForKey:@"window"];
         ui_companion_wimp_cocoa.toggle(d2, true);
         pump(d2, 300);
         CHECK([[c2 valueForKey:@"infoScroll"] isHidden] && [pane_view(c2, 3) isHidden], "Core Info and Boxart hidden as their rows say");
         CHECK(![pane_view(c2, 5) isHidden] && ![pane_view(c2, 4) isHidden] && ![[c2 valueForKey:@"logScroll"] isHidden],
               "Screenshots, Title Screen and the log shown as their rows say");
         fb = [pane_view(c2, 5) frame]; ft = [pane_view(c2, 4) frame];
         CHECK(fb.origin.y + fb.size.height <= ft.origin.y, "Screenshots above Title Screen, each its own pane (%.0f <= %.0f)", fb.origin.y + fb.size.height, ft.origin.y);
         r = [w2 contentRectForFrameRect:[w2 frame]];
         /* The saved 700 comes back as is where it fits; on a screen
          * whose visible frame is shorter (CI's Xvfb) the driver clamps
          * the frame to it, so the content is that less the title bar. */
         {
            NSRect vis = [[NSScreen mainScreen] visibleFrame];
            CGFloat tb = [w2 frame].size.height - r.size.height;
            want_h = 700;
            if (want_h > (int)(vis.size.height - tb))
               want_h = (int)(vis.size.height - tb);
         }
         CHECK(r.size.width == 1000 && abs((int)r.size.height - want_h) <= 1, "window size restored (%.0fx%.0f, wanted %d)", r.size.width, r.size.height, want_h);
         fp = [[c2 valueForKey:@"playlistsScroll"] frame];
         fl = [[c2 valueForKey:@"logScroll"] frame];
         CHECK(fp.size.width > 300, "left column at the saved 340 (playlists %.0f wide)", fp.size.width);
         CHECK(fb.size.width >= 280 && fb.size.width <= 300, "right column at the saved 300 (Screenshots %.0f wide)", fb.size.width);
         CHECK(fl.size.height >= 100 && fl.size.height <= 150, "log inside the saved 150 (%.0f)", fl.size.height);
         [c2 performSelector:NSSelectorFromString(@"layoutViews")];
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_screenshot, "right,1,300,", 12) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",-,0,0") != NULL,
               "screenshot row re-saved standalone in slot 0 (%s)", test_settings.arrays.desktop_menu_dock_screenshot);
         CHECK(strstr(test_settings.arrays.desktop_menu_dock_title, ",-,0,1") != NULL,
               "title row re-saved standalone in slot 1 (%s)", test_settings.arrays.desktop_menu_dock_title);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_logo, "right,0,", 8) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_logo, ",boxart,0,2") != NULL,
               "logo row re-saved hidden, tabbed onto boxart in slot 2 (%s)", test_settings.arrays.desktop_menu_dock_logo);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,0,", 8) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,3") != NULL,
               "Core Info row re-saved hidden in slot 3 (%s)", test_settings.arrays.desktop_menu_dock_core_info);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_log, "bottom,1,", 9) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_log, ",150,-,0,0") != NULL,
               "log row re-saved shown at 150 (%s)", test_settings.arrays.desktop_menu_dock_log);
         [c2 performSelector:NSSelectorFromString(@"geometryStore")];
         CHECK(test_settings.uints.desktop_menu_window_width == 1000 && abs((int)test_settings.uints.desktop_menu_window_height - want_h) <= 1,
               "window geometry re-saved (%ux%u, wanted %d)", test_settings.uints.desktop_menu_window_width, test_settings.uints.desktop_menu_window_height, want_h);
         /* Core Info back on: below the thumbnails, in the slot its row
          * kept for it. */
         [c2 performSelector:NSSelectorFromString(@"toggleInfo:") withObject:nil];
         pump(d2, 100);
         fi = [[c2 valueForKey:@"infoScroll"] frame]; ft = [pane_view(c2, 4) frame];
         CHECK(![[c2 valueForKey:@"infoScroll"] isHidden] && ft.origin.y + ft.size.height <= fi.origin.y,
               "Core Info shown below Title Screen (title bottom %.0f, info top %.0f)", ft.origin.y + ft.size.height, fi.origin.y);
         CHECK(strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,3") != NULL
               && strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,1,", 8) == 0,
               "rows: Core Info shown, still slot 3 (%s)", test_settings.arrays.desktop_menu_dock_core_info);
         /* Gaps: a drag on the gap after the left column widens it and
          * the rows follow; a drag on the gap between the two right
          * panes moves their split. A gap sits just past a pane's
          * views (the pane's padding, then the gap), in the dock
          * view's flipped coordinates. */
         {
            CGFloat before;
            NSPoint a;
            fp = [[c2 valueForKey:@"playlistsScroll"] frame];
            before = fp.size.width;
            a = NSMakePoint(fp.origin.x + fp.size.width + 6 + 2, fp.origin.y + 20);
            dock_drag(c2, a, NSMakePoint(a.x + 60, a.y));
            pump(d2, 50);
            fp = [[c2 valueForKey:@"playlistsScroll"] frame];
            CHECK(fp.size.width >= before + 50, "left gap drag widened the column (%.0f -> %.0f)", before, fp.size.width);
            CHECK(strncmp(test_settings.arrays.desktop_menu_dock_playlists, "left,1,4", 8) == 0,
                  "playlists row follows the drag (%s)", test_settings.arrays.desktop_menu_dock_playlists);
            fb = [pane_view(c2, 5) frame];
            before = fb.size.height;
            a = NSMakePoint(fb.origin.x + 10, fb.origin.y + fb.size.height + 6 + 2);
            dock_drag(c2, a, NSMakePoint(a.x, a.y + 50));
            pump(d2, 50);
            fb = [pane_view(c2, 5) frame];
            CHECK(fb.size.height >= before + 40, "pane gap drag grew Screenshots (%.0f -> %.0f)", before, fb.size.height);
         }
         /* Drag-and-drop of a strip: Title Screen's strip (just above
          * its view) dragged onto the middle of Screenshots tabs it
          * there; dragged out to the content it floats in a window of
          * its own; the float window's title double-click re-docks it;
          * the strip's close glyph hides it and Closed Docks brings it
          * back. */
         {
            NSPoint a, b;
            NSWindow *fw;
            ft = [pane_view(c2, 4) frame]; fb = [pane_view(c2, 5) frame];
            a = NSMakePoint(ft.origin.x + 10, ft.origin.y - 6 - 8);
            b = NSMakePoint(fb.origin.x + fb.size.width / 2, fb.origin.y + fb.size.height / 2);
            dock_drag(c2, a, b);
            pump(d2, 100);
            CHECK(strstr(test_settings.arrays.desktop_menu_dock_title, ",-,1,0") != NULL
                  && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",title,0,0") != NULL,
                  "strip dropped mid-pane: Title Screen tabbed with Screenshots and raised (%s / %s)",
                  test_settings.arrays.desktop_menu_dock_title, test_settings.arrays.desktop_menu_dock_screenshot);
            CHECK(![pane_view(c2, 4) isHidden] && [pane_view(c2, 5) isHidden], "the raised tab shows, the other hides");
            ft = [pane_view(c2, 4) frame];
            a = NSMakePoint(ft.origin.x + 10, ft.origin.y - 6 - 8);
            b = NSMakePoint(600, 300);
            dock_drag(c2, a, b);
            pump(d2, 100);
            fw = [pane_view(c2, 4) window];
            CHECK(fw && fw != w2 && [fw isVisible] && ![pane_view(c2, 4) isHidden],
                  "strip dropped on the content: Title Screen floats in its own window");
            CHECK(strncmp(test_settings.arrays.desktop_menu_dock_title, "float,1,", 8) == 0,
                  "floating row (%s)", test_settings.arrays.desktop_menu_dock_title);
            CHECK(![pane_view(c2, 5) isHidden], "Screenshots shows again once its tab partner left");
            if (fw && fw != w2)
            {
               /* Dragged by its title over Screenshots' middle, the
                * float docks as a tab there (the title-bar drag tracks
                * the pointer against the docks; the release drops). */
               NSView *dv = [c2 valueForKey:@"dockView"];
               NSPoint sp;
               fb = [pane_view(c2, 5) frame];
               sp = [w2 convertBaseToScreen:[dv convertPoint:NSMakePoint(fb.origin.x + fb.size.width / 2,
                     fb.origin.y + fb.size.height / 2) toView:nil]];
               [c2 floatDragUpdate:4 screenPoint:sp];
               [c2 floatDragEnd:4];
               pump(d2, 100);
               CHECK([pane_view(c2, 4) window] == w2 && ![fw isVisible]
                     && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",title,0,0") != NULL,
                     "float dragged over a pane docks there as a tab (%s)", test_settings.arrays.desktop_menu_dock_screenshot);
               /* Out again, then the title double-click re-docks it on
                * its side's end. */
               ft = [pane_view(c2, 4) frame];
               a = NSMakePoint(ft.origin.x + 10, ft.origin.y - 6 - 8);
               dock_drag(c2, a, b);
               pump(d2, 100);
               CHECK([pane_view(c2, 4) window] == fw && [fw isVisible], "floated again");
               [c2 redockPane:4];
               pump(d2, 100);
               CHECK([pane_view(c2, 4) window] == w2 && ![fw isVisible], "re-docked (Qt's title double-click)");
               CHECK(strncmp(test_settings.arrays.desktop_menu_dock_title, "right,1,", 8) == 0,
                     "re-docked row (%s)", test_settings.arrays.desktop_menu_dock_title);
            }
            ft = [pane_view(c2, 4) frame];
            a = NSMakePoint(ft.origin.x + ft.size.width + 6 - 4 - 6, ft.origin.y - 6 - 8);
            dock_click(c2, a);
            pump(d2, 50);
            CHECK([pane_view(c2, 4) isHidden] && strncmp(test_settings.arrays.desktop_menu_dock_title, "right,0,", 8) == 0,
                  "close glyph hides the pane (%s)", test_settings.arrays.desktop_menu_dock_title);
            {
               NSMenu *cd = [c2 valueForKey:@"closedDocksMenu"];
               NSMenuItem *it = nil;
               NSUInteger i;
               [c2 menuNeedsUpdate:cd];
               for (i = 0; i < (NSUInteger)[cd numberOfItems]; i++)
                  if ([[cd itemAtIndex:i] tag] == 4)
                     it = [cd itemAtIndex:i];
               CHECK(it != nil, "Closed Docks lists Title Screen (%ld items)", (long)[cd numberOfItems]);
               if (it)
                  [c2 showClosedDock:it];
               pump(d2, 50);
               CHECK(![pane_view(c2, 4) isHidden] && strncmp(test_settings.arrays.desktop_menu_dock_title, "right,1,", 8) == 0,
                     "Closed Docks shows it again (%s)", test_settings.arrays.desktop_menu_dock_title);
            }
         }
         [w2 close];
         pump(d2, 100);
         ui_companion_wimp_cocoa.deinit(d2);
      }
   }
   companion_test_teardown_fixtures(root);
   [pool drain];
   if (fails)
   {
      printf("companion_cocoa_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("companion_cocoa_test: OK\n");
   return 0;
}
