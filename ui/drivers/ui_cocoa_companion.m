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
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/cocoa_defines.h>
#include "cocoa/cocoa_common.h"
#include "cocoa/apple_platform.h"

#include "../../command.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../msg_hash.h"
#include "../../version.h"
#include "../../AUTHORS_c.h"
#include "../../verbosity.h"
#include "../../input/drivers_keyboard/keyboard_event_apple.h"

#include "../ui_companion_driver.h"
#include "../companion/companion_core.h"
#include "../companion/companion_thumbs.h"
#include <formats/image.h>

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
/* Keep an autoreleased object already stored in a strong ivar: ARC has
 * retained it on assignment, MRC needs the retain spelled out. */
#define KEEP_IVAR(x) ((void)0)
#else
#define autorelease_compat autorelease
#define RETAIN_COMPAT(x) [(x) retain]
#define KEEP_IVAR(x) [(x) retain]
#endif

/* 10.12 renamed the text-alignment constants; older SDKs (the 10.5 SDK
 * the PowerPC cross build uses) only have the original names. */
#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || MAC_OS_X_VERSION_MAX_ALLOWED < 101200
#define NSTextAlignmentCenter NSCenterTextAlignment
#define NSTextAlignmentRight  NSRightTextAlignment
#endif

typedef struct ui_companion_cocoa_wimp ui_companion_cocoa_wimp_t;

#define CC_GRID_PAD   16.0
#define CC_GRID_LABEL 18.0
/* Layout metrics (points; macOS scales them for the display). Match the
 * Qt companion's docks. */
#define CC_PANE_W     280.0   /* left and right columns */
#define CC_LABEL_H    18.0
#define CC_CTRL_H     24.0
#define CC_PAD         6.0
#define CC_STATUS_H   20.0
#define CC_FOOTER_H   30.0
#define CC_PL_ROW_H   34.0    /* playlist rows, as Qt's icon rows */
#define CC_PL_ICON    32.0

@class RACompanionController;

/* Hand-laid thumbnail grid: NSCollectionView is 10.5+ and the baseline
 * is 10.4, so this is a flipped NSView that flow-lays fixed cells and
 * draws each (thumbnail letterboxed above a one-line label). One
 * NSImage per row is cached; the controller decodes them one per frame
 * and calls -setImage:forRow: as they arrive. */
/* The boxart pane: an image dropped on it becomes the selected entry's
 * thumbnail of the pane's type (Qt's ThumbnailWidget). */
@interface RACompanionBoxart : NSImageView
{
@public
   RARCH_UNSAFE_UNRETAINED id owner;
}
@end

@interface RACompanionGrid : NSView
{
   RARCH_UNSAFE_UNRETAINED RACompanionController *owner; /* owner holds us; no ARC cycle */
   NSMutableArray *images;
   NSInteger count;
   NSInteger selected;
   CGFloat thumb;   /* thumbnail edge, from the zoom slider */
}
- (void)setThumbEdge:(CGFloat)edge;
- (id)initWithOwner:(RACompanionController*)o;
- (void)setCount:(NSInteger)n;
- (void)setImage:(NSImage*)img forRow:(NSInteger)row;
- (NSInteger)selectedRow;
- (void)setSelectedRow:(NSInteger)row;
- (void)relayout;
- (NSRect)rectForRow:(NSInteger)row;
- (BOOL)hasImageForRow:(NSInteger)row;
/* Rows whose cells intersect the visible rect. */
- (BOOL)visibleRowsFirst:(NSInteger*)first last:(NSInteger*)last;
@end

/* Owns the AppKit objects (as ivars, so both MRC and ARC manage them
 * correctly); is the tables' data source / delegate, the window
 * delegate and the menu target. */
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
@interface RACompanionController : NSObject <NSTableViewDataSource,
   NSTableViewDelegate, NSWindowDelegate, NSMenuDelegate>
#else
@interface RACompanionController : NSObject
#endif
{
   ui_companion_cocoa_wimp_t *wimp;
   NSWindow *window;
   NSTableView *playlists;
   NSTableView *entries;
   NSScrollView *entriesScroll;   /* holds the table or the grid */
   RACompanionGrid *grid;
   BOOL iconView;
   BOOL browseMode;   /* entries table shows the filesystem */
   NSTextField *status;
   NSMenuItem *menuItem;
   NSMenu *entriesMenu;    /* right-click on an entry   */
   NSMenu *playlistsMenu;  /* right-click on a playlist */
   NSMenu *assocMenu;      /* "Associate Core" submenu, rebuilt on open */
   NSScrollView *logScroll; /* log pane, hidden until Companion > Log */
   NSTextView *logView;
   BOOL logVisible;
   /* Load Core window: installed cores by name / version. */
   NSWindow *coresWindow;
   NSTableView *coresTable;
   char coresContent[PATH_MAX_LENGTH]; /* content to run with the pick, or "" */
   NSInteger coresRows;                /* rows to show (filtered when running) */
   const char *thumbSubdir;            /* icon_view_thumbnail_type -> repository subdir */
   BOOL started;                       /* initial_playlist applied once */
   /* Core information pane (right of the entries), shown on demand;
    * rows cached from companion_core_core_info_rows(). */
   NSScrollView *infoScroll;
   NSTableView *infoTable;
   NSImageView *boxart;     /* selected entry's boxart, right pane */
   BOOL boxartVisible;
   BOOL infoVisible;
   struct string_list *infoKeys;
   struct string_list *infoValues;
   char infoCore[PATH_MAX_LENGTH];

   /* Qt-layout chrome (all owned by the view hierarchy; +1 in ivars). */
   NSTextField *searchLabel, *browserLabel, *coreLabel, *infoLabel, *boxartLabel;
   NSTextField *itemsLabel, *zoomLabel;
   NSTextField *searchField;
   NSButton *clearButton, *infoButton, *runButton, *stopButton;
   NSWindow *contributorsWindow;      /* Help > About Contributors */
   NSWindow *optsWindow;  NSTableView *optsTable;   /* View > Core Options */
   NSWindow *shpWindow;   NSTableView *shpTable;    /* View > Shader Parameters */
   NSWindow *setWindow;   NSTableView *setTable;    /* View > Options */
   NSTabView *browserTabs;            /* Playlists | File Browser */
   NSButton *brUp, *brStart, *brDownloads; /* Qt's browser toolbar */
   NSScrollView *playlistsScroll;
   NSPopUpButton *corePopup;          /* launch-with core, like Qt's */
   NSMutableArray *corePaths;         /* per popup row: core path or "" */
   NSPopUpButton *viewPopup;          /* List / Icons */
   NSPopUpButton *thumbPopup;         /* boxart / screenshot / title / logo */
   NSSlider *zoomSlider;
   NSSegmentedControl *boxartTypes;   /* the four types for the boxart pane */
   const char *boxartSubdir;
   NSMutableArray *playlistIcons;     /* NSImage per playlist row */
   NSImage *folderIcon;               /* the XMB folder asset, for the browser */
   char filter[128];                  /* lower-cased search text */
   NSInteger *rowMap;                 /* table row -> entry index under a filter */
   NSInteger rowCount;
   /* Shared thumbnail engine (ui/companion/companion_thumbs): the grid
    * requests what is on screen each tick and installs what finished. */
   companion_thumbs_t *thumbs;
   unsigned thumbGen;                 /* bumped per grid reload; in tags */
   NSInteger visFirst, visLast;
   char *thumbNone;                   /* per row: 1 = no thumbnail file */
   NSInteger thumbNoneCount;          /* its length: rows beyond it do not exist */
   NSInteger boxartEntry;             /* entry the pane shows / awaits */
   /* The pane's own bitmap: animation frames are written into its
    * pixels in place, one rep + image for the animation's life. */
   NSBitmapImageRep *boxartRep;
   NSImage *boxartImage;
   int boxartW, boxartH;
   BOOL syncingSort;                  /* setSortDescriptors: from the core, not a click */
}
- (id)initWithWimp:(ui_companion_cocoa_wimp_t*)w;
- (ui_companion_cocoa_wimp_t*)wimp;
- (BOOL)buildWindow;
- (void)teardown;
- (NSWindow*)window;
- (void)setStatus:(const char*)msg;
- (void)reloadPlaylists;
- (void)reloadEntries;
- (void)refreshPlaylists:(id)sender;
- (void)runSelected:(id)sender;
- (void)browseFiles:(id)sender;
- (void)browseUp:(id)sender;
- (void)browseStart:(id)sender;
- (void)browseDownloads:(id)sender;
- (void)browseReload;
- (void)browseLanded;
- (void)hideAndFocusRetroArch;
- (void)focusRetroArchDeferred:(id)unused;
- (void)windowWillClose:(NSNotification*)note;
- (void)windowDidResignKey:(NSNotification*)note;
- (void)scheduleFocusHandback;
- (void)tableView:(NSTableView*)tv sortDescriptorsDidChange:(NSArray*)old;
- (void)syncSortIndicator;
- (void)playlistsDoubleClick:(id)sender;
- (void)startCore:(id)sender;
- (void)loadCore:(id)sender;
- (void)loadContent:(id)sender;
- (void)deleteEntry:(id)sender;
- (void)associateCore:(id)sender;
- (void)scanDirectory:(id)sender;
- (void)applySharedSettings;
- (void)layoutViews;
- (void)fillCorePopup:(NSInteger)entryRow;
- (const char*)popupCorePath;
- (void)runWithPopup:(id)sender;
- (void)zoomChanged:(id)sender;
- (void)thumbTypeChanged:(id)sender;
- (void)viewChanged:(id)sender;
- (void)boxartTypeChanged:(id)sender;
- (void)clearSearch:(id)sender;
- (void)statusDefault;
- (void)tabChanged:(id)sender;
- (void)corePopupChanged:(id)sender;
- (void)focusSearch:(id)sender;
- (void)searchChanged:(id)sender;
- (void)openDocs:(id)sender;
- (void)showCoreOptions:(id)sender;
- (void)showOptions:(id)sender;
- (void)settingActivate:(id)sender;
- (void)applyTheme;
- (void)optionCycle:(id)sender;
- (void)optionReset:(id)sender;
- (void)optionResetAll:(id)sender;
- (void)showShaderParams:(id)sender;
- (void)shaderApply:(id)sender;
- (void)shaderReset:(id)sender;
- (void)renamePlaylist:(id)sender;
- (BOOL)renamePlaylistAtRow:(NSInteger)row to:(const char*)newName;
- (size_t)addPaths:(NSArray*)paths;
- (void)addFiles:(id)sender;
- (BOOL)installThumbnailFromPath:(const char*)imagePath;
- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id)info
      proposedRow:(NSInteger)row proposedDropOperation:(NSTableViewDropOperation)op;
- (BOOL)tableView:(NSTableView*)tv acceptDrop:(id)info row:(NSInteger)row
      dropOperation:(NSTableViewDropOperation)op;
- (void)stopContent:(id)sender;
- (void)unloadCore:(id)sender;
- (void)quitRetroArch:(id)sender;
- (void)aboutRetroArch:(id)sender;
- (void)aboutContributors:(id)sender;
- (NSInteger)entryForRow:(NSInteger)row;
- (void)rebuildRowMap;
- (void)buildCoresWindow;
- (void)setIconView:(BOOL)icons;
- (void)gridRun:(NSInteger)row;
- (void)gridSelectionChanged:(NSInteger)row;
- (void)iconTick;
- (void)thumbDone:(uintptr_t)tag bits:(const uint32_t*)bits width:(int)w height:(int)h;
- (void)boxartBlit:(const uint32_t*)bits width:(int)w height:(int)h;
- (CGFloat)thumbEdge;
- (void)thumbWant:(NSInteger)row urgent:(BOOL)urgent;
- (BOOL)thumbPathForRow:(NSInteger)row into:(char*)path len:(size_t)len;
- (void)toggleLog:(id)sender;
- (void)loadSelectedCore:(id)sender;
- (void)showCoresForContent:(const char*)content;
- (void)toggleInfo:(id)sender;
- (void)toggleBoxart:(id)sender;
- (void)refreshBoxart;
- (void)refreshInfo;
- (void)infoFollowCore;
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

/* The listing landed (enumerated off the UI thread): rebuild the panes. */
static void cc_on_browse_changed(void *ud)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)ud;
   if (w && w->controller)
      [CC_CTRL(w) browseLanded];
}

static const companion_callbacks_t cc_callbacks = {
   cc_on_playlists_changed,
   cc_on_playlist_changed,
   cc_on_status_message,
   NULL, /* on_log_message */
   cc_on_notify_refresh,
   cc_on_scan_finished,
   NULL, /* on_thumbnail_downloaded */
   NULL, /* on_thumbnail_pack_finished */
   cc_on_browse_changed
};

/* --- Controller ------------------------------------------------------- */

@implementation RACompanionBoxart
- (NSDragOperation)draggingEntered:(id)sender
{
   NSPasteboard *pb = [sender draggingPasteboard];
   if ([[pb types] containsObject:NSFilenamesPboardType])
      return NSDragOperationCopy;
   return NSDragOperationNone;
}
- (BOOL)performDragOperation:(id)sender
{
   NSArray *files = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
   if (![files count] || !owner)
      return NO;
   return [owner installThumbnailFromPath:[[files objectAtIndex:0] UTF8String]];
}
@end

@implementation RACompanionGrid

- (id)initWithOwner:(RACompanionController*)o
{
   if ((self = [super initWithFrame:NSMakeRect(0, 0, 10, 10)]))
   {
      owner    = o;
      images   = [[NSMutableArray alloc] init];
      count    = 0;
      selected = -1;
      thumb    = 192.0;
   }
   return self;
}

- (void)dealloc
{
   RELEASE(images);
#if !(defined(__clang__) && __has_feature(objc_arc))
   [super dealloc];
#endif
}

- (BOOL)isFlipped { return YES; }

- (void)setThumbEdge:(CGFloat)edge
{
   thumb = edge < 32.0 ? 32.0 : edge;
   [self relayout];
}

- (NSInteger)columns
{
   /* From the clip view's width when we have one: our own bounds are
    * zero until the first relayout inside a scroll view. */
   CGFloat w = [self superview] ? [[self superview] bounds].size.width
                                : [self bounds].size.width;
   NSInteger c = (NSInteger)(w / (thumb + CC_GRID_PAD));
   return c < 1 ? 1 : c;
}

