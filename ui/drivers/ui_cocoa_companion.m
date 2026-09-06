/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2026 - libretro team
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

/* Native Cocoa desktop companion ("cocoa" companion UI driver).
 *
 * OS X 10.4 baseline: NSWindow, NSSplitView, NSTableView with classic
 * dataSource / delegate protocols, no blocks, no view-based cells, no
 * properties. Objective-C is limited to the AppKit shell; all model
 * logic lives in ui/companion/companion_core.
 *
 * Built both MRC (Makefile, per-file) and ARC (Xcode griffin_objc.m):
 * every object stored past the current autorelease pool is created with
 * alloc/init and released with RELEASE(). */

#include <objc/objc-runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/cocoa_defines.h>
#include "cocoa/cocoa_common.h"

#include "../../command.h"
#include "../../configuration.h"
#include "../../retroarch.h"

#include "../ui_companion_driver.h"
#include "../companion/companion_core.h"

#define COMPANION_COCOA_ITER_US 2000

/* The 10.4 SDK predates NSInteger (introduced with the 10.5 SDK). */
#ifndef NSINTEGER_DEFINED
typedef int NSInteger;
typedef unsigned int NSUInteger;
#define NSINTEGER_DEFINED 1
#endif

/* Under ARC these are no-ops (the compiler manages ownership); under
 * MRC they are the literal messages. Written as macros so the same
 * source builds both ways. */
#if defined(__clang__) && __has_feature(objc_arc)
#define autorelease_compat self
#define RETAIN_COMPAT(x) (x)
#else
#define autorelease_compat autorelease
#define RETAIN_COMPAT(x) [(x) retain]
#endif

typedef struct ui_companion_cocoa_wimp ui_companion_cocoa_wimp_t;

/* Owns the AppKit objects (as ivars, so both MRC and ARC manage them
 * correctly); is the tables' data source / delegate, the window
 * delegate and the menu target. */
@interface RACompanionController : NSObject
{
   ui_companion_cocoa_wimp_t *wimp;
   NSWindow *window;
   NSTableView *playlists;
   NSTableView *entries;
   NSTextField *status;
   NSMenuItem *menuItem;
   NSMenu *entriesMenu;    /* right-click on an entry   */
   NSMenu *playlistsMenu;  /* right-click on a playlist */
   NSMenu *assocMenu;      /* "Associate Core" submenu, rebuilt on open */
   NSSplitView *split;
   NSScrollView *logScroll; /* log pane, hidden until Companion > Log */
   NSTextView *logView;
   BOOL logVisible;
   /* Load Core window: installed cores by name / version. */
   NSWindow *coresWindow;
   NSTableView *coresTable;
}
- (id)initWithWimp:(ui_companion_cocoa_wimp_t*)w;
- (BOOL)buildWindow;
- (void)teardown;
- (NSWindow*)window;
- (void)setStatus:(const char*)msg;
- (void)reloadPlaylists;
- (void)reloadEntries;
- (void)refreshPlaylists:(id)sender;
- (void)runSelected:(id)sender;
- (void)startCore:(id)sender;
- (void)loadCore:(id)sender;
- (void)loadContent:(id)sender;
- (void)deleteEntry:(id)sender;
- (void)associateCore:(id)sender;
- (void)scanDirectory:(id)sender;
- (void)toggleLog:(id)sender;
- (void)loadSelectedCore:(id)sender;
- (void)cancelLoadCore:(id)sender;
- (void)appendLog:(const char*)msg;
@end

struct ui_companion_cocoa_wimp
{
   companion_core_t *core;
   /* RACompanionController, held at +1 through a C pointer. Under ARC
    * the retain is explicit (CFBridgingRetain) since a void* cannot be
    * a strong reference; under MRC alloc/init already gave us +1. */
   void *controller;
};

#if defined(__clang__) && __has_feature(objc_arc)
#define CC_OWN(obj)      ((void*)CFBridgingRetain(obj))
#define CC_DISOWN(ptr)   CFBridgingRelease(ptr)
#else
#define CC_OWN(obj)      ((void*)(obj))
#define CC_DISOWN(ptr)   [(id)(ptr) release]
#endif

