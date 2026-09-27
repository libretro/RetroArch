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

#ifndef __COMPANION_DOCK_H
#define __COMPANION_DOCK_H

#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

#include "companion_core.h"

RETRO_BEGIN_DECLS

/* The dock layout of a desktop companion, as the Qt companion's
 * QMainWindow keeps it and as the native companions render it: four
 * sides around the content view, each an ordered list of slots; a slot
 * holds one pane, or several as a tab group with one raised; a pane can
 * instead float as a window of its own; any pane can be hidden. The
 * model owns the placement, the sizes and the hit-testing; a backend
 * owns the pane contents and draws what the model tells it to.
 *
 * Sides: left and right stack their slots top to bottom and share the
 * side's width; top and bottom lay theirs left to right and share the
 * side's height. Top and bottom span the full width, as QMainWindow's
 * default corners have it. Sizes are in the backend's units (pixels or
 * points); 0 means the backend's default. */

#define COMPANION_DOCK_SIDES 4 /* COMPANION_DOCK_LEFT .. COMPANION_DOCK_BOTTOM */

typedef struct companion_dock_slot
{
   int size;                              /* along the side: height on
                                             left/right, width on top/bottom;
                                             0 = default */
   int n;                                 /* members; > 1 is a tab group */
   int raised;                            /* index into members */
   unsigned char members[COMPANION_DOCK_COUNT]; /* pane ids, tab order */
} companion_dock_slot_t;

typedef struct companion_dock_layout
{
   companion_dock_slot_t slots[COMPANION_DOCK_SIDES][COMPANION_DOCK_COUNT];
   companion_rect_t      floats[COMPANION_DOCK_COUNT]; /* screen rect, floating panes */
   int  nslots[COMPANION_DOCK_SIDES];
   int  side_size[COMPANION_DOCK_SIDES];  /* width of left/right, height of top/bottom; 0 = default */
   unsigned char area[COMPANION_DOCK_COUNT]; /* enum companion_dock_area of each pane */
   bool shown[COMPANION_DOCK_COUNT];
} companion_dock_layout_t;

/* The Qt companion's default layout: Search, Playlists and Core down
 * the left, the four thumbnail panes tabbed together (Boxart raised)
 * above Core Info on the right, the Log hidden along the bottom. */
void companion_dock_default(companion_dock_layout_t *l);

/* The layout from the shared dock rows. False - and @l left at the
 * default - when desktop_menu_save_dock_positions is off or no row
 * parses. */
bool companion_dock_from_rows(struct settings *settings,
      companion_dock_layout_t *l);
/* The layout into the shared dock rows (a no-op when
 * desktop_menu_save_dock_positions is off). Sizes are written as they
 * are; a backend keeping pixels passes a layout already in logical
 * units. */
void companion_dock_to_rows(struct settings *settings,
      const companion_dock_layout_t *l);

/* --- Edits. Each leaves @l consistent: a pane is in exactly one
 * place, an emptied slot is dropped, a group's raised member exists. */

/* Dock @pane as a slot of its own on @side, before slot @index (past
 * the end appends). */
void companion_dock_place(companion_dock_layout_t *l, enum companion_dock_id pane,
      enum companion_dock_area side, int index);
/* Dock @pane into @onto's tab group (making one if @onto stands
 * alone) and raise it. */
void companion_dock_tabify(companion_dock_layout_t *l, enum companion_dock_id pane,
      enum companion_dock_id onto);
/* Float @pane at @rect (screen coordinates). */
void companion_dock_float(companion_dock_layout_t *l, enum companion_dock_id pane,
      const companion_rect_t *rect);
void companion_dock_set_shown(companion_dock_layout_t *l, enum companion_dock_id pane,
      bool shown);
/* Raise @pane in its tab group (no-op elsewhere). */
void companion_dock_raise(companion_dock_layout_t *l, enum companion_dock_id pane);
/* The slot @pane is in: side and index, or false when floating. */
bool companion_dock_find(const companion_dock_layout_t *l, enum companion_dock_id pane,
      enum companion_dock_area *side, int *index);
/* The pane a tab group shows: the raised member of @pane's slot. */
enum companion_dock_id companion_dock_raised(const companion_dock_layout_t *l,
      enum companion_dock_id pane);
/* True when @pane is in a slot with other members. */
bool companion_dock_is_tabbed(const companion_dock_layout_t *l,
      enum companion_dock_id pane);

/* --- Geometry. */