- (void)viewDidMoveToSuperview
{
   [super viewDidMoveToSuperview];
   if ([self superview])
      [self relayout];
}

- (CGFloat)cellHeight { return thumb + CC_GRID_PAD + CC_GRID_LABEL; }

- (void)relayout
{
   NSInteger cols = [self columns];
   NSInteger rows = (count + cols - 1) / (cols > 0 ? cols : 1);
   NSSize sz      = [[self superview] bounds].size;
   CGFloat h      = rows * [self cellHeight];
   if (h < sz.height)
      h = sz.height;
   [self setFrameSize:NSMakeSize(sz.width, h)];
   [self setNeedsDisplay:YES];
}

- (void)setCount:(NSInteger)n
{
   NSInteger i;
   [images removeAllObjects];
   for (i = 0; i < n; i++)
      [images addObject:[NSNull null]];
   count    = n;
   selected = (n > 0) ? 0 : -1;
   [self relayout];
}

- (void)setImage:(NSImage*)img forRow:(NSInteger)row
{
   if (row < 0 || row >= count)
      return;
   [images replaceObjectAtIndex:row withObject:(img ? (id)img : (id)[NSNull null])];
   [self setNeedsDisplay:YES];
}

- (BOOL)hasImageForRow:(NSInteger)row
{
   return row >= 0 && row < count && [images objectAtIndex:row] != [NSNull null];
}

- (BOOL)visibleRowsFirst:(NSInteger*)first last:(NSInteger*)last
{
   NSRect vis     = [self visibleRect];
   NSInteger cols = [self columns];
   CGFloat ch     = [self cellHeight];
   NSInteger l0, l1;
   if (count <= 0 || ch <= 0)
      return NO;
   l0 = (NSInteger)(NSMinY(vis) / ch);
   l1 = (NSInteger)(NSMaxY(vis) / ch);
   *first = l0 * cols;
   *last  = (l1 + 1) * cols - 1;
   if (*first < 0)      *first = 0;
   if (*last >= count)  *last  = count - 1;
   return *first <= *last;
}

- (NSInteger)selectedRow { return selected; }

- (void)setSelectedRow:(NSInteger)row
{
   if (row >= -1 && row < count)
   {
      selected = row;
      [self setNeedsDisplay:YES];
      if (owner)
         [owner gridSelectionChanged:row];
   }
}

- (NSRect)rectForRow:(NSInteger)row
{
   NSInteger cols = [self columns];
   NSInteger col  = row % cols;
   NSInteger line = row / cols;
   CGFloat cw     = thumb + CC_GRID_PAD;
   return NSMakeRect(col * cw + CC_GRID_PAD / 2,
         line * [self cellHeight] + CC_GRID_PAD / 2,
         thumb, thumb + CC_GRID_LABEL);
}

- (NSInteger)rowAtPoint:(NSPoint)p
{
   NSInteger cols = [self columns];
   NSInteger col  = (NSInteger)(p.x / (thumb + CC_GRID_PAD));
   NSInteger line = (NSInteger)(p.y / [self cellHeight]);
   NSInteger row;
   if (col < 0 || col >= cols)
      return -1;
   row = line * cols + col;
   return (row >= 0 && row < count) ? row : -1;
}

- (void)drawRect:(NSRect)dirty
{
   NSInteger i, first, last, cols;
   CGFloat ch;
   static int logged = 0;
   ui_companion_cocoa_wimp_t *w = owner ? [owner wimp] : NULL;
   if (!logged++)
      RARCH_LOG("[Companion] grid first paint: count=%ld frame=%.0fx%.0f dirty=%.0f,%.0f %.0fx%.0f thumb=%.0f cols=%ld owner=%p wimp=%p\n",
            (long)count, [self frame].size.width, [self frame].size.height,
            dirty.origin.x, dirty.origin.y, dirty.size.width, dirty.size.height,
            thumb, (long)[self columns], (BRIDGE void*)owner, (void*)w);
   if (!w || count <= 0)
      return;

   /* Only the rows the dirty rect covers: the list may be 40k entries. */
   cols  = [self columns];
   ch    = [self cellHeight];
   first = (NSInteger)(NSMinY(dirty) / ch) * cols;
   last  = ((NSInteger)(NSMaxY(dirty) / ch) + 1) * cols - 1;
   if (first < 0)      first = 0;
   if (last >= count)  last  = count - 1;
   for (i = first; i <= last; i++)
   {
      NSRect cell = [self rectForRow:i];
      NSRect trect, label; /* thumbnail box; `thumb` is its edge */
      const struct playlist_entry *e;
      id im;

      if (!NSIntersectsRect(cell, dirty))
         continue;

      trect = NSMakeRect(cell.origin.x, cell.origin.y, thumb, thumb);
      label = NSMakeRect(cell.origin.x, cell.origin.y + thumb,
            thumb, CC_GRID_LABEL);

      if (i == selected)
      {
         [[NSColor selectedControlColor] set];
         NSRectFill(NSInsetRect(cell, -2, -2));
      }

      im = [images objectAtIndex:i];
      if (im != [NSNull null])
      {
         NSSize is = [(NSImage*)im size];
         CGFloat s = 1.0;
         NSRect dst;
         if (is.width > 0 && is.height > 0)
            s = (is.width >= is.height)
               ? thumb / is.width : thumb / is.height;
         dst = NSMakeRect(trect.origin.x + (thumb - is.width * s) / 2,
               trect.origin.y + (thumb - is.height * s) / 2,
               is.width * s, is.height * s);
         /* This view is flipped. From 10.6 the drawing call can honour
          * that itself; before, the image had to be flipped. */
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 1060
         [(NSImage*)im drawInRect:dst fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver fraction:1.0
            respectFlipped:YES hints:nil];
#else
         [(NSImage*)im setFlipped:YES];
         [(NSImage*)im drawInRect:dst fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver fraction:1.0];
#endif
      }
      else
      {
         [[NSColor gridColor] set];
         NSFrameRect(trect);
      }

      e = companion_core_entry(w->core, (size_t)i);
      if (e)
      {
         const char *lbl = !string_is_empty(e->label)
            ? e->label : path_basename(e->path);
         NSMutableParagraphStyle *ps =
            [[[NSMutableParagraphStyle alloc] init] autorelease_compat];
         NSDictionary *attr;
         [ps setAlignment:NSTextAlignmentCenter];
         [ps setLineBreakMode:NSLineBreakByTruncatingTail];
         attr = [NSDictionary dictionaryWithObjectsAndKeys:
               [NSFont systemFontOfSize:11.0], NSFontAttributeName,
               ps, NSParagraphStyleAttributeName, nil];
         [BOXSTRING(lbl ? lbl : "") drawInRect:label withAttributes:attr];
      }
   }
}

- (void)mouseDown:(NSEvent*)event
{
   NSPoint p     = [self convertPoint:[event locationInWindow] fromView:nil];
   NSInteger row = [self rowAtPoint:p];
   if (row < 0)
      return;
   [self setSelectedRow:row];
   if ([event clickCount] >= 2)
      [owner gridRun:row];
}

- (void)keyDown:(NSEvent*)event
{
   NSInteger cols = [self columns];
   NSInteger row  = selected;
   unichar c;
   if (count == 0)
      return;
   c = [[event characters] length] ? [[event characters] characterAtIndex:0] : 0;
   switch (c)
   {
      case NSLeftArrowFunctionKey:  row = (row > 0) ? row - 1 : 0; break;
      case NSRightArrowFunctionKey: row = (row < count - 1) ? row + 1 : row; break;
      case NSUpArrowFunctionKey:    row = (row - cols >= 0) ? row - cols : row; break;
      case NSDownArrowFunctionKey:  row = (row + cols < count) ? row + cols : row; break;
      case '\r': case 3:            [owner gridRun:selected]; return;
      default: [super keyDown:event]; return;
   }
   [self setSelectedRow:row];
   [self scrollRectToVisible:[self rectForRow:row]];
}

- (BOOL)acceptsFirstResponder { return YES; }

@end

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
- (ui_companion_cocoa_wimp_t*)wimp { return wimp; }

- (void)setStatus:(const char*)msg
{
   if (status)
      [status setStringValue:BOXSTRING(msg ? msg : "")];
}

- (void)reloadPlaylists
{
   if (wimp && playlistIcons)
   {
      size_t i, n = companion_core_playlist_count(wimp->core);
      /* 'fldr' is the generic folder HFS type; the named constant lives
       * in Carbon's Icons.h, which is not pulled in here. */
      NSImage *folder = [[NSWorkspace sharedWorkspace] iconForFileType:
         NSFileTypeForHFSTypeCode('fldr')];
      [playlistIcons removeAllObjects];
      for (i = 0; i < n; i++)
      {
         char icon[PATH_MAX_LENGTH];
         NSImage *im = nil;
         /* Qt's per-system XMB dot-art icon when the asset exists. */
         if (companion_core_playlist_icon_path(wimp->core, i, icon, sizeof(icon)))
            im = [[[NSImage alloc] initWithContentsOfFile:BOXSTRING(icon)] autorelease_compat];
         [playlistIcons addObject:(im ? (id)im : (folder ? (id)folder : (id)[NSNull null]))];
      }
   }
   if (playlists)
      [playlists reloadData];

   /* Startup: select desktop_menu_initial_playlist (History as the
    * fallback), once, as the Qt companion does. */
   if (wimp && !started)
   {
      char initial[PATH_MAX_LENGTH];
      size_t i, n  = companion_core_playlist_count(wimp->core);
      long pick    = -1;
      started = YES;
      strlcpy(initial, companion_core_pref_initial_playlist(wimp->core),
            sizeof(initial));
      if (initial[0])
         for (i = 0; i < n && pick < 0; i++)
         {
            const char *p_i = companion_core_playlist_path(wimp->core, i);
            if (p_i && string_is_equal(p_i, initial))
               pick = (long)i;
         }
      /* No (or unknown) start playlist: All Playlists, index 0, as Qt. */
      if (pick < 0 && n > 0)
         pick = 0;
      if (pick >= 0 && playlists)
         [playlists selectRowIndexes:[NSIndexSet indexSetWithIndex:(NSUInteger)pick]
            byExtendingSelection:NO]; /* delegate loads it */
   }
}

- (void)reloadEntries
{
   char buf[64];
   size_t n;
   if (!entries)
      return;
   n = companion_core_entry_count(wimp->core);
   [self rebuildRowMap];
   [entries reloadData];
   if (grid)
   {
      [grid setCount:(NSInteger)n];
   }
   /* New list: drop queued decodes (the engine's cache stays), forget
    * the "no file" marks, and make the grid re-ask for its screen. */
   thumbGen++;
   if (thumbs)
      companion_thumbs_cancel(thumbs);
   free(thumbNone);
   thumbNone      = (char*)calloc(n ? n : 1, 1);
   thumbNoneCount = (NSInteger)n;
   visFirst = visLast = -1;
   /* Qt selects the first entry of a freshly loaded playlist. */
   if (n > 0)
   {
      [entries selectRowIndexes:[NSIndexSet indexSetWithIndex:0]
         byExtendingSelection:NO]; /* delegate updates the boxart */
      if (grid)
         [grid setSelectedRow:0];
   }
   {
      /* Qt's footer: "%1 items". */
      const char *fmt = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT);
      const char *p1  = strstr(fmt, "%1");
      if (p1)
         snprintf(buf, sizeof(buf), "%.*s%u%s", (int)(p1 - fmt), fmt, (unsigned)n, p1 + 2);
      else
         snprintf(buf, sizeof(buf), "%u", (unsigned)n);
   }
   if (itemsLabel)
      [itemsLabel setStringValue:BOXSTRING(buf)];
   [self statusDefault];
}

- (void)setIconView:(BOOL)icons
{
   if (!entriesScroll || iconView == icons)
      return;
   if (started)
      companion_core_pref_set_icon_view(wimp->core, icons);
   iconView = icons;
   if (icons)
   {
      [entriesScroll setDocumentView:grid];
      [grid relayout];
      [window makeFirstResponder:grid];
   }
   else
      [entriesScroll setDocumentView:entries];
   if (viewPopup)
      [viewPopup selectItemAtIndex:icons ? 1 : 0];
}

- (void)gridSelectionChanged:(NSInteger)row
{
   [self refreshBoxart];
   [self fillCorePopup:row];
   [self refreshInfo];
}

- (void)gridRun:(NSInteger)row
{
   char content[PATH_MAX_LENGTH];
   if (row < 0 || !wimp)
      return;
   if (companion_core_entry_needs_core(wimp->core, (size_t)row,
            content, sizeof(content)))
   {
      [self showCoresForContent:content];
      return;
   }
   if (companion_core_request_load_entry(wimp->core, (size_t)row))
      [self hideAndFocusRetroArch];
}

/* Called from the iterate hook: decode one pending grid thumbnail per
 * frame while the icon view is showing (one file decode per frame, any
 * playlist size), and keep the info pane following the core. */
/* Thumbnail file for entry @row, or NO. */
- (BOOL)thumbPathForRow:(NSInteger)row into:(char*)path len:(size_t)len
{
   char db_name[NAME_MAX_LENGTH];
   const struct playlist_entry *e = companion_core_entry(wimp->core, (size_t)row);
   if (!e)
      return NO;
   strlcpy(db_name, e->db_name ? e->db_name : "", sizeof(db_name));
   path_remove_extension(db_name);
   return companion_core_thumbnail_path(wimp->core, db_name,
         thumbSubdir ? thumbSubdir : COMPANION_THUMB_BOXART,
         !string_is_empty(e->label) ? e->label : path_basename(e->path),
         e->path, path, len) ? YES : NO;
}