#define CC_CTRL(w) ((BRIDGE RACompanionController*)(w)->controller)

/* --- companion_core -> Cocoa callbacks -------------------------------- */

static void cc_on_playlists_changed(void *ud)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)ud;
   if (w && w->controller)
      [CC_CTRL(w) reloadPlaylists];
}

static void cc_on_playlist_changed(void *ud)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)ud;
   if (w && w->controller)
      [CC_CTRL(w) reloadEntries];
}

static void cc_on_status_message(void *ud, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)ud;
   if (w && w->controller)
      [CC_CTRL(w) setStatus:msg];
}

static void cc_on_notify_refresh(void *ud)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)ud;
   if (w)
      companion_core_refresh_playlists(w->core);
}

static void cc_on_scan_finished(void *ud)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)ud;
   if (!w)
      return;
   companion_core_refresh_playlists(w->core);
   if (w->controller)
      [CC_CTRL(w) setStatus:"Scan finished."];
}

static const companion_callbacks_t cc_callbacks = {
   cc_on_playlists_changed,
   cc_on_playlist_changed,
   cc_on_status_message,
   NULL, /* on_log_message */
   cc_on_notify_refresh,
   cc_on_scan_finished,
   NULL, /* on_thumbnail_downloaded */
   NULL  /* on_thumbnail_pack_finished */
};

/* --- Controller ------------------------------------------------------- */

@implementation RACompanionController

- (id)initWithWimp:(ui_companion_cocoa_wimp_t*)w
{
   if ((self = [super init]))
      wimp = w;
   return self;
}

- (void)dealloc
{
   [self teardown];
#if !(defined(__clang__) && __has_feature(objc_arc))
   [super dealloc];
#endif
}

- (NSWindow*)window { return window; }

- (void)setStatus:(const char*)msg
{
   if (status)
      [status setStringValue:BOXSTRING(msg ? msg : "")];
}

- (void)reloadPlaylists
{
   if (playlists)
      [playlists reloadData];
}

- (void)reloadEntries
{
   char buf[64];
   if (!entries)
      return;
   [entries reloadData];
   snprintf(buf, sizeof(buf), "%u entries",
         (unsigned)companion_core_entry_count(wimp->core));
   [self setStatus:buf];
}

/* Returns an autoreleased table wrapped in an autoreleased scroll view;
 * both are retained by the view hierarchy once added, and the table is
 * additionally retained into an ivar by the caller. */
