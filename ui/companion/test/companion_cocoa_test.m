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

/* The driver under test (its global driver struct). */
extern ui_companion_driver_t ui_companion_wimp_cocoa;

/* Peek into the driver's private state: the struct is private to the
 * .m, but its first two fields are stable and documented there. */
struct wimp_peek { companion_core_t *core; void *controller; };

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
   content = [win contentView];
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
         { "aboutContributors:","Help > About Contributors (window)" },
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
      s = NSSelectorFromString(@"aboutContributors:");
      CHECK([ctrl respondsToSelector:s], "About Contributors implemented");
      [ctrl performSelector:s withObject:nil];
      pump(data, 100);
      {
         NSWindow *cw = [ctrl valueForKey:@"contributorsWindow"];
         CHECK(cw != nil && [cw isVisible], "contributors window shown");
         if (cw)
         {
            NSTextView *tv = find_view([cw contentView], [NSTextView class]);
            CHECK(tv && [[tv string] length] > 1000, "contributors text present (%lu chars)", tv ? (unsigned long)[[tv string] length] : 0UL);
            [cw orderOut:nil];
         }
      }
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
      s = NSSelectorFromString(@"installThumbnailFromPath:");
      CHECK([ctrl respondsToSelector:s], "installThumbnailFromPath: implemented");
      {
         NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[ctrl methodSignatureForSelector:s]];
         const char *ip = img;
         BOOL ok = NO;
         [inv setSelector:s]; [inv setTarget:ctrl];
         [inv setArgument:&ip atIndex:2];
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

   ui_companion_wimp_cocoa.deinit(data);
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