/* ARGB pixels from the engine -> NSImage (a byte swap into an RGBA rep). */
static NSImage *cc_image_from_argb(const uint32_t *bits, int w, int h)
{
   NSBitmapImageRep *rep = [[[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h
      bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
      colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:w * 4
      bitsPerPixel:32] autorelease_compat];
   NSImage *img;
   unsigned char *dst;
   int i;
   if (!rep)
      return nil;
   dst = [rep bitmapData];
   for (i = 0; i < w * h; i++)
   {
      uint32_t p = bits[i];
      dst[i * 4 + 0] = (unsigned char)((p >> 16) & 0xff);
      dst[i * 4 + 1] = (unsigned char)((p >>  8) & 0xff);
      dst[i * 4 + 2] = (unsigned char)( p        & 0xff);
      dst[i * 4 + 3] = 0xff;
   }
   img = [[[NSImage alloc] initWithSize:NSMakeSize(w, h)] autorelease_compat];
   [img addRepresentation:rep];
   return img;
}

/* Boxart-pane requests carry the entry index and this bit. */
/* The generation rides in the top 32 bits where there are any. A
 * sizeof() ternary does not help - both arms are compiled, so a 32-bit
 * build (the 10.4 PPC / i386 cross) gets an undefined shift by 32. */
#if defined(UINTPTR_MAX) && UINTPTR_MAX > 0xffffffffu
#define CC_TAG(row, gen) ((uintptr_t)(row) | ((uintptr_t)(gen) << 32))
#define CC_TAG_GEN(t)    ((unsigned)((t) >> 32))
#define CC_TAG_HAS_GEN   1
#else
#define CC_TAG(row, gen) ((uintptr_t)(row))
#define CC_TAG_GEN(t)    (0u)
#define CC_TAG_HAS_GEN   0
#endif

#define CC_TAG_BOXART ((uintptr_t)1 << (sizeof(uintptr_t) * 8 - 1))

/* Engine delivery: tag = row | gen << 32. */
static void cc_thumb_done(void *ud, const char *path, int w, int h,
      uintptr_t tag, const uint32_t *bits)
{
   RACompanionController *self = (BRIDGE RACompanionController*)ud;
   (void)path;
   [self thumbDone:tag bits:bits width:w height:h];
}

/* Frames arrive at up to the container's rate: keep one rep for the
 * pane and copy each frame into its pixels (a byte swap into RGBA),
 * rather than allocating a rep and an image per frame. */
- (void)boxartBlit:(const uint32_t*)bits width:(int)w height:(int)h
{
   unsigned char *dst;
   int i;
   if (!boxartRep || boxartW != w || boxartH != h)
   {
      RELEASE(boxartRep);
      RELEASE(boxartImage);
      boxartRep = [[NSBitmapImageRep alloc]
         initWithBitmapDataPlanes:NULL pixelsWide:w pixelsHigh:h
         bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
         colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:w * 4
         bitsPerPixel:32];
      if (!boxartRep)
         return;
      boxartImage = [[NSImage alloc] initWithSize:NSMakeSize(w, h)];
      [boxartImage addRepresentation:boxartRep];
      boxartW = w;
      boxartH = h;
      [boxart setImage:boxartImage];
   }
   dst = [boxartRep bitmapData];
   for (i = 0; i < w * h; i++)
   {
      uint32_t p = bits[i];
      dst[i * 4 + 0] = (unsigned char)((p >> 16) & 0xff);
      dst[i * 4 + 1] = (unsigned char)((p >>  8) & 0xff);
      dst[i * 4 + 2] = (unsigned char)( p        & 0xff);
      dst[i * 4 + 3] = 0xff;
   }
   [boxart setNeedsDisplay:YES];
}

- (void)thumbDone:(uintptr_t)tag bits:(const uint32_t*)bits width:(int)w height:(int)h
{
   NSInteger row;
   if (tag & CC_TAG_BOXART)
   {
      /* The pane: show it if it is still the selected entry's. */
      if ((NSInteger)(tag & ~CC_TAG_BOXART) == boxartEntry && bits && boxart)
         [self boxartBlit:bits width:w height:h];
      return;
   }
   row = (NSInteger)(tag & 0xffffffffu);
   if (CC_TAG_HAS_GEN && CC_TAG_GEN(tag) != thumbGen)
      return;
   /* Against the array's own length: the entry count can have moved on
    * since it was allocated (a delivery for the previous list). */
   if (!grid || row < 0 || row >= thumbNoneCount
         || row >= (NSInteger)companion_core_entry_count(wimp->core))
      return;
   if (!bits)
   {
      if (thumbNone) thumbNone[row] = 1;
      return;
   }
   if (w != (int)[self thumbEdge])
      return; /* zoomed since */
   [grid setImage:cc_image_from_argb(bits, w, h) forRow:row];
}

- (CGFloat)thumbEdge
{
   unsigned z = companion_core_pref_icon_view_zoom(wimp->core);
   return 64.0 + (CGFloat)z * 256.0 / 100.0;
}

/* Make sure @row's thumbnail is installed (from the engine cache) or on
 * its way; @urgent for rows on screen, prefetch otherwise. */
- (void)thumbWant:(NSInteger)row urgent:(BOOL)urgent
{
   char path[PATH_MAX_LENGTH];
   int edge = (int)[self thumbEdge];
   const uint32_t *bits;
   /* iconTick prefetches a screen either side of the visible range, so
    * @row can be outside the list. hasImageForRow: answers NO for those
    * (it is not "no image", it is "no row"), and thumbNone is only as
    * long as the list: reading thumbNone[row] past the end is what
    * crashed on macOS under Guard Malloc. */
   if (row < 0 || row >= thumbNoneCount)
      return;
   if (!thumbs || !grid || [grid hasImageForRow:row])
      return;
   if (thumbNone && thumbNone[row])
      return;
   if (![self thumbPathForRow:row into:path len:sizeof(path)])
   {
      if (thumbNone) thumbNone[row] = 1;
      return;
   }
   bits = companion_thumbs_get(thumbs, path, edge, edge);
   if (bits)
   {
      [grid setImage:cc_image_from_argb(bits, edge, edge) forRow:row];
      return;
   }
   companion_thumbs_request(thumbs, path, edge, edge,
         CC_TAG(row, thumbGen),
         urgent ? true : false, 0xffffffffu);
}

/* Per frame: request what the grid shows (topmost served first), prefetch
 * the next screen, install what finished. The Qt companion's model. */
- (void)iconTick
{
   NSInteger first, last, i, span;
   if (!thumbs)
      return;
   if (!iconView || !grid || browseMode)
   {
      companion_thumbs_poll(thumbs, cc_thumb_done, (BRIDGE void*)self, 0, 4000);
      return;
   }
   if ([grid visibleRowsFirst:&first last:&last]
         && (first != visFirst || last != visLast))
   {
      visFirst = first;
      visLast  = last;
      for (i = last; i >= first; i--)
         [self thumbWant:i urgent:YES];
      span = last - first + 1;
      for (i = last + 1; i <= last + span; i++)
         [self thumbWant:i urgent:NO];
      for (i = first - 1; i >= 0 && i > first - span; i--)
         [self thumbWant:i urgent:NO];
   }
   companion_thumbs_poll(thumbs, cc_thumb_done, (BRIDGE void*)self, 0, 4000);
   /* Idle with empty cells on screen (a decode was abandoned or the
    * queue was full): ask for them again. */
   if (!companion_thumbs_pending(thumbs) && visFirst >= 0)
   {
      for (i = visFirst; i <= visLast && i < thumbNoneCount; i++)
         if (i >= 0 && ![grid hasImageForRow:i] && !(thumbNone && thumbNone[i]))
         {
            visFirst = visLast = -1;
            break;
         }
   }
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

/* A plain label in the small system font. */
- (NSTextField*)makeLabel:(const char*)text
{
   NSTextField *l = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, CC_LABEL_H)] autorelease_compat];
   [l setStringValue:BOXSTRING(text ? text : "")];
   [l setEditable:NO];
   [l setBordered:NO];
   [l setDrawsBackground:NO];
   [l setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
   return l;
}