- (NSTableView*)makeTable:(NSRect)frame scroll:(NSScrollView**)outScroll
   twoColumns:(BOOL)two
{
   NSScrollView *scroll = [[[NSScrollView alloc] initWithFrame:frame] autorelease_compat];
   NSTableView *table   = [[[NSTableView alloc] initWithFrame:
      [[scroll contentView] bounds]] autorelease_compat];
   NSTableColumn *c0    = [[[NSTableColumn alloc] initWithIdentifier:@"name"] autorelease_compat];

   [[c0 headerCell] setStringValue:two ? @"Name" : @"Playlists"];
   [c0 setWidth:two ? 360.0 : 180.0];
   [table addTableColumn:c0];

   if (two)
   {
      NSTableColumn *c1 = [[[NSTableColumn alloc] initWithIdentifier:@"core"] autorelease_compat];
      [[c1 headerCell] setStringValue:@"Core"];
      [c1 setWidth:160.0];
      [table addTableColumn:c1];
   }

   [table setDataSource:self];
   [table setDelegate:self];
   [table setAllowsMultipleSelection:NO];
   [table setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

   [scroll setDocumentView:table];
   [scroll setHasVerticalScroller:YES];
   [scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

   *outScroll = scroll;
   return table;
}

- (BOOL)buildWindow
{
   NSRect frame       = NSMakeRect(0, 0, 800, 520);
   NSScrollView *sl   = nil;
   NSScrollView *sr   = nil;
   NSView *content    = nil;
   NSMenu *menu       = nil;
   NSMenuItem *item   = nil;

   window = [[NSWindow alloc] initWithContentRect:frame
      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
            | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
      backing:NSBackingStoreBuffered defer:NO];
   if (!window)
      return NO;

   [window setTitle:@"RetroArch"];
   [window setDelegate:self];
   [window setMinSize:NSMakeSize(480, 320)];
   [window setReleasedWhenClosed:NO];
   [window center];

   content   = [window contentView];

   playlists = RETAIN_COMPAT([self makeTable:NSMakeRect(0, 0, 200, 500)
         scroll:&sl twoColumns:NO]);
   entries   = RETAIN_COMPAT([self makeTable:NSMakeRect(0, 0, 600, 500)
         scroll:&sr twoColumns:YES]);
   [entries setDoubleAction:@selector(runSelected:)];
   [entries setTarget:self];

   split = [[NSSplitView alloc] initWithFrame:
      NSMakeRect(0, 20, frame.size.width, frame.size.height - 20)];
   [split setVertical:YES];
   [split addSubview:sl];
   [split addSubview:sr];
   [split setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
   [content addSubview:split];

   /* Log pane: a read-only NSTextView under the split, added to the
    * hierarchy only while shown so the split gets the full height
    * otherwise. Anchored to the bottom edge above the status line. */
   logScroll = [[NSScrollView alloc] initWithFrame:
      NSMakeRect(0, 20, frame.size.width, 120)];
   logView   = [[NSTextView alloc] initWithFrame:
      [[logScroll contentView] bounds]];
   [logView setEditable:NO];
   [logView setRichText:NO];
   [logView setAutoresizingMask:NSViewWidthSizable];
   [logScroll setDocumentView:logView];
   [logScroll setHasVerticalScroller:YES];
   [logScroll setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];

   status = [[NSTextField alloc] initWithFrame:
      NSMakeRect(4, 0, frame.size.width - 8, 18)];
   [status setEditable:NO];
   [status setBordered:NO];
   [status setDrawsBackground:NO];
   [status setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
   [content addSubview:status];

   /* "Companion" menu appended to the main menu bar. */
   menu = [[[NSMenu alloc] initWithTitle:@"Companion"] autorelease_compat];
   item = [menu addItemWithTitle:@"Load Core..." action:@selector(loadCore:)
      keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:@"Load Content..." action:@selector(loadContent:)
      keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:@"Start Core" action:@selector(startCore:)
      keyEquivalent:@""];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:@"Scan Directory..." action:@selector(scanDirectory:)
      keyEquivalent:@""];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:@"Run Selected" action:@selector(runSelected:)
      keyEquivalent:@"\r"];
   [item setTarget:self];
   item = [menu addItemWithTitle:@"Refresh Playlists" action:@selector(refreshPlaylists:)
      keyEquivalent:@"r"];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:@"Log" action:@selector(toggleLog:)
      keyEquivalent:@""];
   [item setTarget:self];

   menuItem = [[NSMenuItem alloc] initWithTitle:@"Companion" action:NULL
      keyEquivalent:@""];
   [menuItem setSubmenu:menu];
   [[NSApp mainMenu] addItem:menuItem];

   /* Context menus. The table's -menu is shown on right-click; the
    * actions use -clickedRow so they act on the row under the mouse. */
   entriesMenu = [[NSMenu alloc] initWithTitle:@""];
   item = [entriesMenu addItemWithTitle:@"Run" action:@selector(runSelected:)
      keyEquivalent:@""];
   [item setTarget:self];
   [entriesMenu addItem:[NSMenuItem separatorItem]];
   item = [entriesMenu addItemWithTitle:@"Delete Entry" action:@selector(deleteEntry:)
      keyEquivalent:@""];
   [item setTarget:self];
   [entries setMenu:entriesMenu];

   playlistsMenu = [[NSMenu alloc] initWithTitle:@""];
   assocMenu     = [[NSMenu alloc] initWithTitle:@"Associate Core"];
   [assocMenu setDelegate:self]; /* -menuNeedsUpdate: fills it */
   item = [playlistsMenu addItemWithTitle:@"Associate Core" action:NULL
      keyEquivalent:@""];
   [item setSubmenu:assocMenu];
   item = [playlistsMenu addItemWithTitle:@"Refresh Playlists"
      action:@selector(refreshPlaylists:) keyEquivalent:@""];
   [item setTarget:self];
   [playlists setMenu:playlistsMenu];

   return YES;
}

/* NSMenuDelegate (10.3+): rebuild the core list each time it opens so a
 * core installed while the window is up shows without a restart. */
- (void)menuNeedsUpdate:(NSMenu*)menu
{
   size_t i, n;
   NSMenuItem *item;

   if (menu != assocMenu || !wimp)
      return;

   while ([menu numberOfItems] > 0)
      [menu removeItemAtIndex:0];

   item = [menu addItemWithTitle:@"<Detect>" action:@selector(associateCore:)
      keyEquivalent:@""];
   [item setTarget:self];
   [item setTag:-1];

   n = companion_core_installed_core_count(wimp->core);
   if (n)
      [menu addItem:[NSMenuItem separatorItem]];
   for (i = 0; i < n; i++)
   {
      const char *name = companion_core_installed_core_name(wimp->core, i);
      item = [menu addItemWithTitle:BOXSTRING(name ? name : "")
         action:@selector(associateCore:) keyEquivalent:@""];
      [item setTarget:self];
      [item setTag:(NSInteger)i];
   }
}

- (void)teardown
{
   if (menuItem)
   {
      [[NSApp mainMenu] removeItem:menuItem];
      RELEASE(menuItem);
   }
   if (playlists)
   {
      [playlists setDataSource:nil];
      [playlists setDelegate:nil];
      RELEASE(playlists);
   }
   if (entries)
   {
      [entries setDataSource:nil];
      [entries setDelegate:nil];
      [entries setTarget:nil];
      RELEASE(entries);
   }
   RELEASE(status);
   if (coresTable)
   {
      [coresTable setDataSource:nil];
      [coresTable setDelegate:nil];
      [coresTable setTarget:nil];
      RELEASE(coresTable);
   }
   if (coresWindow)
   {
      [coresWindow orderOut:nil];
      RELEASE(coresWindow);
   }
   RELEASE(logView);
   RELEASE(logScroll);
   RELEASE(split);
   if (assocMenu)
      [assocMenu setDelegate:nil];
   RELEASE(assocMenu);
   RELEASE(playlistsMenu);
   RELEASE(entriesMenu);
   if (window)
   {
      [window setDelegate:nil];
      [window orderOut:nil];
      RELEASE(window);
   }
}

/* NSTableDataSource (10.4 informal protocol) */
- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv
{
   if (!wimp)
      return 0;
   if (tv == playlists)
      return (NSInteger)companion_core_playlist_count(wimp->core);
   if (tv == entries)
      return (NSInteger)companion_core_entry_count(wimp->core);
   if (tv == coresTable)
      return (NSInteger)companion_core_installed_core_count(wimp->core);
   return 0;
}

- (id)tableView:(NSTableView*)tv
   objectValueForTableColumn:(NSTableColumn*)col
   row:(NSInteger)row
{
   const char *s = NULL;

   if (!wimp || row < 0)
      return @"";

   if (tv == playlists)
      s = companion_core_playlist_name(wimp->core, (size_t)row);
   else if (tv == coresTable)
      s = [[col identifier] isEqualToString:@"core"]
         ? companion_core_installed_core_version(wimp->core, (size_t)row)
         : companion_core_installed_core_name(wimp->core, (size_t)row);
   else if (tv == entries)
   {
      const struct playlist_entry *e =
         companion_core_entry(wimp->core, (size_t)row);
      if (e)
      {
         if ([[col identifier] isEqualToString:@"core"])
            s = e->core_name;
         else
            s = !string_is_empty(e->label) ? e->label : e->path;
      }
   }

   return BOXSTRING(s ? s : "");
}

/* NSTableView delegate */
- (void)tableViewSelectionDidChange:(NSNotification*)note
{
   int row;
   if (!wimp || [note object] != playlists)
      return;
   row = (int)[playlists selectedRow];
   if (row < 0)
      return;
   if (companion_core_select_playlist(wimp->core, (size_t)row))
      [self setStatus:"Loading playlist..."];
}

/* NSWindow delegate: closing the companion never quits RetroArch. */
- (BOOL)windowShouldClose:(id)sender
{
   [window orderOut:nil];
   return NO;
}

/* Menu / double-click actions */
- (void)refreshPlaylists:(id)sender
{
   if (wimp)
      companion_core_refresh_playlists(wimp->core);
}

/* Row a menu action applies to: the right-clicked row when the action
 * came from a context menu, else the selection. */
- (NSInteger)actionRowIn:(NSTableView*)table
{
   NSInteger row = [table clickedRow];
   if (row < 0)
      row = [table selectedRow];
   return row;
}

- (void)runSelected:(id)sender
{
   NSInteger row;
   if (!wimp)
      return;
   row = [self actionRowIn:entries];
   if (row < 0)
      return;
   if (companion_core_request_load_entry(wimp->core, (size_t)row))
      [window orderOut:nil];
}

- (void)reloadSelectedPlaylist
{
   size_t sel = companion_core_selected_playlist(wimp->core);
   if (sel != (size_t)-1)
      companion_core_select_playlist(wimp->core, sel);
}

- (void)deleteEntry:(id)sender
{
   NSInteger row;
   size_t sel;
   NSAlert *alert;

   if (!wimp)
      return;
   row = [self actionRowIn:entries];
   sel = companion_core_selected_playlist(wimp->core);
   if (row < 0 || sel == (size_t)-1)
      return;

   alert = [[NSAlert alloc] init];
   [alert setMessageText:@"Delete this playlist entry?"];
   [alert addButtonWithTitle:@"Delete"];
   [alert addButtonWithTitle:@"Cancel"];
   if ([alert runModal] == NSAlertFirstButtonReturn)
   {
      if (companion_core_playlist_delete_entry(wimp->core,
               companion_core_playlist_path(wimp->core, sel), (size_t)row))
         [self reloadSelectedPlaylist];
   }
   RELEASE(alert);
}

- (void)associateCore:(id)sender
{
   NSInteger row, tag;
   const char *core_path = NULL;

   if (!wimp)
      return;
   /* The playlist the menu was opened on, not necessarily the loaded one. */
   row = [self actionRowIn:playlists];
   if (row < 0)
      return;
   tag = [(NSMenuItem*)sender tag];
   if (tag >= 0)
      core_path = companion_core_installed_core_path(wimp->core, (size_t)tag);

   companion_core_playlist_set_default_core(wimp->core,
         companion_core_playlist_path(wimp->core, (size_t)row), core_path);
}

- (void)startCore:(id)sender
{
   if (wimp && !companion_core_start_core(wimp->core))
      [self setStatus:"Failed to start the core."];
}

- (void)toggleLog:(id)sender
{
   NSView *content;
   NSRect frame;
   if (!window || !split || !logScroll)
      return;

   content    = [window contentView];
   frame      = [content bounds];
   logVisible = !logVisible;

   if (logVisible)
   {
      [logScroll setFrame:NSMakeRect(0, 20, frame.size.width, 120)];
      [split setFrame:NSMakeRect(0, 140, frame.size.width,
            frame.size.height - 140)];
      [content addSubview:logScroll];
   }
   else
   {
      [logScroll removeFromSuperview];
      [split setFrame:NSMakeRect(0, 20, frame.size.width,
            frame.size.height - 20)];
   }
}

/* Append a log line; trim the oldest half once the text passes 256 KiB
 * so appends stay proportional to the line, not to the buffer. */
- (void)appendLog:(const char*)msg
{
   NSTextStorage *storage;
   NSUInteger len;
   if (!logView || !msg)
      return;

   storage = [logView textStorage];
   len     = [storage length];
   if (len > 256 * 1024)
   {
      [storage deleteCharactersInRange:NSMakeRange(0, len / 2)];
      len = [storage length];
   }
   [logView replaceCharactersInRange:NSMakeRange(len, 0)
      withString:BOXSTRING(msg)];
   if (logVisible)
      [logView scrollRangeToVisible:NSMakeRange([storage length], 0)];
}

/* Same shape as the platform driver's open panel (ui_cocoa.m): the
 * 10.6+ URL API when present, else the 10.4 selectors through
 * objc_msgSend so a modern SDK does not see the removed declarations. */
- (void)scanDirectory:(id)sender
{
   NSOpenPanel *panel;
   NSString *path = nil;
   NSInteger response;

   if (!wimp)
      return;

   panel = [NSOpenPanel openPanel];
   [panel setTitle:@"Select a directory to scan for content"];
   [panel setCanChooseDirectories:YES];
   [panel setCanChooseFiles:NO];
   [panel setAllowsMultipleSelection:NO];

   if ([panel respondsToSelector:@selector(URL)])
   {
      response = [panel runModal];
      if (response == 1)
         path = [[panel performSelector:@selector(URL)] path];
   }
   else
   {
      response = ((NSInteger (*)(id, SEL, id, id))objc_msgSend)(panel,
            @selector(runModalForDirectory:file:), nil, nil);
      if (response == 1)
         path = ((id (*)(id, SEL))objc_msgSend)(panel, @selector(filename));
   }

   if (response != 1 || !path)
      return;

   if (companion_core_request_scan(wimp->core, [path UTF8String], true,
            config_get_ptr()->bools.show_hidden_files))
      [self setStatus:"Scanning..."];
   else
      [self setStatus:"Scanning is not available in this build."];
}

/* The companion's own Load Core window (like Qt's LoadCoreWindow):
 * a table of installed cores, Load / Cancel. Non-modal; the tables'
 * data source is this controller, keyed on the table object. */
- (void)loadCore:(id)sender
{
   if (!wimp)
      return;

   if (!coresWindow)
   {
      NSRect frame       = NSMakeRect(0, 0, 420, 400);
      NSScrollView *sc   = nil;
      NSView *content    = nil;
      NSButton *load     = nil;
      NSButton *cancel   = nil;

      coresWindow = [[NSWindow alloc] initWithContentRect:frame
         styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
               | NSWindowStyleMaskResizable)
         backing:NSBackingStoreBuffered defer:NO];
      [coresWindow setTitle:@"Load Core"];
      [coresWindow setReleasedWhenClosed:NO];
      [coresWindow center];
      content = [coresWindow contentView];

      coresTable = RETAIN_COMPAT([self makeTable:NSMakeRect(0, 40, 420, 360)
            scroll:&sc twoColumns:YES]);
      [[[coresTable tableColumns] objectAtIndex:0] setWidth:280.0];
      [[[[coresTable tableColumns] objectAtIndex:1] headerCell]
         setStringValue:@"Version"];
      [coresTable setDoubleAction:@selector(loadSelectedCore:)];
      [coresTable setTarget:self];
      [content addSubview:sc];

      load = [[[NSButton alloc] initWithFrame:NSMakeRect(250, 8, 80, 24)] autorelease_compat];
      [load setTitle:@"Load"];
      [load setKeyEquivalent:@"\r"];
      [load setTarget:self];
      [load setAction:@selector(loadSelectedCore:)];
      [load setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
      [content addSubview:load];

      cancel = [[[NSButton alloc] initWithFrame:NSMakeRect(335, 8, 80, 24)] autorelease_compat];
      [cancel setTitle:@"Cancel"];
      [cancel setTarget:self];
      [cancel setAction:@selector(cancelLoadCore:)];
      [cancel setAutoresizingMask:NSViewMinXMargin | NSViewMaxYMargin];
      [content addSubview:cancel];
   }

   [coresTable reloadData];
   if ([coresTable numberOfRows] > 0)
      [coresTable selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
         byExtendingSelection:NO];
   [coresWindow makeKeyAndOrderFront:nil];
}

- (void)loadSelectedCore:(id)sender
{
   NSInteger row;
   if (!wimp || !coresTable)
      return;
   row = [self actionRowIn:coresTable];
   if (row < 0)
      return;
   [coresWindow orderOut:nil];
   if (companion_core_load_core(wimp->core,
            companion_core_installed_core_path(wimp->core, (size_t)row)))
      [self setStatus:"Core loaded."];
   else
      [self setStatus:"Failed to load the core."];
}

- (void)cancelLoadCore:(id)sender
{
   [coresWindow orderOut:nil];
}

- (void)loadContent:(id)sender
{
   id delegate = [[NSApplication sharedApplication] delegate];
   if ([delegate respondsToSelector:@selector(openDocument:)])
      [delegate performSelector:@selector(openDocument:) withObject:sender];
}

@end

/* --- Driver entry points ---------------------------------------------- */

static void *ui_companion_cocoa_wimp_init(void)
{
   RACompanionController *ctrl  = nil;
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)
      calloc(1, sizeof(*w));
   if (!w)
      return NULL;

   w->core = companion_core_new(&cc_callbacks, w);
   ctrl    = [[RACompanionController alloc] initWithWimp:w];

   if (!w->core || !ctrl || ![ctrl buildWindow])
   {
      if (ctrl)
      {
         [ctrl teardown];
         RELEASE(ctrl);
      }
      companion_core_free(w->core);
      free(w);
      return NULL;
   }

   w->controller = CC_OWN(ctrl);
#if !(defined(__clang__) && __has_feature(objc_arc))
   ctrl = nil; /* ownership moved into w->controller */
#endif

   companion_core_refresh_playlists(w->core);
   return w;
}

