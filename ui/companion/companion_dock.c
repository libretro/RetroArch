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

#include <string.h>

#include <string/stdstring.h>

#include "companion_dock.h"
#include "../../configuration.h"

/* Left and right run their slots vertically; top and bottom run them
 * horizontally. */
static bool dock_side_vertical(int side)
{
   return side == COMPANION_DOCK_LEFT || side == COMPANION_DOCK_RIGHT;
}

static bool dock_rect_has(const companion_rect_t *r, int x, int y)
{
   return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

/* Members of @s that are shown. */
static int dock_slot_shown(const companion_dock_layout_t *l,
      const companion_dock_slot_t *s)
{
   int i, n = 0;
   for (i = 0; i < s->n; i++)
      if (l->shown[s->members[i]])
         n++;
   return n;
}

/* The member of @s that is laid out: the raised one when shown, else
 * the first shown one; -1 when none is. */
static int dock_slot_visible_member(const companion_dock_layout_t *l,
      const companion_dock_slot_t *s)
{
   int i;
   if (s->raised >= 0 && s->raised < s->n && l->shown[s->members[s->raised]])
      return s->raised;
   for (i = 0; i < s->n; i++)
      if (l->shown[s->members[i]])
         return i;
   return -1;
}

void companion_dock_default(companion_dock_layout_t *l)
{
   companion_dock_slot_t *s;
   int i;
   memset(l, 0, sizeof(*l));
   for (i = 0; i < COMPANION_DOCK_COUNT; i++)
      l->shown[i] = true;

   /* Left: Search, Playlists, Core. */
   for (i = COMPANION_DOCK_SEARCH; i <= COMPANION_DOCK_CORE; i++)
   {
      s = &l->slots[COMPANION_DOCK_LEFT][l->nslots[COMPANION_DOCK_LEFT]++];
      s->members[0] = (unsigned char)i;
      s->n          = 1;
      l->area[i]    = COMPANION_DOCK_LEFT;
   }
   /* Right: the four thumbnail panes tabbed (Boxart raised), Core Info. */
   s = &l->slots[COMPANION_DOCK_RIGHT][l->nslots[COMPANION_DOCK_RIGHT]++];
   for (i = COMPANION_DOCK_BOXART; i <= COMPANION_DOCK_LOGO; i++)
   {
      s->members[s->n++] = (unsigned char)i;
      l->area[i]         = COMPANION_DOCK_RIGHT;
   }
   s = &l->slots[COMPANION_DOCK_RIGHT][l->nslots[COMPANION_DOCK_RIGHT]++];
   s->members[0]                     = COMPANION_DOCK_CORE_INFO;
   s->n                              = 1;
   l->area[COMPANION_DOCK_CORE_INFO] = COMPANION_DOCK_RIGHT;
   /* Bottom: the Log, hidden. */
   s = &l->slots[COMPANION_DOCK_BOTTOM][l->nslots[COMPANION_DOCK_BOTTOM]++];
   s->members[0]                = COMPANION_DOCK_LOG;
   s->n                         = 1;
   l->area[COMPANION_DOCK_LOG]  = COMPANION_DOCK_BOTTOM;
   l->shown[COMPANION_DOCK_LOG] = false;
}

/* --- Edits ------------------------------------------------------------ */

bool companion_dock_find(const companion_dock_layout_t *l, enum companion_dock_id pane,
      enum companion_dock_area *side, int *index)
{
   int s, i, m;
   if ((unsigned)pane >= COMPANION_DOCK_COUNT || l->area[pane] == COMPANION_DOCK_FLOAT)
      return false;
   s = l->area[pane];
   for (i = 0; i < l->nslots[s]; i++)
      for (m = 0; m < l->slots[s][i].n; m++)
         if (l->slots[s][i].members[m] == pane)
         {
            if (side)  *side  = (enum companion_dock_area)s;
            if (index) *index = i;
            return true;
         }
   return false;
}

/* Take @pane out of the slot it is in (dropping the slot when that
 * empties it); true when it was docked, with where it was and whether
 * its slot went with it. */
static bool dock_detach(companion_dock_layout_t *l, enum companion_dock_id pane,
      int *side_out, int *index_out, bool *slot_gone)
{
   enum companion_dock_area side;
   int idx, m, k;
   companion_dock_slot_t *s;
   if (slot_gone)
      *slot_gone = false;
   if (!companion_dock_find(l, pane, &side, &idx))
      return false;
   s = &l->slots[side][idx];
   for (m = 0; m < s->n; m++)
      if (s->members[m] == pane)
         break;
   for (k = m; k + 1 < s->n; k++)
      s->members[k] = s->members[k + 1];
   s->n--;
   if (s->raised > m || s->raised >= s->n)
      s->raised = s->raised > 0 ? s->raised - 1 : 0;
   if (s->n == 0)
   {
      for (k = idx; k + 1 < l->nslots[side]; k++)
         l->slots[side][k] = l->slots[side][k + 1];
      l->nslots[side]--;
      if (slot_gone)
         *slot_gone = true;
   }
   if (side_out)  *side_out  = side;
   if (index_out) *index_out = idx;
   return true;
}

void companion_dock_place(companion_dock_layout_t *l, enum companion_dock_id pane,
      enum companion_dock_area side, int index)
{
   int old_side = -1, old_idx = -1, k;
   bool slot_gone = false;
   companion_dock_slot_t *s;
   if ((unsigned)pane >= COMPANION_DOCK_COUNT || side >= COMPANION_DOCK_FLOAT)
      return;
   /* A slot of its own that sat before the target moves the target up
    * by one when it goes. */
   if (     dock_detach(l, pane, &old_side, &old_idx, &slot_gone)
         && slot_gone && old_side == (int)side && old_idx < index)
      index--;
   if (index < 0)
      index = 0;
   if (index > l->nslots[side])
      index = l->nslots[side];
   if (l->nslots[side] >= COMPANION_DOCK_COUNT)
      return;
   for (k = l->nslots[side]; k > index; k--)
      l->slots[side][k] = l->slots[side][k - 1];
   s = &l->slots[side][index];
   memset(s, 0, sizeof(*s));
   s->members[0] = (unsigned char)pane;
   s->n          = 1;
   l->nslots[side]++;
   l->area[pane] = (unsigned char)side;
}

void companion_dock_tabify(companion_dock_layout_t *l, enum companion_dock_id pane,
      enum companion_dock_id onto)
{
   enum companion_dock_area side;
   int idx;
   companion_dock_slot_t *s;
   if (pane == onto || (unsigned)pane >= COMPANION_DOCK_COUNT
         || !companion_dock_find(l, onto, &side, &idx))
      return;
   dock_detach(l, pane, NULL, NULL, NULL);
   /* The detach may have shifted onto's slot. */
   if (!companion_dock_find(l, onto, &side, &idx))
      return;
   s = &l->slots[side][idx];
   if (s->n >= COMPANION_DOCK_COUNT)
      return;
   s->members[s->n] = (unsigned char)pane;
   s->raised        = s->n;
   s->n++;
   l->area[pane]    = (unsigned char)side;
}

void companion_dock_float(companion_dock_layout_t *l, enum companion_dock_id pane,
      const companion_rect_t *rect)
{
   if ((unsigned)pane >= COMPANION_DOCK_COUNT)
      return;
   dock_detach(l, pane, NULL, NULL, NULL);
   l->area[pane]   = COMPANION_DOCK_FLOAT;
   if (rect)
      l->floats[pane] = *rect;
}

void companion_dock_set_shown(companion_dock_layout_t *l, enum companion_dock_id pane,
      bool shown)
{
   if ((unsigned)pane < COMPANION_DOCK_COUNT)
      l->shown[pane] = shown;
}

void companion_dock_raise(companion_dock_layout_t *l, enum companion_dock_id pane)
{
   enum companion_dock_area side;
   int idx, m;
   companion_dock_slot_t *s;
   if (!companion_dock_find(l, pane, &side, &idx))
      return;
   s = &l->slots[side][idx];
   for (m = 0; m < s->n; m++)
      if (s->members[m] == pane)
         s->raised = m;
}

enum companion_dock_id companion_dock_raised(const companion_dock_layout_t *l,
      enum companion_dock_id pane)
{
   enum companion_dock_area side;
   int idx, m;
   const companion_dock_slot_t *s;
   if (!companion_dock_find(l, pane, &side, &idx))
      return pane;
   s = &l->slots[side][idx];
   m = dock_slot_visible_member(l, s);
   return m < 0 ? pane : (enum companion_dock_id)s->members[m];
}

bool companion_dock_is_tabbed(const companion_dock_layout_t *l,
      enum companion_dock_id pane)
{
   enum companion_dock_area side;
   int idx;
   if (!companion_dock_find(l, pane, &side, &idx))
      return false;
   return l->slots[side][idx].n > 1;
}

/* --- Rows ------------------------------------------------------------- */

bool companion_dock_from_rows(struct settings *settings,
      companion_dock_layout_t *l)
{
   companion_dock_state_t st[COMPANION_DOCK_COUNT];
   bool have[COMPANION_DOCK_COUNT];
   int seq[COMPANION_DOCK_COUNT];
   int i, j, n = 0;
   bool any = false;

   companion_dock_default(l);
   if (!settings || !settings->bools.desktop_menu_save_dock_positions)
      return false;
   for (i = 0; i < COMPANION_DOCK_COUNT; i++)
   {
      size_t len;
      const char *row = companion_dock_row(settings, (enum companion_dock_id)i, &len);
      have[i] = companion_dock_row_parse(row, &st[i]);
      any    |= have[i];
   }
   if (!any)
      return false;

   /* Docked panes, by side, slot, then id: re-placed in that order each
    * lands after the one before it, which rebuilds the stacking. */
   for (i = 0; i < COMPANION_DOCK_COUNT; i++)
   {
      if (!have[i] || st[i].area == COMPANION_DOCK_FLOAT)
         continue;
      for (j = n; j > 0; j--)
      {
         int k = seq[j - 1];
         if (     st[k].area < st[i].area
               || (st[k].area == st[i].area && st[k].order <= st[i].order))
            break;
         seq[j] = k;
      }
      seq[j] = i;
      n++;
   }
   for (i = 0; i < n; i++)
      companion_dock_place(l, (enum companion_dock_id)seq[i],
            (enum companion_dock_area)st[seq[i]].area, COMPANION_DOCK_COUNT);
   /* Tabs: onto the named partner when it is docked on the same side. */
   for (i = 0; i < n; i++)
   {
      int p = seq[i];
      int t = -1;
      enum companion_dock_area ps, ts;
      int pi, ti;
      if (string_is_empty(st[p].tabbed_with))
         continue;
      for (j = 0; j < COMPANION_DOCK_COUNT; j++)
         if (string_is_equal(st[p].tabbed_with, companion_dock_key((enum companion_dock_id)j)))
            t = j;
      if (     t < 0 || t == p
            || !companion_dock_find(l, (enum companion_dock_id)t, &ts, &ti)
            || !companion_dock_find(l, (enum companion_dock_id)p, &ps, &pi)
            || ts != ps)
         continue;
      companion_dock_tabify(l, (enum companion_dock_id)p, (enum companion_dock_id)t);
   }
   /* A group no row raises shows its first member, as Qt's does. */
   for (i = 0; i < COMPANION_DOCK_SIDES; i++)
      for (j = 0; j < l->nslots[i]; j++)
         l->slots[i][j].raised = 0;
   /* Raised tabs, sizes, visibility. */
   for (i = 0; i < COMPANION_DOCK_COUNT; i++)
   {
      enum companion_dock_area side;
      int idx;
      if (!have[i])
         continue;
      l->shown[i] = st[i].shown;
      if (st[i].area == COMPANION_DOCK_FLOAT)
      {
         companion_rect_t r;
         r.x = st[i].x; r.y = st[i].y; r.w = st[i].width; r.h = st[i].height;
         companion_dock_float(l, (enum companion_dock_id)i, &r);
         continue;
      }
      if (!companion_dock_find(l, (enum companion_dock_id)i, &side, &idx))
         continue;
      if (dock_side_vertical(side))
      {
         if (st[i].width  > l->side_size[side])       l->side_size[side]       = st[i].width;
         if (st[i].height > l->slots[side][idx].size) l->slots[side][idx].size = st[i].height;
      }
      else
      {
         if (st[i].height > l->side_size[side])       l->side_size[side]       = st[i].height;
         if (st[i].width  > l->slots[side][idx].size) l->slots[side][idx].size = st[i].width;
      }
      if (st[i].raised)
         companion_dock_raise(l, (enum companion_dock_id)i);
   }
   return true;
}

void companion_dock_to_rows(struct settings *settings,
      const companion_dock_layout_t *l)
{
   int i, m;
   if (!settings || !settings->bools.desktop_menu_save_dock_positions)
      return;
   for (i = 0; i < COMPANION_DOCK_COUNT; i++)
   {
      companion_dock_state_t st;
      enum companion_dock_area side;
      int idx;
      size_t len;
      char *row = companion_dock_row(settings, (enum companion_dock_id)i, &len);
      memset(&st, 0, sizeof(st));
      st.shown = l->shown[i];
      if (!companion_dock_find(l, (enum companion_dock_id)i, &side, &idx))
      {
         st.area   = COMPANION_DOCK_FLOAT;
         st.x      = l->floats[i].x;
         st.y      = l->floats[i].y;
         st.width  = l->floats[i].w;
         st.height = l->floats[i].h;
      }
      else
      {
         const companion_dock_slot_t *s = &l->slots[side][idx];
         int vis      = dock_slot_visible_member(l, s);
         bool carries = (vis >= 0 && s->members[vis] == i);
         int first    = COMPANION_DOCK_COUNT;
         st.area  = side;
         st.order = idx;
         /* The laid-out member carries the slot's size; the tab
          * partner is the earliest member of the group. */
         if (carries)
         {
            if (dock_side_vertical(side))
            {
               st.width  = l->side_size[side];
               st.height = s->size;
            }
            else
            {
               st.width  = s->size;
               st.height = l->side_size[side];
            }
         }
         if (s->n > 1)
         {
            for (m = 0; m < s->n; m++)
               if (s->members[m] < first)
                  first = s->members[m];
            if (first < i)
               strlcpy(st.tabbed_with, companion_dock_key((enum companion_dock_id)first),
                     sizeof(st.tabbed_with));
            st.raised = (s->raised >= 0 && s->raised < s->n && s->members[s->raised] == i);
         }
      }
      companion_dock_row_format(row, len, &st);
   }
}

/* --- Geometry --------------------------------------------------------- */

/* Shown slots of @side, and whether the side shows at all. */
static int dock_side_visible_slots(const companion_dock_layout_t *l, int side)
{
   int i, n = 0;
   for (i = 0; i < l->nslots[side]; i++)
      if (dock_slot_shown(l, &l->slots[side][i]))
         n++;
   return n;
}

void companion_dock_layout(companion_dock_layout_t *l,
      const companion_dock_metrics_t *m, const companion_rect_t *client,
      companion_dock_geometry_t *g)
{
   int side_ext[COMPANION_DOCK_SIDES];   /* width of L/R, height of T/B, 0 when not shown */
   companion_rect_t side_rect[COMPANION_DOCK_SIDES];
   int side, i, k;
   int min_side = m->min_pane;

   memset(g, 0, sizeof(*g));

   for (side = 0; side < COMPANION_DOCK_SIDES; side++)
   {
      side_ext[side] = 0;
      if (dock_side_visible_slots(l, side) == 0)
         continue;
      side_ext[side] = l->side_size[side] > 0 ? l->side_size[side]
         : (dock_side_vertical(side) ? m->def_side : m->def_bar);
      if (side_ext[side] < min_side)
         side_ext[side] = min_side;
   }
   /* The content keeps at least min_pane each way: shrink the sides
    * in proportion when they would not leave it. */
   {
      int gaps = (side_ext[COMPANION_DOCK_LEFT] ? m->gap : 0)
         + (side_ext[COMPANION_DOCK_RIGHT] ? m->gap : 0);
      int avail = client->w - m->min_pane - gaps;
      int sum   = side_ext[COMPANION_DOCK_LEFT] + side_ext[COMPANION_DOCK_RIGHT];
      if (avail < 0) avail = 0;
      if (sum > avail && sum > 0)
      {
         side_ext[COMPANION_DOCK_LEFT]  = (int)((long)side_ext[COMPANION_DOCK_LEFT]  * avail / sum);
         side_ext[COMPANION_DOCK_RIGHT] = (int)((long)side_ext[COMPANION_DOCK_RIGHT] * avail / sum);
      }
      gaps  = (side_ext[COMPANION_DOCK_TOP] ? m->gap : 0)
         + (side_ext[COMPANION_DOCK_BOTTOM] ? m->gap : 0);
      avail = client->h - m->min_pane - gaps;
      sum   = side_ext[COMPANION_DOCK_TOP] + side_ext[COMPANION_DOCK_BOTTOM];
      if (avail < 0) avail = 0;
      if (sum > avail && sum > 0)
      {
         side_ext[COMPANION_DOCK_TOP]    = (int)((long)side_ext[COMPANION_DOCK_TOP]    * avail / sum);
         side_ext[COMPANION_DOCK_BOTTOM] = (int)((long)side_ext[COMPANION_DOCK_BOTTOM] * avail / sum);
      }
   }
   for (side = 0; side < COMPANION_DOCK_SIDES; side++)
      if (side_ext[side])
         l->side_size[side] = side_ext[side];

   /* Side rectangles: top and bottom span the width, left and right
    * sit between them. */
   {
      int top_h    = side_ext[COMPANION_DOCK_TOP];
      int bot_h    = side_ext[COMPANION_DOCK_BOTTOM];
      int mid_y    = client->y + (top_h ? top_h + m->gap : 0);
      int mid_h    = client->h - (top_h ? top_h + m->gap : 0) - (bot_h ? bot_h + m->gap : 0);
      int left_w   = side_ext[COMPANION_DOCK_LEFT];
      int right_w  = side_ext[COMPANION_DOCK_RIGHT];
      if (mid_h < 0) mid_h = 0;
      side_rect[COMPANION_DOCK_TOP].x    = client->x;
      side_rect[COMPANION_DOCK_TOP].y    = client->y;
      side_rect[COMPANION_DOCK_TOP].w    = client->w;
      side_rect[COMPANION_DOCK_TOP].h    = top_h;
      side_rect[COMPANION_DOCK_BOTTOM].x = client->x;
      side_rect[COMPANION_DOCK_BOTTOM].y = client->y + client->h - bot_h;
      side_rect[COMPANION_DOCK_BOTTOM].w = client->w;
      side_rect[COMPANION_DOCK_BOTTOM].h = bot_h;
      side_rect[COMPANION_DOCK_LEFT].x   = client->x;
      side_rect[COMPANION_DOCK_LEFT].y   = mid_y;
      side_rect[COMPANION_DOCK_LEFT].w   = left_w;
      side_rect[COMPANION_DOCK_LEFT].h   = mid_h;
      side_rect[COMPANION_DOCK_RIGHT].x  = client->x + client->w - right_w;
      side_rect[COMPANION_DOCK_RIGHT].y  = mid_y;
      side_rect[COMPANION_DOCK_RIGHT].w  = right_w;
      side_rect[COMPANION_DOCK_RIGHT].h  = mid_h;
      g->content.x = client->x + (left_w ? left_w + m->gap : 0);
      g->content.y = mid_y;
      g->content.w = client->w - (left_w ? left_w + m->gap : 0) - (right_w ? right_w + m->gap : 0);
      g->content.h = mid_h;
      if (g->content.w < 0) g->content.w = 0;
      /* Side-edge gaps. */
      if (left_w)
      {
         companion_dock_gap_t *gp = &g->gaps[g->ngaps++];
         gp->side = COMPANION_DOCK_LEFT; gp->index = -1;
         gp->rect.x = client->x + left_w; gp->rect.y = mid_y;
         gp->rect.w = m->gap; gp->rect.h = mid_h;
      }
      if (right_w)
      {
         companion_dock_gap_t *gp = &g->gaps[g->ngaps++];
         gp->side = COMPANION_DOCK_RIGHT; gp->index = -1;
         gp->rect.x = client->x + client->w - right_w - m->gap; gp->rect.y = mid_y;
         gp->rect.w = m->gap; gp->rect.h = mid_h;
      }
      if (top_h)
      {
         companion_dock_gap_t *gp = &g->gaps[g->ngaps++];
         gp->side = COMPANION_DOCK_TOP; gp->index = -1;
         gp->rect.x = client->x; gp->rect.y = client->y + top_h;
         gp->rect.w = client->w; gp->rect.h = m->gap;
      }
      if (bot_h)
      {
         companion_dock_gap_t *gp = &g->gaps[g->ngaps++];
         gp->side = COMPANION_DOCK_BOTTOM; gp->index = -1;
         gp->rect.x = client->x; gp->rect.y = client->y + client->h - bot_h - m->gap;
         gp->rect.w = client->w; gp->rect.h = m->gap;
      }
   }

   /* Slots along each side. */
   for (side = 0; side < COMPANION_DOCK_SIDES; side++)
   {
      int vis[COMPANION_DOCK_COUNT];
      int member[COMPANION_DOCK_COUNT];
      int size[COMPANION_DOCK_COUNT];
      int minsz[COMPANION_DOCK_COUNT];
      int nvis = 0, along, avail, sum, pos;
      bool vertical = dock_side_vertical(side);
      if (!side_ext[side])
         continue;
      along = vertical ? side_rect[side].h : side_rect[side].w;
      for (i = 0; i < l->nslots[side]; i++)
      {
         const companion_dock_slot_t *s = &l->slots[side][i];
         int mm = dock_slot_visible_member(l, s);
         if (mm < 0)
            continue;
         vis[nvis]    = i;
         member[nvis] = mm;
         size[nvis]   = s->size > 0 ? s->size : m->def_slot;
         minsz[nvis]  = m->min_pane + m->strip_h
            + (dock_slot_shown(l, s) > 1 ? m->tab_h : 0);
         nvis++;
      }
      if (!nvis)
         continue;
      avail = along - (nvis - 1) * m->gap;
      if (avail < 0) avail = 0;
      sum = 0;
      for (k = 0; k < nvis; k++)
         sum += size[k];
      /* Scale the preferences to the room, then keep every slot at
       * its minimum, the last one taking up the difference. */
      if (sum > 0)
         for (k = 0; k < nvis; k++)
            size[k] = (int)((long)size[k] * avail / sum);
      sum = 0;
      for (k = 0; k < nvis; k++)
      {
         if (size[k] < minsz[k])
            size[k] = minsz[k];
         sum += size[k];
      }
      size[nvis - 1] += avail - sum;
      if (size[nvis - 1] < minsz[nvis - 1])
         size[nvis - 1] = minsz[nvis - 1];
      pos = vertical ? side_rect[side].y : side_rect[side].x;
      for (k = 0; k < nvis; k++)
      {
         companion_dock_slot_t *s = &l->slots[side][vis[k]];
         int pane   = s->members[member[k]];
         int tab_h  = dock_slot_shown(l, s) > 1 ? m->tab_h : 0;
         int ext    = size[k];
         companion_rect_t slot;
         s->size = ext;
         if (vertical)
         {
            slot.x = side_rect[side].x; slot.y = pos;
            slot.w = side_rect[side].w; slot.h = ext;
         }
         else
         {
            slot.x = pos; slot.y = side_rect[side].y;
            slot.w = ext; slot.h = side_rect[side].h;
         }
         g->strip[pane]    = slot;
         g->strip[pane].h  = m->strip_h;
         g->tabbar[pane]   = slot;
         g->tabbar[pane].y = slot.y + slot.h - tab_h;
         g->tabbar[pane].h = tab_h;
         g->pane[pane]     = slot;
         g->pane[pane].y   = slot.y + m->strip_h;
         g->pane[pane].h   = slot.h - m->strip_h - tab_h;
         if (g->pane[pane].h < 0) g->pane[pane].h = 0;
         g->laid_out[pane] = true;
         pos += ext;
         if (k + 1 < nvis)
         {
            companion_dock_gap_t *gp = &g->gaps[g->ngaps++];
            gp->side  = side;
            gp->index = vis[k];
            if (vertical)
            {
               gp->rect.x = slot.x; gp->rect.y = pos;
               gp->rect.w = slot.w; gp->rect.h = m->gap;
            }
            else
            {
               gp->rect.x = pos; gp->rect.y = slot.y;
               gp->rect.w = m->gap; gp->rect.h = slot.h;
            }
            pos += m->gap;
         }
      }
   }
}

/* The tab rects of @pane's group along its tab bar, in member order,
 * shown members only; returns how many. */
static int dock_tab_rects(const companion_dock_layout_t *l,
      const companion_dock_geometry_t *g, enum companion_dock_id pane,
      companion_rect_t *rects, int *ids)
{
   enum companion_dock_area side;
   int idx, i, n = 0, shown, w, x;
   const companion_dock_slot_t *s;
   const companion_rect_t *bar;
   if (!companion_dock_find(l, pane, &side, &idx))
      return 0;
   s     = &l->slots[side][idx];
   shown = dock_slot_shown(l, s);
   bar   = &g->tabbar[pane];
   if (shown < 2 || bar->h <= 0)
      return 0;
   w = bar->w / shown;
   x = bar->x;
   for (i = 0; i < s->n; i++)
   {
      if (!l->shown[s->members[i]])
         continue;
      rects[n].x = x; rects[n].y = bar->y;
      rects[n].w = (n == shown - 1) ? bar->x + bar->w - x : w;
      rects[n].h = bar->h;
      ids[n]     = s->members[i];
      x         += w;
      n++;
   }
   return n;
}

void companion_dock_hit_test(const companion_dock_layout_t *l,
      const companion_dock_geometry_t *g, const companion_dock_metrics_t *m,
      int x, int y, companion_dock_hit_t *hit)
{
   int i, k;
   (void)m;
   memset(hit, 0, sizeof(*hit));
   for (i = 0; i < COMPANION_DOCK_COUNT; i++)
   {
      companion_rect_t tabs[COMPANION_DOCK_COUNT];
      int ids[COMPANION_DOCK_COUNT], n;
      if (!g->laid_out[i])
         continue;
      if (dock_rect_has(&g->strip[i], x, y))
      {
         hit->kind = COMPANION_DOCK_HIT_STRIP;
         hit->pane = (enum companion_dock_id)i;
         return;
      }
      n = dock_tab_rects(l, g, (enum companion_dock_id)i, tabs, ids);
      for (k = 0; k < n; k++)
         if (dock_rect_has(&tabs[k], x, y))
         {
            hit->kind = COMPANION_DOCK_HIT_TAB;
            hit->pane = (enum companion_dock_id)ids[k];
            return;
         }
   }
   for (i = 0; i < g->ngaps; i++)
      if (dock_rect_has(&g->gaps[i].rect, x, y))
      {
         hit->kind = COMPANION_DOCK_HIT_GAP;
         hit->gap  = i;
         return;
      }
}

void companion_dock_drag_gap(companion_dock_layout_t *l,
      const companion_dock_geometry_t *g, const companion_dock_metrics_t *m,
      const companion_rect_t *client, int gap, int pos)
{
   const companion_dock_gap_t *gp;
   int side;
   if (gap < 0 || gap >= g->ngaps)
      return;
   gp   = &g->gaps[gap];
   side = gp->side;
   if (gp->index < 0)
   {
      /* The side's edge. */
      int ext;
      switch (side)
      {
         case COMPANION_DOCK_LEFT:   ext = pos - client->x; break;
         case COMPANION_DOCK_RIGHT:  ext = client->x + client->w - pos - m->gap; break;
         case COMPANION_DOCK_TOP:    ext = pos - client->y; break;
         default:                    ext = client->y + client->h - pos - m->gap; break;
      }
      if (ext < m->min_pane)
         ext = m->min_pane;
      l->side_size[side] = ext;
      return;
   }
   /* Between two slots: what one gains the other loses. */
   {
      bool vertical = dock_side_vertical(side);
      int a = gp->index, b = -1, i;
      int total, start, na, mina, minb;
      companion_dock_slot_t *sa, *sb;
      for (i = a + 1; i < l->nslots[side]; i++)
         if (dock_slot_shown(l, &l->slots[side][i]))
         {
            b = i;
            break;
         }
      if (b < 0)
         return;
      sa    = &l->slots[side][a];
      sb    = &l->slots[side][b];
      total = sa->size + sb->size;
      start = vertical ? gp->rect.y - sa->size : gp->rect.x - sa->size;
      mina  = m->min_pane + m->strip_h + (dock_slot_shown(l, sa) > 1 ? m->tab_h : 0);
      minb  = m->min_pane + m->strip_h + (dock_slot_shown(l, sb) > 1 ? m->tab_h : 0);
      na    = pos - start - m->gap / 2;
      if (na < mina)         na = mina;
      if (na > total - minb) na = total - minb;
      if (na < mina)         na = mina; /* both minimums cannot fit: keep a */
      sa->size = na;
      sb->size = total - na;
      if (sb->size < 0) sb->size = 0;
   }
}

void companion_dock_drop_target(const companion_dock_layout_t *l,
      const companion_dock_geometry_t *g, const companion_dock_metrics_t *m,
      const companion_rect_t *client, enum companion_dock_id pane,
      int x, int y, companion_dock_drop_t *drop)
{
   int i;
   int band = m->strip_h * 2;
   memset(drop, 0, sizeof(*drop));
   drop->kind = COMPANION_DOCK_DROP_FLOAT;

   /* Over a docked pane: thirds along its side. */
   for (i = 0; i < COMPANION_DOCK_COUNT; i++)
   {
      companion_rect_t r;
      enum companion_dock_area side;
      int idx;
      bool vertical;
      if (!g->laid_out[i] || i == (int)pane)
         continue;
      r   = g->strip[i];
      r.h = g->pane[i].y + g->pane[i].h + g->tabbar[i].h - r.y;
      if (!dock_rect_has(&r, x, y))
         continue;
      if (!companion_dock_find(l, (enum companion_dock_id)i, &side, &idx))
         continue;
      vertical = dock_side_vertical(side);
      drop->side = side;
      if (vertical ? (y < r.y + r.h / 3) : (x < r.x + r.w / 3))
      {
         drop->kind  = COMPANION_DOCK_DROP_SLOT;
         drop->index = idx;
         drop->indicator = r;
         if (vertical) drop->indicator.h = r.h / 2;
         else          drop->indicator.w = r.w / 2;
      }
      else if (vertical ? (y >= r.y + r.h - r.h / 3) : (x >= r.x + r.w - r.w / 3))
      {
         drop->kind  = COMPANION_DOCK_DROP_SLOT;
         drop->index = idx + 1;
         drop->indicator = r;
         if (vertical) { drop->indicator.y += r.h / 2; drop->indicator.h = r.h - r.h / 2; }
         else          { drop->indicator.x += r.w / 2; drop->indicator.w = r.w - r.w / 2; }
      }
      else
      {
         drop->kind      = COMPANION_DOCK_DROP_TAB;
         drop->onto      = (enum companion_dock_id)i;
         drop->indicator = r;
      }
      return;
   }
   /* Along an edge of the client: the end of that side. */
   if (dock_rect_has(client, x, y))
   {
      int side = -1;
      if (x < client->x + band)                       side = COMPANION_DOCK_LEFT;
      else if (x >= client->x + client->w - band)     side = COMPANION_DOCK_RIGHT;
      else if (y < client->y + band)                  side = COMPANION_DOCK_TOP;
      else if (y >= client->y + client->h - band)     side = COMPANION_DOCK_BOTTOM;
      if (side >= 0)
      {
         drop->kind  = COMPANION_DOCK_DROP_SLOT;
         drop->side  = (enum companion_dock_area)side;
         drop->index = COMPANION_DOCK_COUNT;
         drop->indicator = *client;
         switch (side)
         {
            case COMPANION_DOCK_LEFT:   drop->indicator.w = band * 3; break;
            case COMPANION_DOCK_RIGHT:  drop->indicator.x += client->w - band * 3; drop->indicator.w = band * 3; break;
            case COMPANION_DOCK_TOP:    drop->indicator.h = band * 3; break;
            default:                    drop->indicator.y += client->h - band * 3; drop->indicator.h = band * 3; break;
         }
      }
   }
}

void companion_dock_apply_drop(companion_dock_layout_t *l,
      enum companion_dock_id pane, const companion_dock_drop_t *drop,
      const companion_rect_t *rect)
{
   switch (drop->kind)
   {
      case COMPANION_DOCK_DROP_SLOT:
         companion_dock_place(l, pane, drop->side, drop->index);
         break;
      case COMPANION_DOCK_DROP_TAB:
         companion_dock_tabify(l, pane, drop->onto);
         break;
      default:
         companion_dock_float(l, pane, rect);
         break;
   }
   l->shown[pane] = true;
}