- (NSButton*)makeButton:(const char*)title action:(SEL)sel
{
   NSButton *b = [[[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 60, CC_CTRL_H)] autorelease_compat];
   [b setTitle:BOXSTRING(title ? title : "")];
   [b setBezelStyle:NSRoundedBezelStyle];
   [b setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
   [b setTarget:self];
   [b setAction:sel];
   return b;
}

- (NSPopUpButton*)makePopup:(SEL)sel
{
   NSPopUpButton *pb = [[[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 100, CC_CTRL_H)
      pullsDown:NO] autorelease_compat];
   [pb setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
   [pb setTarget:self];
   [pb setAction:sel];
   return pb;
}

- (BOOL)buildWindow
{
   NSRect screen      = [[NSScreen mainScreen] visibleFrame];
   /* Qt opens at 1280x720 logical, centred, clamped to the screen. */
   CGFloat ww         = screen.size.width  < 1280.0 ? screen.size.width  : 1280.0;
   CGFloat wh         = screen.size.height <  720.0 ? screen.size.height :  720.0;
   NSRect frame       = NSMakeRect(0, 0, ww, wh);
   NSScrollView *sl   = nil;
   NSScrollView *sr   = nil;
   NSScrollView *si   = nil;
   NSView *content    = nil;
   NSMenu *menu       = nil;
   NSMenuItem *item   = nil;
   NSTabViewItem *tab = nil;

   window = [[NSWindow alloc] initWithContentRect:frame
      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
            | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
      backing:NSBackingStoreBuffered defer:NO];
   if (!window)
      return NO;

   [window setTitle:@"RetroArch"];
   [window setDelegate:self];
   [window setMinSize:NSMakeSize(640, 400)];
   [window setReleasedWhenClosed:NO];
   [window center];
   content = [window contentView];

   /* --- Left column: Search / Content Browser (tabs) / Core ----------- */
   searchLabel = [self makeLabel:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH)];
   KEEP_IVAR(searchLabel);
   [content addSubview:searchLabel];
   searchField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, CC_CTRL_H)];
   [searchField setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
   [searchField setTarget:self];
   [searchField setAction:@selector(searchChanged:)];
   [searchField setDelegate:(id)self];
   [content addSubview:searchField];
   clearButton = [self makeButton:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR)
      action:@selector(clearSearch:)];
   KEEP_IVAR(clearButton);
   [content addSubview:clearButton];

   browserLabel = [self makeLabel:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER)];
   KEEP_IVAR(browserLabel);
   [content addSubview:browserLabel];

   /* Playlists: icon + name, tall rows like Qt's. */
   playlists = RETAIN_COMPAT([self makeTable:NSMakeRect(0, 0, 200, 500)
         scroll:&sl twoColumns:NO]);
   playlistsScroll = sl;
   KEEP_IVAR(playlistsScroll);
   {
      NSTableColumn *ic = [[[NSTableColumn alloc] initWithIdentifier:@"icon"] autorelease_compat];
      NSImageCell *cell = [[[NSImageCell alloc] init] autorelease_compat];
      [cell setImageScaling:NSImageScaleProportionallyDown];
      [ic setDataCell:cell];
      [ic setWidth:CC_PL_ICON + 4.0];
      [playlists addTableColumn:ic];
      /* icon first, then the name */
      [playlists moveColumn:1 toColumn:0];
      [playlists setRowHeight:CC_PL_ROW_H];
      [playlists setHeaderView:nil];
      [[[playlists tableColumns] objectAtIndex:1] setWidth:CC_PANE_W - CC_PL_ICON - 30.0];
   }
   playlistIcons = [[NSMutableArray alloc] init];
   [playlists setDoubleAction:@selector(playlistsDoubleClick:)];
   [playlists setTarget:self];
   {
      char icon[PATH_MAX_LENGTH];
      if (companion_core_folder_icon_path(wimp->core, icon, sizeof(icon)))
         folderIcon = [[NSImage alloc] initWithContentsOfFile:BOXSTRING(icon)];
      if (!folderIcon)
         folderIcon = RETAIN_COMPAT([[NSWorkspace sharedWorkspace]
               iconForFileType:NSFileTypeForHFSTypeCode('fldr')]);
   }

   browserTabs = [[NSTabView alloc] initWithFrame:NSMakeRect(0, 0, CC_PANE_W, 300)];
   [browserTabs setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
   [browserTabs setDelegate:(id)self];
   tab = [[[NSTabViewItem alloc] initWithIdentifier:@"playlists"] autorelease_compat];
   [tab setLabel:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS))];
   [browserTabs addTabViewItem:tab];
   /* The table is not a tab item's view: it shows playlists under one
    * tab and folders under the other, so it lives beside the strip and
    * layoutViews places it. (Making it item 0's view hid it, and showed
    * an empty pane, whenever the File Browser tab was selected.) */
   [content addSubview:sl];
   tab = [[[NSTabViewItem alloc] initWithIdentifier:@"files"] autorelease_compat];
   [tab setLabel:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER))];
   [browserTabs addTabViewItem:tab];
   [content addSubview:browserTabs];
   brUp        = [self makeButton:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP)
      action:@selector(browseUp:)];
   brStart     = [self makeButton:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES)
      action:@selector(browseStart:)];
   brDownloads = [self makeButton:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST)
      action:@selector(browseDownloads:)];
   KEEP_IVAR(brUp); KEEP_IVAR(brStart); KEEP_IVAR(brDownloads);
   [brUp setHidden:YES]; [brStart setHidden:YES]; [brDownloads setHidden:YES];
   [content addSubview:brUp];
   [content addSubview:brStart];
   [content addSubview:brDownloads];

   coreLabel = [self makeLabel:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE)];
   KEEP_IVAR(coreLabel);
   [content addSubview:coreLabel];
   corePopup = [self makePopup:@selector(corePopupChanged:)];
   KEEP_IVAR(corePopup);
   [content addSubview:corePopup];
   corePaths  = [[NSMutableArray alloc] init];
   infoButton = [self makeButton:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_INFO)
      action:@selector(toggleInfo:)];
   KEEP_IVAR(infoButton);
   [content addSubview:infoButton];
   runButton  = [self makeButton:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RUN)
      action:@selector(runWithPopup:)];
   KEEP_IVAR(runButton);
   [content addSubview:runButton];
   /* Qt's Stop button beside Run: unloads the running core. */
   stopButton = [self makeButton:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_STOP)
      action:@selector(stopContent:)];
   KEEP_IVAR(stopButton);
   [content addSubview:stopButton];

   /* --- Centre: entries (table or grid) over Qt's footer -------------- */
   entries = RETAIN_COMPAT([self makeTable:NSMakeRect(0, 0, 600, 500)
         scroll:&sr twoColumns:YES]);
   [entries setDoubleAction:@selector(runSelected:)];
   [entries setTarget:self];
   [[[[entries tableColumns] objectAtIndex:0] headerCell]
      setStringValue:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME))];
   [[[[entries tableColumns] objectAtIndex:1] headerCell]
      setStringValue:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE))];
   /* Header clicks sort through the core (under the browser). Column 0
    * is Name, column 1 doubles as Type there; Size and Date columns
    * appear only in browse mode. */
   {
      NSTableColumn *sz = [[[NSTableColumn alloc] initWithIdentifier:@"size"] autorelease_compat];
      NSTableColumn *dt = [[[NSTableColumn alloc] initWithIdentifier:@"date"] autorelease_compat];
      [[sz headerCell] setStringValue:@"Size"];
      [[dt headerCell] setStringValue:@"Date Modified"];
      [sz setWidth:90.0];
      [dt setWidth:140.0];
      [entries addTableColumn:sz];
      [entries addTableColumn:dt];
      [sz setHidden:YES];
      [dt setHidden:YES];
      [[[entries tableColumns] objectAtIndex:0] setSortDescriptorPrototype:
         [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES]];
      [[[entries tableColumns] objectAtIndex:1] setSortDescriptorPrototype:
         [NSSortDescriptor sortDescriptorWithKey:@"type" ascending:YES]];
      [sz setSortDescriptorPrototype:[NSSortDescriptor sortDescriptorWithKey:@"size" ascending:YES]];
      [dt setSortDescriptorPrototype:[NSSortDescriptor sortDescriptorWithKey:@"date" ascending:YES]];
   }
   entriesScroll = RETAIN_COMPAT(sr);
   [content addSubview:sr];
   grid = [[RACompanionGrid alloc] initWithOwner:self];

   itemsLabel = [self makeLabel:""];
   KEEP_IVAR(itemsLabel);
   [content addSubview:itemsLabel];
   zoomLabel = [self makeLabel:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ZOOM)];
   KEEP_IVAR(zoomLabel);
   [zoomLabel setAlignment:NSTextAlignmentRight];
   [content addSubview:zoomLabel];
   zoomSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(0, 0, 140, CC_CTRL_H)];
   [zoomSlider setMinValue:0];
   [zoomSlider setMaxValue:100];
   [zoomSlider setTarget:self];
   [zoomSlider setAction:@selector(zoomChanged:)];
   [content addSubview:zoomSlider];
   thumbPopup = [self makePopup:@selector(thumbTypeChanged:)];
   KEEP_IVAR(thumbPopup);
   [thumbPopup addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART))];
   [thumbPopup addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT))];
   [thumbPopup addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN))];
   [thumbPopup addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO))];
   [content addSubview:thumbPopup];
   viewPopup = [self makePopup:@selector(viewChanged:)];
   KEEP_IVAR(viewPopup);
   [viewPopup addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST))];
   [viewPopup addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS))];
   [content addSubview:viewPopup];

   /* --- Right column: Core Info over Boxart (with its type tabs) ------ */
   infoLabel = [self makeLabel:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_INFO)];
   KEEP_IVAR(infoLabel);
   [content addSubview:infoLabel];
   infoTable  = RETAIN_COMPAT([self makeTable:NSMakeRect(0, 0, CC_PANE_W, 300)
         scroll:&si twoColumns:NO]);
   infoScroll = si;
   KEEP_IVAR(infoScroll);
   [infoTable setHeaderView:nil];
   [[[infoTable tableColumns] objectAtIndex:0] setWidth:800.0]; /* long lines scroll */
   [infoScroll setHasHorizontalScroller:YES];
   [content addSubview:infoScroll];

   boxartLabel = [self makeLabel:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART)];
   KEEP_IVAR(boxartLabel);
   [content addSubview:boxartLabel];
   boxartTypes = [[NSSegmentedControl alloc] initWithFrame:NSMakeRect(0, 0, CC_PANE_W, CC_CTRL_H)];
   [boxartTypes setSegmentCount:4];
   [boxartTypes setLabel:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART)) forSegment:0];
   [boxartTypes setLabel:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN)) forSegment:1];
   [boxartTypes setLabel:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT)) forSegment:2];
   [boxartTypes setLabel:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO)) forSegment:3];
   [boxartTypes setSelectedSegment:0];
   [boxartTypes setTarget:self];
   [boxartTypes setAction:@selector(boxartTypeChanged:)];
   [content addSubview:boxartTypes];
   boxartSubdir = COMPANION_THUMB_BOXART;
   boxart = [[RACompanionBoxart alloc] initWithFrame:NSMakeRect(0, 0, CC_PANE_W, 300)];
   ((RACompanionBoxart*)boxart)->owner = self;
   [boxart registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
   [boxart setImageScaling:NSImageScaleProportionallyUpOrDown];
   [boxart setImageFrameStyle:NSImageFrameGrayBezel];
   [content addSubview:boxart];
   infoVisible   = YES; /* Qt shows both docks by default */
   boxartVisible = YES;

   /* Log pane (hidden until Companion > Log). */
   logScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, CC_STATUS_H, frame.size.width, 120)];
   logView   = [[NSTextView alloc] initWithFrame:[[logScroll contentView] bounds]];
   [logView setEditable:NO];
   [logView setRichText:NO];
   [logView setAutoresizingMask:NSViewWidthSizable];
   [logScroll setDocumentView:logView];
   [logScroll setHasVerticalScroller:YES];

   status = [[NSTextField alloc] initWithFrame:NSMakeRect(4, 0, frame.size.width - 8, CC_STATUS_H)];
   [status setEditable:NO];
   [status setBordered:NO];
   [status setDrawsBackground:NO];
   [status setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
   [content addSubview:status];

   /* "Companion" menu on the main menu bar (Qt's File / View entries). */
   menu = [[[NSMenu alloc] initWithTitle:@"Companion"] autorelease_compat];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE))
      action:@selector(loadCore:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:@"Load Content..." action:@selector(loadContent:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_CORE))
      action:@selector(startCore:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE))
      action:@selector(unloadCore:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT))
      action:@selector(quitRetroArch:) keyEquivalent:@""];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:@"Browse Files" action:@selector(browseFiles:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:@"Scan Directory..." action:@selector(scanDirectory:) keyEquivalent:@""];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH))
      action:@selector(focusSearch:) keyEquivalent:@"f"];
   [item setTarget:self];
   item = [menu addItemWithTitle:@"Run Selected" action:@selector(runWithPopup:) keyEquivalent:@"\r"];
   [item setTarget:self];
   item = [menu addItemWithTitle:@"Refresh Playlists" action:@selector(refreshPlaylists:) keyEquivalent:@"r"];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST))
      action:@selector(viewList:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS))
      action:@selector(viewIcons:) keyEquivalent:@""];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:@"Log" action:@selector(toggleLog:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_INFO))
      action:@selector(toggleInfo:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART))
      action:@selector(toggleBoxart:) keyEquivalent:@""];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS))
      action:@selector(showCoreOptions:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS))
      action:@selector(showShaderParams:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS))
      action:@selector(showOptions:) keyEquivalent:@","];
   [item setTarget:self];
   [menu addItem:[NSMenuItem separatorItem]];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION))
      action:@selector(openDocs:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT))
      action:@selector(aboutRetroArch:) keyEquivalent:@""];
   [item setTarget:self];
   item = [menu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS))
      action:@selector(aboutContributors:) keyEquivalent:@""];
   [item setTarget:self];

   menuItem = [[NSMenuItem alloc] initWithTitle:@"Companion" action:NULL keyEquivalent:@""];
   [menuItem setSubmenu:menu];
   [[NSApp mainMenu] addItem:menuItem];

   /* Context menus. */
   entriesMenu = [[NSMenu alloc] initWithTitle:@""];
   item = [entriesMenu addItemWithTitle:@"Run" action:@selector(runSelected:) keyEquivalent:@""];
   [item setTarget:self];
   [entriesMenu addItem:[NSMenuItem separatorItem]];
   item = [entriesMenu addItemWithTitle:@"Delete Entry" action:@selector(deleteEntry:) keyEquivalent:@""];
   [item setTarget:self];
   [entries setMenu:entriesMenu];
   [entries registerForDraggedTypes:[NSArray arrayWithObject:NSFilenamesPboardType]];
   item = [entriesMenu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FILES))
      action:@selector(addFiles:) keyEquivalent:@""];
   [item setTarget:self];

   playlistsMenu = [[NSMenu alloc] initWithTitle:@""];
   assocMenu     = [[NSMenu alloc] initWithTitle:@"Associate Core"];
   [assocMenu setDelegate:self];
   item = [playlistsMenu addItemWithTitle:@"Associate Core" action:NULL keyEquivalent:@""];
   [item setSubmenu:assocMenu];
   item = [playlistsMenu addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST))
      action:@selector(renamePlaylist:) keyEquivalent:@""];
   [item setTarget:self];
   item = [playlistsMenu addItemWithTitle:@"Refresh Playlists" action:@selector(refreshPlaylists:) keyEquivalent:@""];
   [item setTarget:self];
   [playlists setMenu:playlistsMenu];

   [self layoutViews];
   [self statusDefault];
   [self fillCorePopup:-1];
   [self applyTheme];
   thumbs   = companion_thumbs_new(0, 0);
   visFirst = visLast = -1;
   return YES;
}

/* Explicit layout (10.4-safe; no autolayout), recomputed on resize.
 * Cocoa coordinates run bottom-up, so y grows towards the top. */
- (void)layoutViews
{
   NSRect b;
   CGFloat W, H, top, y, x, leftW, rightW, cx, cw;
   CGFloat logH = logVisible ? 120.0 : 0.0;
   if (!window)
      return;
   b     = [[window contentView] bounds];
   W     = b.size.width;
   H     = b.size.height;
   top   = H - CC_PAD;
   leftW = CC_PANE_W;
   rightW = (infoVisible || boxartVisible) ? CC_PANE_W : 0.0;
   if (W - leftW - rightW < 300.0)
      rightW = 0.0;

   [status setFrame:NSMakeRect(4, 0, W - 8, CC_STATUS_H)];
   if (logVisible)
      [logScroll setFrame:NSMakeRect(0, CC_STATUS_H, W, logH)];

   /* Left column, top-down. */
   x = CC_PAD;
   y = top - CC_LABEL_H;
   [searchLabel setFrame:NSMakeRect(x, y, leftW - 2 * CC_PAD, CC_LABEL_H)];
   y -= CC_CTRL_H + 2;
   [searchField setFrame:NSMakeRect(x, y, leftW - 3 * CC_PAD - 60.0, CC_CTRL_H)];
   [clearButton setFrame:NSMakeRect(leftW - CC_PAD - 60.0, y, 60.0, CC_CTRL_H)];
   y -= CC_PAD + CC_LABEL_H;
   [browserLabel setFrame:NSMakeRect(x, y, leftW - 2 * CC_PAD, CC_LABEL_H)];
   {
      /* Core section at the bottom of the column. */
      CGFloat coreY = CC_STATUS_H + logH + CC_PAD;
      [corePopup setFrame:NSMakeRect(x, coreY, leftW - 5 * CC_PAD - 3 * 50.0, CC_CTRL_H)];
      [infoButton setFrame:NSMakeRect(leftW - 3 * CC_PAD - 150.0, coreY, 50.0, CC_CTRL_H)];
      [runButton setFrame:NSMakeRect(leftW - 2 * CC_PAD - 100.0, coreY, 50.0, CC_CTRL_H)];
      [stopButton setFrame:NSMakeRect(leftW - CC_PAD - 50.0, coreY, 50.0, CC_CTRL_H)];
      coreY += CC_CTRL_H + 2;
      [coreLabel setFrame:NSMakeRect(x, coreY, leftW - 2 * CC_PAD, CC_LABEL_H)];
      coreY += CC_LABEL_H + CC_PAD;
      /* Tabs fill what is left between the browser label and Core;
       * under the browser a button row sits at the top of that area. */
      if (browseMode)
      {
         CGFloat bw3 = (leftW - 4 * CC_PAD) / 3;
         CGFloat by  = y - CC_PAD - CC_CTRL_H;
         [brUp        setFrame:NSMakeRect(x, by, bw3, CC_CTRL_H)];
         [brStart     setFrame:NSMakeRect(x + bw3 + CC_PAD, by, bw3, CC_CTRL_H)];
         [brDownloads setFrame:NSMakeRect(x + 2 * (bw3 + CC_PAD), by, bw3, CC_CTRL_H)];
         y = by - CC_PAD;
      }
      [brUp setHidden:!browseMode]; [brStart setHidden:!browseMode]; [brDownloads setHidden:!browseMode];
      {
         /* the strip on top, the table filling down to the Core section */
         CGFloat strip = 28.0;
         [browserTabs setFrame:NSMakeRect(x, y - CC_PAD - strip, leftW - 2 * CC_PAD, strip)];
         [playlistsScroll setFrame:NSMakeRect(x, coreY, leftW - 2 * CC_PAD,
               y - CC_PAD - strip - CC_PAD - coreY)];
      }
   }

   /* Centre. */
   cx = leftW;
   cw = W - leftW - rightW;
   {
      CGFloat fy = CC_STATUS_H + logH;
      [entriesScroll setFrame:NSMakeRect(cx + CC_PAD, fy + CC_FOOTER_H, cw - 2 * CC_PAD, H - CC_PAD - fy - CC_FOOTER_H)];
      [itemsLabel setFrame:NSMakeRect(cx + CC_PAD, fy + (CC_FOOTER_H - CC_LABEL_H) / 2, 160.0, CC_LABEL_H)];
      x = cx + cw - CC_PAD;
      x -= 100.0;
      [viewPopup setFrame:NSMakeRect(x, fy + (CC_FOOTER_H - CC_CTRL_H) / 2, 100.0, CC_CTRL_H)];
      x -= CC_PAD + 120.0;
      [thumbPopup setFrame:NSMakeRect(x, fy + (CC_FOOTER_H - CC_CTRL_H) / 2, 120.0, CC_CTRL_H)];
      x -= CC_PAD + 140.0;
      [zoomSlider setFrame:NSMakeRect(x, fy + (CC_FOOTER_H - CC_CTRL_H) / 2, 140.0, CC_CTRL_H)];
      x -= CC_PAD + 44.0;
      [zoomLabel setFrame:NSMakeRect(x, fy + (CC_FOOTER_H - CC_LABEL_H) / 2, 44.0, CC_LABEL_H)];
      if (iconView && grid)
         [grid relayout];
   }

   /* Right column: Core Info on top, Boxart below, 50/50 like Qt. */
   x = W - rightW + CC_PAD;
   {
      CGFloat colTop = top;
      CGFloat colBot = CC_STATUS_H + logH + CC_PAD;
      CGFloat colH   = colTop - colBot;
      CGFloat infoH  = boxartVisible ? (infoVisible ? colH / 2 : 0) : colH;
      CGFloat boxH   = colH - infoH;
      CGFloat w      = rightW - 2 * CC_PAD;
      BOOL showInfo  = rightW > 0 && infoVisible;
      BOOL showBox   = rightW > 0 && boxartVisible;
      [infoLabel  setHidden:!showInfo];
      [infoScroll setHidden:!showInfo];
      [boxartLabel setHidden:!showBox];
      [boxartTypes setHidden:!showBox];
      [boxart     setHidden:!showBox];
      if (showInfo)
      {
         [infoLabel  setFrame:NSMakeRect(x, colTop - CC_LABEL_H, w, CC_LABEL_H)];
         [infoScroll setFrame:NSMakeRect(x, colTop - infoH + CC_PAD, w, infoH - CC_LABEL_H - CC_PAD)];
      }
      if (showBox)
      {
         CGFloat by = colBot;
         [boxartLabel setFrame:NSMakeRect(x, by + boxH - CC_LABEL_H, w, CC_LABEL_H)];
         [boxartTypes setFrame:NSMakeRect(x, by + boxH - CC_LABEL_H - CC_CTRL_H, w, CC_CTRL_H)];
         [boxart      setFrame:NSMakeRect(x, by, w, boxH - CC_LABEL_H - CC_CTRL_H - CC_PAD)];
      }
   }
}