static void ui_companion_cocoa_wimp_deinit(void *data)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   if (!w)
      return;
   if (w->controller)
   {
      [CC_CTRL(w) teardown];
      CC_DISOWN(w->controller);
      w->controller = NULL;
   }
   companion_core_free(w->core);
   free(w);
}

static void ui_companion_cocoa_wimp_toggle(void *data, bool force)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   settings_t *settings         = config_get_ptr();

   if (!w || !w->controller)
      return;
   if (!(settings->bools.ui_companion_toggle || force))
      return;

   companion_core_prepare_show_window(w->core);
   [[CC_CTRL(w) window] makeKeyAndOrderFront:nil];
}

static void ui_companion_cocoa_wimp_iterate(void *data)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   if (w)
      companion_core_iterate(w->core, COMPANION_COCOA_ITER_US);
}

static void ui_companion_cocoa_wimp_event_command(void *data,
      enum event_command cmd)
{
   (void)data;
   (void)cmd;
}

static void ui_companion_cocoa_wimp_notify_refresh(void *data)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   if (w)
      companion_core_notify_refresh(w->core);
}

static void ui_companion_cocoa_wimp_msg_queue_push(void *data,
      const char *msg, unsigned priority, unsigned duration, bool flush)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   if (w)
      companion_core_status_message(w->core, msg, priority, duration, flush);
}