typedef struct companion_dock_metrics
{
   int gap;        /* the draggable gap between slots and around a side */
   int strip_h;    /* a pane's title strip */
   int tab_h;      /* a tab group's tab bar */
   int min_pane;   /* a pane's content never goes below this */
   int def_side;   /* the left and right sides' default width */
   int def_bar;    /* the top and bottom sides' default height */
   int def_slot;   /* a slot's default size when 0 (shared equally when all are 0) */
} companion_dock_metrics_t;

/* One draggable gap: between two slots of a side (@index is the slot
 * above / left of it) or between a side and the content (@index -1). */
typedef struct companion_dock_gap
{
   companion_rect_t rect;
   int side;
   int index;
} companion_dock_gap_t;

typedef struct companion_dock_geometry
{
   companion_rect_t content;                       /* what is left for the content view */
   companion_rect_t pane[COMPANION_DOCK_COUNT];    /* the pane's content area (docked, shown, raised) */
   companion_rect_t strip[COMPANION_DOCK_COUNT];   /* its title strip */
   companion_rect_t tabbar[COMPANION_DOCK_COUNT];  /* a group's tab bar (on the raised member) */
   companion_dock_gap_t gaps[COMPANION_DOCK_COUNT * 2 + COMPANION_DOCK_SIDES];
   int ngaps;
   bool laid_out[COMPANION_DOCK_COUNT];            /* docked, shown and raised: has a rect */
} companion_dock_geometry_t;

/* Lay @l out inside @client. Docked, shown panes get rects; a group's
 * members other than the raised one are laid out nowhere (the backend
 * hides them). Slot sizes in @l are read as preferences and clamped
 * to what fits; the sizes actually used are written back so a later
 * companion_dock_to_rows() persists what was on screen. */
void companion_dock_layout(companion_dock_layout_t *l,
      const companion_dock_metrics_t *m, const companion_rect_t *client,
      companion_dock_geometry_t *g);

/* --- Hit testing and drag targets, in @client coordinates. */

enum companion_dock_hit_kind
{
   COMPANION_DOCK_HIT_NONE = 0,
   COMPANION_DOCK_HIT_STRIP,   /* a pane's title strip: pane */
   COMPANION_DOCK_HIT_TAB,     /* a tab of a group: pane (the tab's) */
   COMPANION_DOCK_HIT_GAP      /* a splitter gap: gap */
};

typedef struct companion_dock_hit
{
   enum companion_dock_hit_kind kind;
   enum companion_dock_id pane;
   int gap;                    /* index into geometry.gaps */
} companion_dock_hit_t;

void companion_dock_hit_test(const companion_dock_layout_t *l,
      const companion_dock_geometry_t *g, const companion_dock_metrics_t *m,
      int x, int y, companion_dock_hit_t *hit);

/* Move the gap @gap to @pos (its x for a vertical gap, y for a
 * horizontal one), resizing the side or the two slots it separates. */
void companion_dock_drag_gap(companion_dock_layout_t *l,
      const companion_dock_geometry_t *g, const companion_dock_metrics_t *m,
      const companion_rect_t *client, int gap, int pos);

enum companion_dock_drop_kind
{
   COMPANION_DOCK_DROP_FLOAT = 0, /* outside any target: float at the pointer */
   COMPANION_DOCK_DROP_SLOT,      /* a slot of its own on side, before index */
   COMPANION_DOCK_DROP_TAB        /* into the tab group of pane */
};

typedef struct companion_dock_drop
{
   enum companion_dock_drop_kind kind;
   enum companion_dock_area side;
   int index;
   enum companion_dock_id onto;
   companion_rect_t indicator;    /* where to draw the drop marker (client coords) */
} companion_dock_drop_t;

/* Where dropping @pane at (@x, @y) would put it: over the middle of a
 * docked pane its tab group, over its first or last third a slot
 * before or after it, within a band along an edge of the client the
 * end of that side, elsewhere floating. */
void companion_dock_drop_target(const companion_dock_layout_t *l,
      const companion_dock_geometry_t *g, const companion_dock_metrics_t *m,
      const companion_rect_t *client, enum companion_dock_id pane,
      int x, int y, companion_dock_drop_t *drop);
/* Apply a drop_target() result. A float drop takes @rect. */
void companion_dock_apply_drop(companion_dock_layout_t *l,
      enum companion_dock_id pane, const companion_dock_drop_t *drop,
      const companion_rect_t *rect);

RETRO_END_DECLS

#endif