- (void)windowDidResize:(NSNotification*)note { [self layoutViews]; }

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
   if (thumbs)
   {
      companion_thumbs_free(thumbs); /* joins the decode threads first */
      thumbs = NULL;
   }
   free(thumbNone);
   thumbNone      = NULL;
   thumbNoneCount = 0;
   RELEASE(grid);
   RELEASE(entriesScroll);
   RELEASE(status);
   if (infoTable)
   {
      [infoTable setDataSource:nil];
      [infoTable setDelegate:nil];
      RELEASE(infoTable);
   }
   RELEASE(infoScroll);
   RELEASE(boxart);
   RELEASE(searchLabel);  RELEASE(browserLabel); RELEASE(coreLabel);
   RELEASE(infoLabel);    RELEASE(boxartLabel);  RELEASE(itemsLabel);
   RELEASE(zoomLabel);    RELEASE(searchField);  RELEASE(clearButton);
   RELEASE(infoButton);   RELEASE(runButton);    RELEASE(browserTabs);
   RELEASE(playlistsScroll); RELEASE(corePopup); RELEASE(corePaths);
   RELEASE(viewPopup);    RELEASE(thumbPopup);   RELEASE(zoomSlider);
   RELEASE(boxartTypes);  RELEASE(playlistIcons); RELEASE(folderIcon);
   RELEASE(brUp); RELEASE(brStart); RELEASE(brDownloads);
   RELEASE(stopButton);
   if (contributorsWindow) { [contributorsWindow orderOut:nil]; RELEASE(contributorsWindow); }
   if (optsWindow) { [optsWindow orderOut:nil]; RELEASE(optsWindow); }
   if (shpWindow)  { [shpWindow orderOut:nil];  RELEASE(shpWindow); }
   if (setWindow)  { [setWindow orderOut:nil];  RELEASE(setWindow); }
   RELEASE(optsTable); RELEASE(shpTable); RELEASE(setTable);
   RELEASE(boxartRep); RELEASE(boxartImage);
   free(rowMap);
   rowMap = NULL;
   string_list_free(infoKeys);
   string_list_free(infoValues);
   infoKeys   = NULL;
   infoValues = NULL;
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
/* Search filter: map visible table rows to entry indices (identity when
 * the filter is empty), as Qt's proxy model does. */
- (void)rebuildRowMap
{
   size_t i, n = wimp ? companion_core_entry_count(wimp->core) : 0;
   free(rowMap);
   rowMap   = NULL;
   rowCount = 0;
   if (browseMode)
   {
      /* The browser's content view is the whole listing, folders
       * first, like Qt's table. */
      size_t bc = companion_core_browse_count(wimp->core);
      rowMap = (NSInteger*)malloc((bc ? bc : 1) * sizeof(NSInteger));
      if (!rowMap)
         return;
      for (i = 0; i < bc; i++)
         rowMap[i] = (NSInteger)i;
      rowCount = (NSInteger)bc;
      return;
   }
   if (!filter[0])
   {
      rowCount = (NSInteger)n;
      return;
   }
   rowMap = (NSInteger*)malloc((n ? n : 1) * sizeof(NSInteger));
   if (!rowMap)
      return;
   for (i = 0; i < n; i++)
   {
      const struct playlist_entry *e = companion_core_entry(wimp->core, i);
      const char *label = e ? (!string_is_empty(e->label) ? e->label : e->path) : NULL;
      char low[PATH_MAX_LENGTH];
      size_t k;
      if (!label)
         continue;
      for (k = 0; label[k] && k < sizeof(low) - 1; k++)
         low[k] = (char)tolower((unsigned char)label[k]);
      low[k] = '\0';
      if (strstr(low, filter))
         rowMap[rowCount++] = (NSInteger)i;
   }
}