static void ui_companion_cocoa_wimp_log_msg(void *data, const char *msg)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   if (w && w->controller)
      [CC_CTRL(w) appendLog:msg];
}

static void *ui_companion_cocoa_wimp_get_main_window(void *data)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   if (!w || !w->controller)
      return NULL;
   return (BRIDGE void*)[CC_CTRL(w) window];
}

static bool ui_companion_cocoa_wimp_is_active(void *data)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   return w && w->controller && [[CC_CTRL(w) window] isVisible];
}

ui_companion_driver_t ui_companion_wimp_cocoa = {
   ui_companion_cocoa_wimp_init,
   ui_companion_cocoa_wimp_deinit,
   ui_companion_cocoa_wimp_toggle,
   ui_companion_cocoa_wimp_iterate,
   ui_companion_cocoa_wimp_event_command,
   ui_companion_cocoa_wimp_notify_refresh,
   ui_companion_cocoa_wimp_msg_queue_push,
   NULL, /* render_messagebox */
   ui_companion_cocoa_wimp_get_main_window,
   ui_companion_cocoa_wimp_log_msg,
   ui_companion_cocoa_wimp_is_active,
   NULL, /* get_app_icons */
   NULL, /* set_app_icon */
   NULL, /* get_app_icon_texture */
   NULL, /* browser_window: platform driver's */
   NULL, /* msg_window:     platform driver's */
   NULL, /* window:         platform driver's */
   NULL, /* application:    pumped by the platform driver */
   "cocoa",
};