- (NSInteger)entryForRow:(NSInteger)row
{
   if (row < 0)
      return -1;
   if (rowMap)
      return (row < rowCount) ? rowMap[row] : -1;
   return row;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv
{
   if (!wimp)
      return 0;
   if (tv == optsTable)
      return (NSInteger)companion_core_option_count(wimp->core);
   if (tv == shpTable)
      return (NSInteger)companion_core_shader_param_count(wimp->core);
   if (tv == setTable)
      return (NSInteger)companion_core_setting_count(wimp->core);
   if (tv == playlists)
      return browseMode
         ? (NSInteger)companion_core_browse_dir_count(wimp->core)
         : (NSInteger)companion_core_playlist_count(wimp->core);
   if (tv == entries)
      return rowCount; /* under the browser: the files (see rebuildRowMap) */
   if (tv == coresTable)
      return coresRows;
   if (tv == infoTable)
      return infoKeys ? (NSInteger)infoKeys->size : 0;
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
   {
      if (browseMode)
      {
         /* Qt's File Browser: the folder pane. */
         static int logged = 0;
         if ([[col identifier] isEqualToString:@"icon"])
            return folderIcon;
         s = companion_core_browse_name(wimp->core, (size_t)row);
         if (!logged++)
            RARCH_LOG("[Companion] folder pane row %ld col '%s': name=[%s] utf8=%s\n",
                  (long)row, [[col identifier] UTF8String], s ? s : "(null)",
                  (s && [NSString stringWithUTF8String:s]) ? "ok" : "INVALID");
      }
      else if ([[col identifier] isEqualToString:@"icon"])
      {
         id im = ((NSUInteger)row < [playlistIcons count])
            ? [playlistIcons objectAtIndex:(NSUInteger)row] : nil;
         return (im && im != [NSNull null]) ? im : nil;
      }
      else
         s = companion_core_playlist_name(wimp->core, (size_t)row);
   }
   else if (tv == infoTable)
   {
      /* Qt renders "Key: value" on one line; do the same. */
      const char *k = (infoKeys   && (size_t)row < infoKeys->size)   ? infoKeys->elems[row].data   : "";
      const char *v = (infoValues && (size_t)row < infoValues->size) ? infoValues->elems[row].data : "";
      if (k && *k && v && *v)
         return [NSString stringWithFormat:@"%@ %@", BOXSTRING(k), BOXSTRING(v)];
      s = (k && *k) ? k : v;
   }
   else if (tv == optsTable)
   {
      if ([[col identifier] isEqualToString:@"val"])
         s = companion_core_option_value_label(wimp->core, (size_t)row,
               companion_core_option_current(wimp->core, (size_t)row));
      else
         s = companion_core_option_desc(wimp->core, (size_t)row);
   }
   else if (tv == shpTable)
   {
      NSString *cid = [col identifier];
      if ([cid isEqualToString:@"pval"])
         return [NSString stringWithFormat:@"%g", (double)companion_core_shader_param_current(wimp->core, (size_t)row)];
      if ([cid isEqualToString:@"range"])
      {
         float mn = 0, mx = 0, st = 0, ini = 0;
         companion_core_shader_param_range(wimp->core, (size_t)row, &mn, &mx, &st, &ini);
         return [NSString stringWithFormat:@"%g .. %g (step %g)", (double)mn, (double)mx, (double)st];
      }
      s = companion_core_shader_param_desc(wimp->core, (size_t)row);
   }
   else if (tv == setTable)
   {
      static char sbuf[PATH_MAX_LENGTH];
      if ([[col identifier] isEqualToString:@"sval"])
      {
         companion_core_setting_get(wimp->core, (size_t)row, sbuf, sizeof(sbuf));
         if (companion_core_setting_kind(wimp->core, (size_t)row) == COMPANION_SETTING_BOOL)
            return sbuf[0] == '1' ? @"Yes" : @"No";
         s = sbuf;
      }
      else
         s = companion_core_setting_label(wimp->core, (size_t)row);
   }
   else if (tv == coresTable)
      s = [[col identifier] isEqualToString:@"core"]
         ? companion_core_installed_core_version(wimp->core, (size_t)row)
         : companion_core_installed_core_name(wimp->core, (size_t)row);
   else if (tv == entries && browseMode)
   {
      NSInteger bi = [self entryForRow:row];
      if ([[col identifier] isEqualToString:@"core"]
            || [[col identifier] isEqualToString:@"size"]
            || [[col identifier] isEqualToString:@"date"])
      {
         /* Qt's Type / Size / Date columns, formatted by the core for
          * every backend. */
         char buf[64];
         NSString *id = [col identifier];
         if (bi < 0)
            return @"";
         if ([id isEqualToString:@"size"])
            s = companion_core_browse_size_str(wimp->core, (size_t)bi, buf, sizeof(buf));
         else if ([id isEqualToString:@"date"])
            s = companion_core_browse_date_str(wimp->core, (size_t)bi, buf, sizeof(buf));
         else
            s = companion_core_browse_type_str(wimp->core, (size_t)bi, buf, sizeof(buf));
         return BOXSTRING(s ? s : "");
      }
      else
         s = bi >= 0 ? companion_core_browse_name(wimp->core, (size_t)bi) : "";
   }
   else if (tv == entries)
   {
      NSInteger ei = [self entryForRow:row];
      const struct playlist_entry *e = ei >= 0
         ? companion_core_entry(wimp->core, (size_t)ei) : NULL;
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
   if (!wimp)
      return;
   if ([note object] == entries)
   {
      NSInteger row = [self entryForRow:[entries selectedRow]];
      [self refreshBoxart];
      [self fillCorePopup:row];
      [self refreshInfo];
      return;
   }
   if ([note object] != playlists)
      return;
   if (browseMode)
      return; /* folders: double-click descends (-playlistsDoubleClick:) */
   row = (int)[playlists selectedRow];
   if (row < 0)
      return;
   if (companion_core_select_playlist(wimp->core, (size_t)row))
      [self setStatus:"Loading playlist..."];
}


/* NSWindow delegate: closing the companion never quits RetroArch. */
/* Hide the companion and hand the keyboard back to RetroArch's own
 * window (closing, or loading content), as the Qt companion does. */
- (void)hideAndFocusRetroArch
{
   NSWindow *host = nil;
   id rv          = nil;
   [window orderOut:nil];
   /* RetroArch's window is the one hosting the render view - asked of
    * the platform, not of CocoaView, which on a Metal build is not the
    * render view and sits in no window. The render view must be first
    * responder again too, or key events go nowhere. */
   if ([(id)apple_platform respondsToSelector:@selector(hostWindow)])
      host = [(id)apple_platform hostWindow];
   if ([(id)apple_platform respondsToSelector:@selector(renderView)])
      rv = [(id)apple_platform renderView];
   if (!host && rv && [rv respondsToSelector:@selector(window)])
      host = [rv window];
   if (!host)
      host = [[CocoaView get] window];
   if (host)
   {
      [NSApp activateIgnoringOtherApps:YES];
      [host makeKeyAndOrderFront:nil];
      [host makeMainWindow];
      if (rv && [rv isKindOfClass:[NSView class]])
         [host makeFirstResponder:(NSView*)rv];
   }
}

/* AppKit re-arbitrates the key window as part of closing the one that
 * was key, which can run after windowShouldClose: returned - so the
 * hand-back is repeated once the close has completed, and once more on
 * the next run-loop pass, to win over that arbitration. */
- (void)focusRetroArchDeferred:(id)unused
{
   NSWindow *host = nil;
   id rv          = nil;
   (void)unused;
   if ([window isVisible])
      return;                     /* re-shown meanwhile: leave it */
   if ([(id)apple_platform respondsToSelector:@selector(hostWindow)])
      host = [(id)apple_platform hostWindow];
   if ([(id)apple_platform respondsToSelector:@selector(renderView)])
      rv = [(id)apple_platform renderView];
   if (!host)
   {
      RARCH_LOG("[Companion] focus hand-back: no host window\n");
      return;
   }
   [host makeKeyAndOrderFront:nil];
   [host makeMainWindow];
   if (rv && [rv isKindOfClass:[NSView class]])
      [host makeFirstResponder:(NSView*)rv];
   RARCH_LOG("[Companion] focus hand-back: host key=%d main=%d appKey=%s\n",
         [host isKeyWindow] ? 1 : 0, [host isMainWindow] ? 1 : 0,
         [NSApp keyWindow] == host ? "host" : ([NSApp keyWindow] ? "other" : "none"));
}

- (void)windowWillClose:(NSNotification*)note
{
   if ([note object] != window)
      return;
   [self focusRetroArchDeferred:nil];
   [self scheduleFocusHandback];
}

/* The companion window stopped being key. If it is on its way out (not
 * visible), the keyboard belongs to RetroArch's window: AppKit's own
 * choice of the next key window, made while a close is in progress,
 * has been seen to land on nothing - the mouse still works (it goes to
 * the window under it) but key events go nowhere. */
- (void)windowDidResignKey:(NSNotification*)note
{
   if ([note object] != window || [window isVisible])
      return;
   [self scheduleFocusHandback];
}

/* Re-assert the hand-back after AppKit's close processing, in every
 * run-loop mode (the close button's click is dispatched in the event-
 * tracking mode, where a default-mode timer would wait). Repeated on
 * three successive passes: the arbitration can run late. */
- (void)scheduleFocusHandback
{
   int i;
   for (i = 0; i < 3; i++)
      [self performSelector:@selector(focusRetroArchDeferred:) withObject:nil
         afterDelay:(0.02 * i) inModes:[NSArray arrayWithObjects:
            NSDefaultRunLoopMode, NSEventTrackingRunLoopMode, NSModalPanelRunLoopMode, nil]];
}

- (BOOL)windowShouldClose:(id)sender
{
   /* Let AppKit close it (setReleasedWhenClosed:NO keeps the object;
    * -show orders it back in). Returning NO and hiding it ourselves
    * left AppKit believing the window was still open - and still the
    * key window - so key events went to a hidden window and the
    * keyboard never came back to RetroArch. The hand-back runs from
    * windowWillClose: and windowDidResignKey:, which only fire on a
    * real close. */
   return YES;
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
   char content[PATH_MAX_LENGTH];
   NSInteger row;
   if (!wimp)
      return;
   row = [self entryForRow:[self actionRowIn:entries]];
   if (row < 0)
      return;

   if (browseMode)
   {
      bool needs_core = false;
      int r = companion_core_browse_activate(wimp->core, (size_t)row,
            NULL, &needs_core, content, sizeof(content));
      if (r == 0)
         [self setStatus:"Loading..."]; /* entered a directory: lands via callback */
      else if (r == 1)
         [self hideAndFocusRetroArch]; /* content loaded */
      else if (needs_core)
         [self showCoresForContent:content];
      return;
   }

   /* No usable core: ask, filtered to what runs this content. */
   if (companion_core_entry_needs_core(wimp->core, (size_t)row,
            content, sizeof(content)))
   {
      [self showCoresForContent:content];
      return;
   }
   if (companion_core_request_load_entry(wimp->core, (size_t)row))
      [self hideAndFocusRetroArch];
}

/* Left pane double-click under the browser: descend into the folder. */
- (void)playlistsDoubleClick:(id)sender
{
   NSInteger sel;
   if (!browseMode)
      return;
   sel = [playlists clickedRow];
   if (sel < 0)
      sel = [playlists selectedRow];
   if (sel >= 0 && companion_core_browse_activate(wimp->core, (size_t)sel,
            NULL, NULL, NULL, 0) == 0)
      [self setStatus:"Loading..."];
}

- (void)browseUp:(id)sender
{
   if (browseMode && companion_core_browse_up(wimp->core))
      [self setStatus:"Loading..."];
}

- (void)browseStart:(id)sender
{
   settings_t *st = config_get_ptr();
   if (browseMode && companion_core_browse_open(wimp->core,
            !string_is_empty(st->paths.directory_menu_content)
            ? st->paths.directory_menu_content : NULL))
      [self setStatus:"Loading..."];
}

- (void)browseDownloads:(id)sender
{
   settings_t *st = config_get_ptr();
   if (browseMode && !string_is_empty(st->paths.directory_core_assets)
         && companion_core_browse_open(wimp->core, st->paths.directory_core_assets))
      [self setStatus:"Loading..."];
}

- (void)browseLanded
{
   if (browseMode)
      [self browseReload];
}

/* NSTableView: a header click changed the sort descriptors. */
- (void)tableView:(NSTableView*)tv sortDescriptorsDidChange:(NSArray*)old
{
   NSSortDescriptor *d;
   enum companion_browse_column col = COMPANION_BROWSE_SORT_NAME;
   NSString *key;
   if (tv != entries || !browseMode || syncingSort || ![[tv sortDescriptors] count])
      return;
   d   = [[tv sortDescriptors] objectAtIndex:0];
   key = [d key];
   if ([key isEqualToString:@"size"])      col = COMPANION_BROWSE_SORT_SIZE;
   else if ([key isEqualToString:@"type"]) col = COMPANION_BROWSE_SORT_TYPE;
   else if ([key isEqualToString:@"date"]) col = COMPANION_BROWSE_SORT_DATE;
   companion_core_browse_sort(wimp->core, col, [d ascending] ? true : false);
   /* on_browse_changed -> browseLanded -> browseReload */
}

/* The header's sort column and arrow follow the core's order - the
 * same rule as Qt's header and the Win32 header, whether the order came
 * from a click here or was remembered from before. */
- (void)syncSortIndicator
{
   NSString *key;
   NSSortDescriptor *d;
   NSTableColumn *col;
   switch (companion_core_browse_sort_column(wimp->core))
   {
      case COMPANION_BROWSE_SORT_SIZE: key = @"size"; break;
      case COMPANION_BROWSE_SORT_TYPE: key = @"type"; break;
      case COMPANION_BROWSE_SORT_DATE: key = @"date"; break;
      default:                         key = @"name"; break;
   }
   d = [NSSortDescriptor sortDescriptorWithKey:key
         ascending:companion_core_browse_sort_ascending(wimp->core) ? YES : NO];
   syncingSort = YES;
   [entries setSortDescriptors:[NSArray arrayWithObject:d]];
   syncingSort = NO;
   /* the highlighted column + arrow: the column whose prototype key matches */
   col = nil;
   if ([key isEqualToString:@"name"])      col = [[entries tableColumns] objectAtIndex:0];
   else if ([key isEqualToString:@"type"]) col = [[entries tableColumns] objectAtIndex:1];
   else                                    col = [entries tableColumnWithIdentifier:key];
   if (col)
   {
      [entries setHighlightedTableColumn:col];
      [entries setIndicatorImage:[NSImage imageNamed:
         ([d ascending] ? @"NSAscendingSortIndicator" : @"NSDescendingSortIndicator")]
         inTableColumn:col];
   }
}

/* Both panes from the current browse listing. */
- (void)browseReload
{
   const char *dir = companion_core_browse_dir(wimp->core);
   [self rebuildRowMap];
   [playlists reloadData];
   [entries reloadData];
   [self syncSortIndicator];
   [self setStatus:(dir && *dir) ? dir : "/"];
   {
      char buf[64];
      const char *fmt = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT);
      const char *p1  = strstr(fmt, "%1");
      if (p1)
         snprintf(buf, sizeof(buf), "%.*s%u%s", (int)(p1 - fmt), fmt,
               (unsigned)rowCount, p1 + 2);
      else
         snprintf(buf, sizeof(buf), "%u", (unsigned)rowCount);
      if (itemsLabel)
         [itemsLabel setStringValue:BOXSTRING(buf)];
   }
}

- (void)browseFiles:(id)sender
{
   browseMode = YES;
   [[entries tableColumnWithIdentifier:@"size"] setHidden:NO];
   [[entries tableColumnWithIdentifier:@"date"] setHidden:NO];
   [self setIconView:NO];   /* the browser is a list */
   [self layoutViews];      /* the button row appears */
   if (!companion_core_browse_count(wimp->core) && !companion_core_browse_busy(wimp->core))
   {
      companion_core_browse_open(wimp->core, NULL); /* lands via callback */
      [self setStatus:"Loading..."];
   }
   else
      [self browseReload];
   /* Qt shows no boxart, and no entry's core, for the browser. */
   [self refreshBoxart];
   [self fillCorePopup:-1];
   [self refreshInfo];
   if (browserTabs && [browserTabs indexOfTabViewItem:[browserTabs selectedTabViewItem]] != 1)
      [browserTabs selectTabViewItemAtIndex:1];
   companion_core_pref_set_last_tab(wimp->core, 1);
}

/* NSTabView delegate: the second tab is the file browser. */
- (void)tabView:(NSTabView*)tv didSelectTabViewItem:(NSTabViewItem*)item
{
   NSInteger idx = [tv indexOfTabViewItem:item];
   if (idx == 1)
   {
      if (!browseMode)
         [self browseFiles:nil];
   }
   else if (browseMode)
   {
      browseMode = NO;
      [[entries tableColumnWithIdentifier:@"size"] setHidden:YES];
      [[entries tableColumnWithIdentifier:@"date"] setHidden:YES];
      [entries setHighlightedTableColumn:nil];
      {
         NSUInteger k;
         for (k = 0; k < [[entries tableColumns] count]; k++)
            [entries setIndicatorImage:nil inTableColumn:[[entries tableColumns] objectAtIndex:k]];
      }
      [self reloadPlaylists];
      [self rebuildRowMap];
      [entries reloadData];
      [self layoutViews];
      companion_core_pref_set_last_tab(wimp->core, 0);
   }
}

- (void)tabChanged:(id)sender { }

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
   row = [self entryForRow:[self actionRowIn:entries]];
   sel = companion_core_selected_playlist(wimp->core);
   if (row < 0 || sel == (size_t)-1)
      return;

   alert = [[NSAlert alloc] init];
   [alert setMessageText:@"Delete this playlist entry?"];
   [alert addButtonWithTitle:@"Delete"];
   [alert addButtonWithTitle:@"Cancel"];
   if ([alert runModal] == NSAlertFirstButtonReturn)
   {
      /* The entry's own file and its index there (differ from the
       * selected slot / aggregate row under All Playlists). */
      const char *pl = companion_core_entry_playlist_path(wimp->core, (size_t)row);
      if (pl && companion_core_playlist_delete_entry(wimp->core, pl,
               companion_core_entry_index_in_playlist(wimp->core, (size_t)row)))
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

- (void)refreshInfo
{
   if (!wimp || !infoVisible)
      return;
   string_list_free(infoKeys);
   string_list_free(infoValues);
   infoKeys   = string_list_new();
   infoValues = string_list_new();
   strlcpy(infoCore, [self popupCorePath], sizeof(infoCore));
   if (infoKeys && infoValues)
      companion_core_core_info_rows(infoCore, infoKeys, infoValues);
   [infoTable reloadData];
}

/* Called from the iterate hook: a short strcmp per frame while shown. */
- (void)infoFollowCore
{
   if (wimp && strcmp(infoCore, [self popupCorePath]))
   {
      if (infoVisible)
         [self refreshInfo];
      else
         strlcpy(infoCore, [self popupCorePath], sizeof(infoCore));
      [self statusDefault];
   }
}

/* Boxart pane: from the engine cache at once, else an urgent request
 * that lands in -thumbDone:. Cleared meanwhile; never decodes on the UI
 * thread. */
- (void)refreshBoxart
{
   NSInteger row;
   char path[PATH_MAX_LENGTH];
   char db_name[NAME_MAX_LENGTH];
   const struct playlist_entry *e;
   const uint32_t *bits;
   NSSize sz;
   int bw, bh;

   [boxart setImage:nil];
   RELEASE(boxartRep);
   RELEASE(boxartImage);
   boxartRep = nil; boxartImage = nil; boxartW = boxartH = 0;
   boxartEntry = -1;
   if (thumbs)
      companion_thumbs_animate_stop(thumbs); /* the old selection's animation */
   if (!boxartVisible || !boxart || !thumbs)
      return;
   if (browseMode)
   {
      /* Qt previews an image file selected in the browser. */
      NSInteger bi = [self entryForRow:[entries selectedRow]];
      const char *fp = bi >= 0 ? companion_core_browse_path(wimp->core, (size_t)bi) : NULL;
      if (!fp || companion_core_browse_is_dir(wimp->core, (size_t)bi)
            || image_texture_get_type(fp) == IMAGE_TYPE_NONE)
         return;
      strlcpy(path, fp, sizeof(path));
      row         = bi;
      boxartEntry = 0x40000000L | bi;
   }
   else
   {
      row = iconView ? [grid selectedRow] : [self entryForRow:[entries selectedRow]];
      if (row < 0 || !(e = companion_core_entry(wimp->core, (size_t)row)))
         return;
      boxartEntry = row;
      strlcpy(db_name, e->db_name ? e->db_name : "", sizeof(db_name));
      path_remove_extension(db_name);
      if (!companion_core_thumbnail_path(wimp->core, db_name,
               boxartSubdir ? boxartSubdir : COMPANION_THUMB_BOXART,
               !string_is_empty(e->label) ? e->label : path_basename(e->path),
               e->path, path, sizeof(path)))
         return;
   }
   sz = [boxart bounds].size;
   bw = (int)sz.width  - 4;
   bh = (int)sz.height - 4;
   if (bw < 1 || bh < 1)
      return;
   bits = companion_thumbs_get(thumbs, path, bw, bh);
   if (bits)
      [self boxartBlit:bits width:bw height:bh];
   else
      companion_thumbs_request(thumbs, path, bw, bh,
            (uintptr_t)boxartEntry | CC_TAG_BOXART, true, 0xffe8e8e8u);
   /* Like RetroArch's File Browser: an animated file plays in the pane
    * (frames arrive in -thumbDone: with the pane's tag). */
   companion_thumbs_animate(thumbs, path, bw, bh,
         (uintptr_t)boxartEntry | CC_TAG_BOXART, 0xffe8e8e8u);
   (void)row;
}

- (void)toggleBoxart:(id)sender
{
   boxartVisible = !boxartVisible;
   [self layoutViews];
   if (boxartVisible)
      [self refreshBoxart];
   return;
}


- (void)toggleInfo:(id)sender
{
   infoVisible = !infoVisible;
   [self layoutViews];
   if (infoVisible)
      [self refreshInfo];
   return;
}


- (void)viewList:(id)sender  { [self setIconView:NO]; }
- (void)viewIcons:(id)sender { [self setIconView:YES]; }

- (void)toggleLog:(id)sender
{
   if (!window || !logScroll)
      return;
   logVisible = !logVisible;
   if (logVisible)
      [[window contentView] addSubview:logScroll];
   else
      [logScroll removeFromSuperview];
   [self layoutViews];
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
/* Shared companion settings (retroarch.cfg), applied at startup. */
- (void)applySharedSettings
{
   unsigned z;
   if (!wimp)
      return;
   thumbSubdir = companion_core_pref_thumbnail_subdir(wimp->core);
   if (thumbPopup)
      [thumbPopup selectItemAtIndex:(NSInteger)companion_core_pref_thumbnail_type(wimp->core)];
   z = companion_core_pref_icon_view_zoom(wimp->core);
   if (zoomSlider)
      [zoomSlider setDoubleValue:(double)z];
   if (grid)
      [grid setThumbEdge:64.0 + (CGFloat)z * 256.0 / 100.0];
   if (companion_core_pref_icon_view(wimp->core))
      [self setIconView:YES];
   if (companion_core_pref_last_tab(wimp->core) == 1)
      [self browseFiles:nil];
}

/* --- Qt-layout actions -------------------------------------------------- */

- (void)statusDefault
{
   char buf[NAME_MAX_LENGTH + 32];
   const char *core = wimp ? companion_core_current_core_name(wimp->core) : NULL;
   snprintf(buf, sizeof(buf), "%s - %s", PACKAGE_VERSION,
         (core && *core) ? core : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE));
   [self setStatus:buf];
}

/* Launch-with popup: the running / entry's / playlist-default cores for
 * @entryRow, then "Ask" and "Load Core..."; a parallel path per row. */
- (void)fillCorePopup:(NSInteger)entryRow
{
   companion_launch_option_t opts[6];
   size_t i, n = 0;
   const struct playlist_entry *e = NULL;
   char pl_name[NAME_MAX_LENGTH];
   char label[64];
   if (!wimp || !corePopup)
      return;
   [corePopup removeAllItems];
   [corePaths removeAllObjects];
   pl_name[0] = '\0';
   if (entryRow >= 0 && !browseMode)
      e = companion_core_entry(wimp->core, (size_t)entryRow);
   if (e && e->db_name)
   {
      strlcpy(pl_name, e->db_name, sizeof(pl_name));
      path_remove_extension(pl_name);
   }
   n = companion_core_launch_options(wimp->core,
         e ? e->core_path : NULL, e ? e->core_name : NULL, pl_name,
         companion_core_pref_suggest_loaded_core_first(wimp->core),
         opts, sizeof(opts) / sizeof(opts[0]));
   for (i = 0; i < n; i++)
   {
      [corePopup addItemWithTitle:BOXSTRING(opts[i].name)];
      [[corePopup lastItem] setTag:(NSInteger)opts[i].selection];
      [corePaths addObject:BOXSTRING(opts[i].path)];
   }
   [corePopup addItemWithTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK))];
   [[corePopup lastItem] setTag:COMPANION_LAUNCH_ASK];
   [corePaths addObject:@""];
   snprintf(label, sizeof(label), "%s...", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE));
   [corePopup addItemWithTitle:BOXSTRING(label)];
   [[corePopup lastItem] setTag:COMPANION_LAUNCH_LOAD_CORE];
   [corePaths addObject:@""];
   [corePopup selectItemAtIndex:0];
}

/* Core the popup names, or the running core's path. */
- (const char*)popupCorePath
{
   NSInteger idx = corePopup ? [corePopup indexOfSelectedItem] : -1;
   if (idx >= 0 && (NSUInteger)idx < [corePaths count])
   {
      NSInteger tag = [[corePopup itemAtIndex:idx] tag];
      NSString *pth = [corePaths objectAtIndex:(NSUInteger)idx];
      if ((tag == COMPANION_LAUNCH_CURRENT || tag == COMPANION_LAUNCH_PLAYLIST_SAVED
               || tag == COMPANION_LAUNCH_PLAYLIST_DEFAULT) && [pth length])
         return [pth UTF8String];
   }
   return wimp ? companion_core_current_core_path(wimp->core) : "";
}

- (void)corePopupChanged:(id)sender { [self refreshInfo]; }

/* Run with the popup's choice (Qt's Run button). */
- (void)runWithPopup:(id)sender
{
   NSInteger row, idx, tag;
   const struct playlist_entry *e;
   if (!wimp)
      return;
   row = iconView ? [grid selectedRow] : [entries selectedRow];
   if (browseMode || row < 0)
   {
      [self runSelected:sender];
      return;
   }
   e = companion_core_entry(wimp->core, (size_t)row);
   if (!e)
      return;
   idx = [corePopup indexOfSelectedItem];
   tag = idx >= 0 ? [[corePopup itemAtIndex:idx] tag] : COMPANION_LAUNCH_ASK;
   switch (tag)
   {
      case COMPANION_LAUNCH_CURRENT:
      case COMPANION_LAUNCH_PLAYLIST_SAVED:
      case COMPANION_LAUNCH_PLAYLIST_DEFAULT:
         if (companion_core_request_load_content(wimp->core,
                  [[corePaths objectAtIndex:(NSUInteger)idx] UTF8String],
                  e->path, e->label, e->db_name, e->crc32))
            [self hideAndFocusRetroArch];
         else
            [self setStatus:"Failed to load the content."];
         return;
      case COMPANION_LAUNCH_LOAD_CORE:
         [self showCoresForContent:NULL];
         return;
      default:
         [self showCoresForContent:e->path];
         return;
   }
}

- (void)zoomChanged:(id)sender
{
   unsigned z = (unsigned)[zoomSlider doubleValue];
   companion_core_pref_set_icon_view_zoom(wimp->core, z);
   if (grid)
   {
      [grid setThumbEdge:64.0 + (CGFloat)z * 256.0 / 100.0];
      [grid setCount:(NSInteger)companion_core_entry_count(wimp->core)];
   }
   thumbGen++;
   if (thumbs)
      companion_thumbs_cancel(thumbs);
   visFirst = visLast = -1;
}

- (void)thumbTypeChanged:(id)sender
{
   unsigned t = (unsigned)[thumbPopup indexOfSelectedItem];
   companion_core_pref_set_thumbnail_type(wimp->core, t);
   thumbSubdir = companion_core_pref_thumbnail_subdir(wimp->core);
   /* redraw the grid at the new type: images come back from the engine
    * (cached per path, so a type already seen is instant) */
   if (grid)
   {
      [grid setCount:(NSInteger)companion_core_entry_count(wimp->core)];
   }
   thumbGen++;
   if (thumbs)
      companion_thumbs_cancel(thumbs);
   if (thumbNone && thumbNoneCount > 0)
      memset(thumbNone, 0, (size_t)thumbNoneCount);
   visFirst = visLast = -1;
}

- (void)viewChanged:(id)sender
{
   [self setIconView:[viewPopup indexOfSelectedItem] == 1];
}

- (void)boxartTypeChanged:(id)sender
{
   switch ([boxartTypes selectedSegment])
   {
      case 1:  boxartSubdir = COMPANION_THUMB_TITLE;      break;
      case 2:  boxartSubdir = COMPANION_THUMB_SCREENSHOT; break;
      case 3:  boxartSubdir = COMPANION_THUMB_LOGO;       break;
      default: boxartSubdir = COMPANION_THUMB_BOXART;     break;
   }
   [self refreshBoxart];
}

- (void)clearSearch:(id)sender
{
   [searchField setStringValue:@""];
   filter[0] = '\0';
   [self rebuildRowMap];
   [entries reloadData];
}

- (void)focusSearch:(id)sender { [window makeFirstResponder:searchField]; }

/* Search: NSTextField's delegate hook for edits; filters the entry list
 * the way Qt's proxy model does (case-insensitive substring). Applied
 * at draw time for the table; the aggregate stays untouched. */
- (void)controlTextDidChange:(NSNotification*)note
{
   if ([note object] == searchField)
   {
      const char *raw = [[searchField stringValue] UTF8String];
      size_t k;
      for (k = 0; raw && raw[k] && k < sizeof(filter) - 1; k++)
         filter[k] = (char)tolower((unsigned char)raw[k]);
      filter[k] = '\0';
      [self rebuildRowMap];
      [entries reloadData];
   }
}

- (void)searchChanged:(id)sender { }

/* Qt's Stop button and File > Unload Core: the same thing. */
- (void)stopContent:(id)sender
{
   if (!wimp)
      return;
   companion_core_unload_core(wimp->core);
   [self fillCorePopup:-1];
   [self refreshInfo];
}

- (void)unloadCore:(id)sender
{
   [self stopContent:sender];
}

- (void)quitRetroArch:(id)sender
{
   if (wimp)
      companion_core_event_command(wimp->core, CMD_EVENT_QUIT);
}

- (void)aboutRetroArch:(id)sender
{
   NSAlert *a = [[[NSAlert alloc] init] autorelease_compat];
   [a setMessageText:@"RetroArch"];
   [a setInformativeText:[NSString stringWithFormat:@"%s %s\n%s",
      msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT), PACKAGE_VERSION,
      "www.libretro.com"]];
   [a runModal];
}

/* Qt's About Contributors: the AUTHORS list in a scrolling text window. */
- (void)aboutContributors:(id)sender
{
   if (!contributorsWindow)
   {
      NSRect fr = NSMakeRect(0, 0, 520, 460);
      NSScrollView *sv;
      NSTextView *tv;
      contributorsWindow = [[NSWindow alloc] initWithContentRect:fr
         styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask)
         backing:NSBackingStoreBuffered defer:NO];
      [contributorsWindow setTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS))];
      [contributorsWindow setReleasedWhenClosed:NO];
      sv = [[[NSScrollView alloc] initWithFrame:fr] autorelease_compat];
      [sv setHasVerticalScroller:YES];
      [sv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
      tv = [[[NSTextView alloc] initWithFrame:fr] autorelease_compat];
      [tv setEditable:NO];
      [tv setFont:[NSFont userFixedPitchFontOfSize:11.0]];
      [tv setString:BOXSTRING(retroarch_contributors_list)];
      [tv setAutoresizingMask:NSViewWidthSizable];
      [sv setDocumentView:tv];
      [[contributorsWindow contentView] addSubview:sv];
      [contributorsWindow center];
   }
   [contributorsWindow makeKeyAndOrderFront:nil];
}

/* --- Qt's rename / add files / drops ------------------------------------- */

/* Rename the selected playlist: a sheet with a text field; the core
 * moves the file and refreshes the list (special playlists refused). */
- (void)renamePlaylist:(id)sender
{
   NSInteger row = [playlists selectedRow];
   const char *path;
   NSAlert *a;
   NSTextField *field;
   char name[NAME_MAX_LENGTH];
   if (!wimp || browseMode || row < 0)
      return;
   path = companion_core_playlist_path(wimp->core, (size_t)row);
   if (!path || string_is_equal(path, COMPANION_ALL_PLAYLISTS_TOKEN))
      return;
   fill_pathname(name, path_basename(path), "", sizeof(name));
   a = [[[NSAlert alloc] init] autorelease_compat];
   [a setMessageText:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST))];
   [a addButtonWithTitle:@"OK"];
   [a addButtonWithTitle:@"Cancel"];
   field = [[[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)] autorelease_compat];
   [field setStringValue:BOXSTRING(name)];
   /* 10.5+ (and not GNUstep): the field sits in the alert */
   if ([a respondsToSelector:@selector(setAccessoryView:)])
      [a performSelector:@selector(setAccessoryView:) withObject:field];
   if ([a runModal] != NSAlertFirstButtonReturn)
      return;
   [self renamePlaylistAtRow:row to:[[field stringValue] UTF8String]];
}

/* The rename itself (the harness calls this without the sheet). */
- (BOOL)renamePlaylistAtRow:(NSInteger)row to:(const char*)newName
{
   const char *path;
   if (!wimp || row < 0 || !newName || !*newName)
      return NO;
   path = companion_core_playlist_path(wimp->core, (size_t)row);
   if (!path)
      return NO;
   if (!companion_core_playlist_rename(wimp->core, path, newName, NULL, 0))
   {
      [self setStatus:"Could not rename the playlist"];
      return NO;
   }
   return YES;   /* on_playlists_changed reloads the list */
}

/* Add @paths (files or directories) to the selected playlist. */
- (size_t)addPaths:(NSArray*)paths
{
   const char *pl = wimp ? companion_core_selected_playlist_path(wimp->core) : NULL;
   const char **cp;
   NSUInteger i, n = [paths count];
   size_t added;
   char msg[96];
   if (!pl || browseMode || !n)
      return 0;
   cp = (const char**)malloc(n * sizeof(char*));
   if (!cp)
      return 0;
   for (i = 0; i < n; i++)
      cp[i] = [[paths objectAtIndex:i] UTF8String];
   added = companion_core_playlist_add_files(wimp->core, pl, cp, (size_t)n, NULL, NULL);
   free((void*)cp);
   snprintf(msg, sizeof(msg), "%u file(s) added", (unsigned)added);
   [self setStatus:msg];
   return added;
}

- (void)addFiles:(id)sender
{
   NSOpenPanel *panel = [NSOpenPanel openPanel];
   [panel setCanChooseDirectories:YES];
   [panel setCanChooseFiles:YES];
   [panel setAllowsMultipleSelection:YES];
   [panel setTitle:BOXSTRING(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ADD_FILES))];
   if ([panel runModal] != 1)
      return;
   {
      NSArray *urls = [panel performSelector:@selector(URLs)];
      NSMutableArray *paths = [NSMutableArray array];
      NSUInteger i;
      for (i = 0; i < [urls count]; i++)
         [paths addObject:[[urls objectAtIndex:i] path]];
      [self addPaths:paths];
   }
}

/* An image dropped on the boxart pane: the selected entry's thumbnail
 * of the pane's type (Qt's changeThumbnail), then shown again. */
- (BOOL)installThumbnailFromPath:(const char*)imagePath
{
   NSInteger row;
   const struct playlist_entry *e;
   char db_name[NAME_MAX_LENGTH], out[PATH_MAX_LENGTH];
   if (!wimp || browseMode || !imagePath || image_texture_get_type(imagePath) == IMAGE_TYPE_NONE)
      return NO;
   row = iconView ? [grid selectedRow] : [self entryForRow:[entries selectedRow]];
   if (row < 0 || !(e = companion_core_entry(wimp->core, (size_t)row)))
      return NO;
   strlcpy(db_name, e->db_name ? e->db_name : "", sizeof(db_name));
   path_remove_extension(db_name);
   if (!companion_core_thumbnail_install(wimp->core, db_name,
            boxartSubdir ? boxartSubdir : COMPANION_THUMB_BOXART,
            !string_is_empty(e->label) ? e->label : path_basename(e->path),
            imagePath, out, sizeof(out)))
   {
      [self setStatus:"Could not save the thumbnail"];
      return NO;
   }
   if (thumbs)
      companion_thumbs_forget(thumbs, out);   /* the file changed on disk */
   boxartEntry = -1;
   [self refreshBoxart];
   thumbGen++;
   if (thumbNone && thumbNoneCount > 0)
      memset(thumbNone, 0, (size_t)thumbNoneCount);
   visFirst = visLast = -1;
   [self setStatus:"Thumbnail updated"];
   return YES;
}

/* NSTableView drop support (files onto the entries table). */
- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id)info
      proposedRow:(NSInteger)row proposedDropOperation:(NSTableViewDropOperation)op
{
   (void)row; (void)op; (void)info;
   return (tv == entries && !browseMode) ? NSDragOperationCopy : NSDragOperationNone;
}

- (BOOL)tableView:(NSTableView*)tv acceptDrop:(id)info row:(NSInteger)row
      dropOperation:(NSTableViewDropOperation)op
{
   NSArray *files;
   (void)row; (void)op;
   if (tv != entries || browseMode)
      return NO;
   files = [[info draggingPasteboard] propertyListForType:NSFilenamesPboardType];
   return [self addPaths:files] > 0;
}

/* --- Core Options / Shader Parameters windows (Qt's dialogs) ---------- */

/* A window holding one table with the given columns and a row of
 * buttons; the table's data source and delegate are the controller. */
- (NSWindow*)makeTableWindow:(const char*)title columns:(NSArray*)titles
      ids:(NSArray*)ids widths:(NSArray*)widths table:(NSTableView**)tableOut
      buttons:(NSArray*)buttonTitles actions:(NSArray*)actions
{
   NSRect fr = NSMakeRect(0, 0, 600, 420);
   NSWindow *win = [[NSWindow alloc] initWithContentRect:fr
      styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask)
      backing:NSBackingStoreBuffered defer:NO];
   NSScrollView *sv = [[[NSScrollView alloc] initWithFrame:NSMakeRect(0, 40, 600, 380)] autorelease_compat];
   NSTableView *tv  = [[[NSTableView alloc] initWithFrame:[[sv contentView] bounds]] autorelease_compat];
   NSUInteger i;
   CGFloat x = 8;
   [win setTitle:BOXSTRING(title)];
   [win setReleasedWhenClosed:NO];
   for (i = 0; i < [titles count]; i++)
   {
      NSTableColumn *c = [[[NSTableColumn alloc] initWithIdentifier:[ids objectAtIndex:i]] autorelease_compat];
      [[c headerCell] setStringValue:[titles objectAtIndex:i]];
      [c setWidth:[[widths objectAtIndex:i] doubleValue]];
      [tv addTableColumn:c];
   }
   [tv setDataSource:self];
   [tv setDelegate:self];
   [tv setAllowsMultipleSelection:NO];
   [tv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
   [sv setDocumentView:tv];
   [sv setHasVerticalScroller:YES];
   [sv setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
   [[win contentView] addSubview:sv];
   for (i = 0; i < [buttonTitles count]; i++)
   {
      NSButton *b = [self makeButton:[[buttonTitles objectAtIndex:i] UTF8String]
         action:NSSelectorFromString([actions objectAtIndex:i])];
      [b setFrame:NSMakeRect(x, 8, 100, 24)];
      [b setAutoresizingMask:NSViewMaxXMargin | NSViewMaxYMargin];
      [[win contentView] addSubview:b];
      x += 108;
   }
   [win center];
   *tableOut = RETAIN_COMPAT(tv);
   return win;
}

- (void)showCoreOptions:(id)sender
{
   if (!optsWindow)
   {
      NSTableView *tv = nil;
      optsWindow = [self makeTableWindow:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS)
         columns:[NSArray arrayWithObjects:@"Option", @"Value", nil]
         ids:[NSArray arrayWithObjects:@"opt", @"val", nil]
         widths:[NSArray arrayWithObjects:[NSNumber numberWithDouble:330.0], [NSNumber numberWithDouble:220.0], nil]
         table:&tv
         buttons:[NSArray arrayWithObjects:@"Reset", @"Reset All", nil]
         actions:[NSArray arrayWithObjects:@"optionReset:", @"optionResetAll:", nil]];
      optsTable = tv;
      [optsTable setTarget:self];
      [optsTable setDoubleAction:@selector(optionCycle:)];
   }
   [optsTable reloadData];
   [optsWindow makeKeyAndOrderFront:nil];
}

- (void)optionCycle:(id)sender
{
   NSInteger row = [optsTable clickedRow] >= 0 ? [optsTable clickedRow] : [optsTable selectedRow];
   size_t nv;
   if (!wimp || row < 0)
      return;
   nv = companion_core_option_value_count(wimp->core, (size_t)row);
   if (nv)
      companion_core_option_set(wimp->core, (size_t)row,
            (companion_core_option_current(wimp->core, (size_t)row) + 1) % nv);
   [optsTable reloadData];
}

- (void)optionReset:(id)sender
{
   NSInteger row = [optsTable selectedRow];
   if (wimp && row >= 0)
      companion_core_option_reset(wimp->core, (size_t)row);
   [optsTable reloadData];
}

- (void)optionResetAll:(id)sender
{
   if (wimp)
      companion_core_option_reset_all(wimp->core);
   [optsTable reloadData];
}

- (void)showShaderParams:(id)sender
{
   if (!shpWindow)
   {
      NSTableView *tv = nil;
      shpWindow = [self makeTableWindow:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS)
         columns:[NSArray arrayWithObjects:@"Parameter", @"Value", @"Range", nil]
         ids:[NSArray arrayWithObjects:@"param", @"pval", @"range", nil]
         widths:[NSArray arrayWithObjects:[NSNumber numberWithDouble:280.0], [NSNumber numberWithDouble:90.0], [NSNumber numberWithDouble:200.0], nil]
         table:&tv
         buttons:[NSArray arrayWithObjects:@"Apply", @"Reset", nil]
         actions:[NSArray arrayWithObjects:@"shaderApply:", @"shaderReset:", nil]];
      shpTable = tv;
      [[shpTable tableColumnWithIdentifier:@"pval"] setEditable:YES];
   }
   [shpTable reloadData];
   [shpWindow makeKeyAndOrderFront:nil];
}

- (void)shaderApply:(id)sender
{
   if (wimp)
      companion_core_shader_apply(wimp->core);
   [shpTable reloadData];
}

- (void)shaderReset:(id)sender
{
   NSInteger row = [shpTable selectedRow];
   if (wimp && row >= 0)
   {
      companion_core_shader_param_reset(wimp->core, (size_t)row);
      companion_core_shader_apply(wimp->core);
   }
   [shpTable reloadData];
}

/* Editing the Value column of the shader table sets the parameter. */
- (void)tableView:(NSTableView*)tv setObjectValue:(id)obj forTableColumn:(NSTableColumn*)col row:(NSInteger)row
{
   if (tv == shpTable && wimp && [[col identifier] isEqualToString:@"pval"])
      companion_core_shader_param_set(wimp->core, (size_t)row, [[obj description] floatValue]);
   else if (tv == setTable && wimp && [[col identifier] isEqualToString:@"sval"])
   {
      if (!companion_core_setting_set(wimp->core, (size_t)row, [[obj description] UTF8String]))
         NSBeep();
      [self applyTheme];
   }
}

/* --- Options window (Qt's View > Options) ------------------------------
 * setting | value, the Value column editable; a bool toggles and a
 * choice cycles on double-click. Each edit sets through the core. */
- (void)showOptions:(id)sender
{
   if (!setWindow)
   {
      NSTableView *tv = nil;
      setWindow = [self makeTableWindow:msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS)
         columns:[NSArray arrayWithObjects:@"Setting", @"Value", nil]
         ids:[NSArray arrayWithObjects:@"setting", @"sval", nil]
         widths:[NSArray arrayWithObjects:[NSNumber numberWithDouble:330.0], [NSNumber numberWithDouble:240.0], nil]
         table:&tv
         buttons:[NSArray array] actions:[NSArray array]];
      setTable = tv;
      [[setTable tableColumnWithIdentifier:@"sval"] setEditable:YES];
      [setTable setTarget:self];
      [setTable setDoubleAction:@selector(settingActivate:)];
   }
   [setTable reloadData];
   [setWindow makeKeyAndOrderFront:nil];
}

- (void)settingActivate:(id)sender
{
   NSInteger row = [setTable clickedRow] >= 0 ? [setTable clickedRow] : [setTable selectedRow];
   char buf[PATH_MAX_LENGTH];
   if (!wimp || row < 0)
      return;
   companion_core_setting_get(wimp->core, (size_t)row, buf, sizeof(buf));
   switch (companion_core_setting_kind(wimp->core, (size_t)row))
   {
      case COMPANION_SETTING_BOOL:
         companion_core_setting_set(wimp->core, (size_t)row, buf[0] == '1' ? "0" : "1");
         break;
      case COMPANION_SETTING_CHOICE:
         {
            size_t c, n = companion_core_setting_choice_count(wimp->core, (size_t)row);
            for (c = 0; c < n; c++)
               if (string_is_equal(companion_core_setting_choice(wimp->core, (size_t)row, c), buf))
                  break;
            if (n)
               companion_core_setting_set(wimp->core, (size_t)row,
                     companion_core_setting_choice(wimp->core, (size_t)row, (c + 1) % n));
            [self applyTheme];
         }
         break;
      default:
         [setTable editColumn:1 row:row withEvent:nil select:YES];
         return;
   }
   [setTable reloadData];
}

/* Qt's dark theme setting, honoured on 10.14+: the companion's windows
 * take the dark appearance; "System default" follows the system. */
- (void)applyTheme
{
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 101400 && !defined(GNUSTEP)
   unsigned theme = config_get_ptr()->uints.desktop_menu_theme;
   NSAppearance *ap = nil;
   if (theme == 1 && [NSAppearance respondsToSelector:@selector(appearanceNamed:)])
      ap = [NSAppearance appearanceNamed:@"NSAppearanceNameDarkAqua"];
   if ([window respondsToSelector:@selector(setAppearance:)])
      [window performSelector:@selector(setAppearance:) withObject:ap];
#endif
}

- (void)openDocs:(id)sender
{
   [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://docs.libretro.com/"]];
}

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
#ifndef GNUSTEP
   else
   {
      /* 10.4 / 10.5: the pre-URL API, called through the Apple runtime
       * (the GNU runtime, used by the Linux regression harness, has no
       * objc_msgSend and never lacks -URL). */
      response = ((NSInteger (*)(id, SEL, id, id))objc_msgSend)(panel,
            @selector(runModalForDirectory:file:), nil, nil);
      if (response == 1)
         path = ((id (*)(id, SEL))objc_msgSend)(panel, @selector(filename));
   }
#else
   else
      response = 0;
#endif

   if (response != 1 || !path)
      return;

   if (companion_core_request_scan(wimp->core, [path UTF8String], true,
            companion_core_pref_show_hidden_files(wimp->core)))
      [self setStatus:"Scanning..."];
   else
      [self setStatus:"Scanning is not available in this build."];
}

/* The companion's own Load Core window (like Qt's LoadCoreWindow):
 * a table of installed cores, Load / Cancel. Non-modal; the tables'
 * data source is this controller, keyed on the table object. */
- (void)buildCoresWindow
{
   if (coresWindow || !wimp)
      return;
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
}

- (void)loadCore:(id)sender
{
   [self showCoresForContent:NULL];
}

/* Populate and raise the Load Core window. @content NULL: every core,
 * a plain "load a core". Non-NULL: the cores that run it first and only
 * those, and the pick launches that content. */
- (void)showCoresForContent:(const char*)content
{
   if (!wimp)
      return;
   [self buildCoresWindow];
   if (!coresWindow)
      return;

   if (content && *content)
   {
      NSInteger supported;
      strlcpy(coresContent, content, sizeof(coresContent));
      supported = (NSInteger)companion_core_installed_cores_supporting(
            wimp->core, coresContent);
      coresRows = (supported > 0) ? supported
         : (NSInteger)companion_core_installed_core_count(wimp->core);
   }
   else
   {
      coresContent[0] = '\0';
      coresRows = (NSInteger)companion_core_installed_core_count(wimp->core);
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

   if (coresContent[0])
   {
      const char *core_path =
         companion_core_installed_core_path(wimp->core, (size_t)row);
      if (companion_core_request_load_content(wimp->core, core_path,
               coresContent, NULL, NULL, NULL))
         [self hideAndFocusRetroArch];
      else
         [self setStatus:"Failed to load the content."];
      coresContent[0] = '\0';
   }
   else if (companion_core_load_core(wimp->core,
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

   [CC_CTRL(w) applySharedSettings];
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
   /* The keys that opened us (a hotkey, Enter in the menu) will be
    * released into this window, not RetroArch's: forget them there, or
    * the menu waits for a release that never comes and the keyboard is
    * dead after the companion closes. RetroArch's window delegate does
    * the same on resigning key; this covers a toggle from a non-key
    * state too. */
   apple_input_keyboard_reset();
}

static void ui_companion_cocoa_wimp_iterate(void *data)
{
   ui_companion_cocoa_wimp_t *w = (ui_companion_cocoa_wimp_t*)data;
   if (!w)
      return;
   companion_core_iterate(w->core, COMPANION_COCOA_ITER_US);
   if (w->controller)
   {
      [CC_CTRL(w) iconTick];
      [CC_CTRL(w) infoFollowCore];
   }
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
