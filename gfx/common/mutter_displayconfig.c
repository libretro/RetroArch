/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2026 - The RetroArch team
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mutter_displayconfig.h"

#ifdef RARCH_HAVE_MUTTER_DC

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <compat/strl.h>
#include <rthreads/rthreads.h>

#include "../../verbosity.h"

/* libdbus, for the worker's calls only; set before its first. */
static const rdbus_t *mdc_rd;

#define MDC_BUS_NAME   "org.gnome.Mutter.DisplayConfig"
#define MDC_PATH       "/org/gnome/Mutter/DisplayConfig"
#define MDC_IFACE      "org.gnome.Mutter.DisplayConfig"
#define MDC_TIMEOUT_MS 2000

/* ApplyMonitorsConfig methods */
#define MDC_METHOD_TEMPORARY 1

/* Layout modes: logical monitors sized by mode / scale, or by mode */
#define MDC_LAYOUT_LOGICAL  1
#define MDC_LAYOUT_PHYSICAL 2

#define MDC_MAX_SCALES 16
#define MDC_MAX_LM_MON 8

typedef struct
{
   char     id[128];
   int      w, h;
   double   refresh;
   double   scales[MDC_MAX_SCALES];
   int      nscales;
   bool     current;
   bool     interlaced;
} mdc_mode_t;

typedef struct
{
   char        connector[64];
   mdc_mode_t *modes;
   int         nmodes;
   unsigned    color_mode;
   unsigned    rgb_range;
   bool        underscanning;
   bool        has_underscanning;
   bool        has_color_mode;
   bool        has_rgb_range;
} mdc_monitor_t;

typedef struct
{
   int      x, y;
   double   scale;
   unsigned transform;
   int      mon[MDC_MAX_LM_MON];  /* indices into state->mon */
   int      nmon;
   bool     primary;
} mdc_logical_t;

typedef struct
{
   unsigned       serial;
   unsigned       layout_mode;
   mdc_monitor_t *mon;
   int            nmon;
   mdc_logical_t *lm;
   int            nlm;
   bool           has_layout_mode;
   bool           supports_layout_change;
} mdc_state_t;

/* ------------------------------------------------------------------
 * Connection
 * ------------------------------------------------------------------ */

/* The session bus, never an autolaunched one: with no address in the
 * environment and no $XDG_RUNTIME_DIR/bus socket, libdbus would spawn
 * a bus of its own, which is nothing Mutter is on. */
static rdbus_connection_t *mdc_connect(void)
{
   rdbus_error_t err;
   rdbus_connection_t *conn;
   const char *addr = getenv("DBUS_SESSION_BUS_ADDRESS");

   if (!addr || !*addr)
   {
      char path[512];
      const char *rt = getenv("XDG_RUNTIME_DIR");
      if (!rt || !*rt)
         return NULL;
      snprintf(path, sizeof(path), "%s/bus", rt);
      if (access(path, F_OK) != 0)
         return NULL;
   }

   mdc_rd->error_init(&err);
   conn = mdc_rd->bus_get_private(RDBUS_BUS_SESSION, &err);
   if (mdc_rd->error_is_set(&err))
      mdc_rd->error_free(&err);
   if (conn)
      mdc_rd->connection_set_exit_on_disconnect(conn, false);
   return conn;
}

static void mdc_disconnect(rdbus_connection_t *conn)
{
   if (!conn)
      return;
   mdc_rd->connection_close(conn);
   mdc_rd->connection_unref(conn);
}

static bool mdc_has_owner(rdbus_connection_t *conn)
{
   rdbus_error_t err;
   bool has;
   mdc_rd->error_init(&err);
   has = mdc_rd->bus_name_has_owner(conn, MDC_BUS_NAME, &err) ? true : false;
   if (mdc_rd->error_is_set(&err))
   {
      mdc_rd->error_free(&err);
      return false;
   }
   return has;
}


/* ------------------------------------------------------------------
 * GetCurrentState
 * ------------------------------------------------------------------ */

static void mdc_state_free(mdc_state_t *st)
{
   int i;
   if (!st)
      return;
   for (i = 0; i < st->nmon; i++)
      free(st->mon[i].modes);
   free(st->mon);
   free(st->lm);
   free(st);
}

static int mdc_count(rdbus_iter_t *array)
{
   int n = 0;
   rdbus_iter_t it;
   mdc_rd->message_iter_recurse(array, &it);
   while (mdc_rd->message_iter_get_arg_type(&it) != RDBUS_TYPE_INVALID)
   {
      n++;
      mdc_rd->message_iter_next(&it);
   }
   return n;
}

/* One a{sv} entry's variant, if it holds a basic value of type t */
static bool mdc_variant_basic(rdbus_iter_t *variant, int t, void *out)
{
   rdbus_iter_t v;
   mdc_rd->message_iter_recurse(variant, &v);
   if (mdc_rd->message_iter_get_arg_type(&v) != t)
      return false;
   mdc_rd->message_iter_get_basic(&v, out);
   return true;
}

/* Walks an a{sv}; cb is handed each key and its variant */
typedef void (*mdc_prop_cb)(void *ud, const char *key,
      rdbus_iter_t *variant);

static bool mdc_props(rdbus_iter_t *array, mdc_prop_cb cb, void *ud)
{
   rdbus_iter_t it;
   if (mdc_rd->message_iter_get_arg_type(array) != RDBUS_TYPE_ARRAY)
      return false;
   mdc_rd->message_iter_recurse(array, &it);
   while (mdc_rd->message_iter_get_arg_type(&it) == RDBUS_TYPE_DICT_ENTRY)
   {
      rdbus_iter_t e;
      const char *key = NULL;
      mdc_rd->message_iter_recurse(&it, &e);
      if (mdc_rd->message_iter_get_arg_type(&e) == RDBUS_TYPE_STRING)
      {
         mdc_rd->message_iter_get_basic(&e, &key);
         mdc_rd->message_iter_next(&e);
         if (mdc_rd->message_iter_get_arg_type(&e) == RDBUS_TYPE_VARIANT)
            cb(ud, key, &e);
      }
      mdc_rd->message_iter_next(&it);
   }
   return true;
}

static void mdc_mode_prop(void *ud, const char *key, rdbus_iter_t *v)
{
   mdc_mode_t *m = (mdc_mode_t*)ud;
   rdbus_bool_t b = 0;
   if (!strcmp(key, "is-current") && mdc_variant_basic(v, RDBUS_TYPE_BOOLEAN, &b))
      m->current    = b ? true : false;
   else if (!strcmp(key, "is-interlaced") && mdc_variant_basic(v, RDBUS_TYPE_BOOLEAN, &b))
      m->interlaced = b ? true : false;
}

static void mdc_monitor_prop(void *ud, const char *key, rdbus_iter_t *v)
{
   mdc_monitor_t *mon = (mdc_monitor_t*)ud;
   rdbus_bool_t b      = 0;
   uint32_t u    = 0;
   if (!strcmp(key, "is-underscanning") && mdc_variant_basic(v, RDBUS_TYPE_BOOLEAN, &b))
   {
      mon->underscanning     = b ? true : false;
      mon->has_underscanning = true;
   }
   else if (!strcmp(key, "color-mode") && mdc_variant_basic(v, RDBUS_TYPE_UINT32, &u))
   {
      mon->color_mode     = u;
      mon->has_color_mode = true;
   }
   else if (!strcmp(key, "rgb-range") && mdc_variant_basic(v, RDBUS_TYPE_UINT32, &u))
   {
      mon->rgb_range     = u;
      mon->has_rgb_range = true;
   }
}

static void mdc_global_prop(void *ud, const char *key, rdbus_iter_t *v)
{
   mdc_state_t *st = (mdc_state_t*)ud;
   rdbus_bool_t b   = 0;
   uint32_t u = 0;
   if (!strcmp(key, "layout-mode") && mdc_variant_basic(v, RDBUS_TYPE_UINT32, &u))
   {
      st->layout_mode     = u;
      st->has_layout_mode = true;
   }
   else if (!strcmp(key, "supports-changing-layout-mode")
         && mdc_variant_basic(v, RDBUS_TYPE_BOOLEAN, &b))
      st->supports_layout_change = b ? true : false;
}

/* (ssss): the connector is the first string */
static bool mdc_spec_connector(rdbus_iter_t *spec, char *s, size_t len)
{
   rdbus_iter_t f;
   const char *c = NULL;
   if (mdc_rd->message_iter_get_arg_type(spec) != RDBUS_TYPE_STRUCT)
      return false;
   mdc_rd->message_iter_recurse(spec, &f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_STRING)
      return false;
   mdc_rd->message_iter_get_basic(&f, &c);
   strlcpy(s, c ? c : "", len);
   return true;
}

/* (siiddada{sv}) */
static bool mdc_parse_mode(rdbus_iter_t *s, mdc_mode_t *m)
{
   rdbus_iter_t f, sc;
   const char *id    = NULL;
   int32_t w    = 0, h = 0;
   double refresh    = 0.0, pref = 0.0;

   mdc_rd->message_iter_recurse(s, &f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_STRING)
      return false;
   mdc_rd->message_iter_get_basic(&f, &id);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_INT32)
      return false;
   mdc_rd->message_iter_get_basic(&f, &w);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_INT32)
      return false;
   mdc_rd->message_iter_get_basic(&f, &h);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_DOUBLE)
      return false;
   mdc_rd->message_iter_get_basic(&f, &refresh);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_DOUBLE)
      return false;
   mdc_rd->message_iter_get_basic(&f, &pref);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_ARRAY)
      return false;

   memset(m, 0, sizeof(*m));
   strlcpy(m->id, id ? id : "", sizeof(m->id));
   m->w       = (int)w;
   m->h       = (int)h;
   m->refresh = refresh;

   mdc_rd->message_iter_recurse(&f, &sc);
   while (mdc_rd->message_iter_get_arg_type(&sc) == RDBUS_TYPE_DOUBLE
         && m->nscales < MDC_MAX_SCALES)
   {
      mdc_rd->message_iter_get_basic(&sc, &m->scales[m->nscales++]);
      mdc_rd->message_iter_next(&sc);
   }
   mdc_rd->message_iter_next(&f);
   mdc_props(&f, mdc_mode_prop, m);
   return m->id[0] && m->w > 0 && m->h > 0;
}

/* ((ssss)a(siiddada{sv})a{sv}) */
static bool mdc_parse_monitor(rdbus_iter_t *s, mdc_monitor_t *mon)
{
   rdbus_iter_t f, modes;
   int n;

   memset(mon, 0, sizeof(*mon));
   mdc_rd->message_iter_recurse(s, &f);
   if (!mdc_spec_connector(&f, mon->connector, sizeof(mon->connector)))
      return false;
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_ARRAY)
      return false;

   if ((n = mdc_count(&f)) > 0
         && !(mon->modes = (mdc_mode_t*)calloc(n, sizeof(*mon->modes))))
      return false;
   mdc_rd->message_iter_recurse(&f, &modes);
   while (mdc_rd->message_iter_get_arg_type(&modes) == RDBUS_TYPE_STRUCT
         && mon->nmodes < n)
   {
      if (mdc_parse_mode(&modes, &mon->modes[mon->nmodes]))
         mon->nmodes++;
      mdc_rd->message_iter_next(&modes);
   }
   mdc_rd->message_iter_next(&f);
   mdc_props(&f, mdc_monitor_prop, mon);
   return true;
}

/* (iiduba(ssss)a{sv}) */
static bool mdc_parse_logical(rdbus_iter_t *s, const mdc_state_t *st,
      mdc_logical_t *lm)
{
   rdbus_iter_t f, specs;
   int32_t x = 0, y = 0;
   uint32_t transform = 0;
   rdbus_bool_t primary = 0;
   double scale = 1.0;

   memset(lm, 0, sizeof(*lm));
   mdc_rd->message_iter_recurse(s, &f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_INT32)
      return false;
   mdc_rd->message_iter_get_basic(&f, &x);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_INT32)
      return false;
   mdc_rd->message_iter_get_basic(&f, &y);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_DOUBLE)
      return false;
   mdc_rd->message_iter_get_basic(&f, &scale);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_UINT32)
      return false;
   mdc_rd->message_iter_get_basic(&f, &transform);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_BOOLEAN)
      return false;
   mdc_rd->message_iter_get_basic(&f, &primary);
   mdc_rd->message_iter_next(&f);
   if (mdc_rd->message_iter_get_arg_type(&f) != RDBUS_TYPE_ARRAY)
      return false;

   lm->x         = (int)x;
   lm->y         = (int)y;
   lm->scale     = scale > 0.0 ? scale : 1.0;
   lm->transform = transform;
   lm->primary   = primary ? true : false;

   mdc_rd->message_iter_recurse(&f, &specs);
   while (mdc_rd->message_iter_get_arg_type(&specs) == RDBUS_TYPE_STRUCT)
   {
      char c[64];
      if (mdc_spec_connector(&specs, c, sizeof(c)))
      {
         int m;
         for (m = 0; m < st->nmon && lm->nmon < MDC_MAX_LM_MON; m++)
            if (!strcmp(st->mon[m].connector, c))
            {
               lm->mon[lm->nmon++] = m;
               break;
            }
      }
      mdc_rd->message_iter_next(&specs);
   }
   return lm->nmon > 0;
}

static mdc_state_t *mdc_get_state(rdbus_connection_t *conn)
{
   rdbus_iter_t it, arr;
   rdbus_error_t err;
   int n;
   uint32_t serial = 0;
   rdbus_message_t *reply   = NULL;
   mdc_state_t *st      = NULL;
   rdbus_message_t *msg     = mdc_rd->message_new_method_call(MDC_BUS_NAME,
         MDC_PATH, MDC_IFACE, "GetCurrentState");

   if (!msg)
      return NULL;
   mdc_rd->error_init(&err);
   reply = mdc_rd->connection_send_with_reply_and_block(conn, msg,
         MDC_TIMEOUT_MS, &err);
   mdc_rd->message_unref(msg);
   if (mdc_rd->error_is_set(&err))
   {
      RARCH_WARN("[Mutter] GetCurrentState: %s.\n", err.message);
      mdc_rd->error_free(&err);
      return NULL;
   }
   if (!reply)
      return NULL;

   if (     !mdc_rd->message_iter_init(reply, &it)
         || mdc_rd->message_iter_get_arg_type(&it) != RDBUS_TYPE_UINT32
         || !(st = (mdc_state_t*)calloc(1, sizeof(*st))))
      goto fail;
   mdc_rd->message_iter_get_basic(&it, &serial);
   st->serial      = serial;
   st->layout_mode = MDC_LAYOUT_LOGICAL;

   /* monitors */
   mdc_rd->message_iter_next(&it);
   if (mdc_rd->message_iter_get_arg_type(&it) != RDBUS_TYPE_ARRAY)
      goto fail;
   if ((n = mdc_count(&it)) > 0
         && !(st->mon = (mdc_monitor_t*)calloc(n, sizeof(*st->mon))))
      goto fail;
   mdc_rd->message_iter_recurse(&it, &arr);
   while (mdc_rd->message_iter_get_arg_type(&arr) == RDBUS_TYPE_STRUCT
         && st->nmon < n)
   {
      if (mdc_parse_monitor(&arr, &st->mon[st->nmon]))
         st->nmon++;
      else
         free(st->mon[st->nmon].modes);
      mdc_rd->message_iter_next(&arr);
   }

   /* logical monitors */
   mdc_rd->message_iter_next(&it);
   if (mdc_rd->message_iter_get_arg_type(&it) != RDBUS_TYPE_ARRAY)
      goto fail;
   if ((n = mdc_count(&it)) > 0
         && !(st->lm = (mdc_logical_t*)calloc(n, sizeof(*st->lm))))
      goto fail;
   mdc_rd->message_iter_recurse(&it, &arr);
   while (mdc_rd->message_iter_get_arg_type(&arr) == RDBUS_TYPE_STRUCT
         && st->nlm < n)
   {
      if (mdc_parse_logical(&arr, st, &st->lm[st->nlm]))
         st->nlm++;
      mdc_rd->message_iter_next(&arr);
   }

   /* properties */
   mdc_rd->message_iter_next(&it);
   mdc_props(&it, mdc_global_prop, st);

   mdc_rd->message_unref(reply);
   if (!st->nlm)
   {
      mdc_state_free(st);
      return NULL;
   }
   return st;

fail:
   RARCH_WARN("[Mutter] GetCurrentState reply not understood.\n");
   mdc_rd->message_unref(reply);
   mdc_state_free(st);
   return NULL;
}

/* ------------------------------------------------------------------
 * Head selection and geometry
 * ------------------------------------------------------------------ */

static const mdc_mode_t *mdc_current_mode(const mdc_monitor_t *mon)
{
   int i;
   for (i = 0; i < mon->nmodes; i++)
      if (mon->modes[i].current)
         return &mon->modes[i];
   return NULL;
}

static int mdc_round(double v)
{
   return (int)(v + 0.5);
}

/* A logical monitor's extent in the layout on the given mode */
static void mdc_logical_size(const mdc_state_t *st, const mdc_logical_t *lm,
      const mdc_mode_t *mode, double scale, int *w, int *h)
{
   int mw = mode ? mode->w : 0;
   int mh = mode ? mode->h : 0;
   if (lm->transform & 1)
   {
      int t = mw;
      mw    = mh;
      mh    = t;
   }
   if (st->layout_mode != MDC_LAYOUT_PHYSICAL && scale > 0.0)
   {
      mw = mdc_round(mw / scale);
      mh = mdc_round(mh / scale);
   }
   *w = mw;
   *h = mh;
}

static int mdc_pick_logical(const mdc_state_t *st,
      const mutter_dc_target_t *t)
{
   int i, j;
   if (t && t->connector && *t->connector)
      for (i = 0; i < st->nlm; i++)
         for (j = 0; j < st->lm[i].nmon; j++)
            if (!strcmp(st->mon[st->lm[i].mon[j]].connector, t->connector))
               return i;
   if (t && t->have_point)
      for (i = 0; i < st->nlm; i++)
      {
         int w, h;
         const mdc_logical_t *lm = &st->lm[i];
         mdc_logical_size(st, lm, mdc_current_mode(&st->mon[lm->mon[0]]),
               lm->scale, &w, &h);
         if (     t->x >= lm->x && t->x < lm->x + w
               && t->y >= lm->y && t->y < lm->y + h)
            return i;
      }
   if (t && t->monitor_index > 0 && t->monitor_index <= st->nlm)
      return t->monitor_index - 1;
   for (i = 0; i < st->nlm; i++)
      if (st->lm[i].primary)
         return i;
   return 0;
}

/* The listed mode of that size nearest the rate: within half a hertz
 * or matching the whole-hertz label, progressive first, the current
 * one on a tie */
static const mdc_mode_t *mdc_find_mode(const mdc_monitor_t *mon,
      int w, int h, int int_hz, double hz)
{
   int i;
   double best_diff       = 0.0;
   const mdc_mode_t *best = NULL;
   for (i = 0; i < mon->nmodes; i++)
   {
      double diff;
      const mdc_mode_t *m = &mon->modes[i];
      if (m->w != w || m->h != h)
         continue;
      diff = m->refresh - hz;
      if (diff < 0.0)
         diff = -diff;
      if (diff >= 0.5 && !(int_hz > 0 && (int)(m->refresh + 0.001) == int_hz))
         continue;
      if (m->interlaced)
         diff += 1000.0;
      if (!best || diff < best_diff || (diff == best_diff && m->current))
      {
         best      = m;
         best_diff = diff;
      }
   }
   return best;
}

static bool mdc_mode_has_scale(const mdc_mode_t *m, double scale)
{
   int i;
   if (!m->nscales)
      return true;
   for (i = 0; i < m->nscales; i++)
      if (m->scales[i] - scale < 0.0001 && scale - m->scales[i] < 0.0001)
         return true;
   return false;
}

/* The supported scale nearest the one in use */
static double mdc_nearest_scale(const mdc_mode_t *m, double scale)
{
   int i;
   double best = scale, best_diff = -1.0;
   for (i = 0; i < m->nscales; i++)
   {
      double d = m->scales[i] - scale;
      if (d < 0.0)
         d = -d;
      if (best_diff < 0.0 || d < best_diff)
      {
         best      = m->scales[i];
         best_diff = d;
      }
   }
   return best;
}

/* ------------------------------------------------------------------
 * Resolution list
 * ------------------------------------------------------------------ */

static int mdc_list_qsort(const void *pa, const void *pb)
{
   const video_display_config_t *a = (const video_display_config_t*)pa;
   const video_display_config_t *b = (const video_display_config_t*)pb;
   if (a->dims != b->dims)
      return a->dims < b->dims ? -1 : 1;
   if (a->interlaced != b->interlaced)
      return a->interlaced ? 1 : -1;
   if (a->refreshrate_float != b->refreshrate_float)
      return a->refreshrate_float < b->refreshrate_float ? -1 : 1;
   return 0;
}

/* The target head's modes as the menu's list, from a state snapshot. */
static enum mutter_dc_result mdc_list_from(const mdc_state_t *st,
      const mutter_dc_target_t *target,
      video_display_config_t **list, unsigned *len)
{
   int i;
   unsigned j, n                = 0;
   const mdc_monitor_t *mon     = NULL;
   video_display_config_t *conf = NULL;

   *list = NULL;
   *len  = 0;

   mon = &st->mon[st->lm[mdc_pick_logical(st, target)].mon[0]];
   if (mon->nmodes > 0
         && (conf = (video_display_config_t*)calloc(mon->nmodes, sizeof(*conf))))
   {
      for (i = 0; i < mon->nmodes; i++)
      {
         video_display_config_t e;
         const mdc_mode_t *m = &mon->modes[i];
         bool dup            = false;

         memset(&e, 0, sizeof(e));
         e.dims              = VIDEO_SCALE_PACK(m->w, m->h);
         e.bpp               = 32;
         e.refreshrate_float = (float)m->refresh;
         e.refreshrate       = (unsigned)(m->refresh + 0.001);
         e.interlaced        = m->interlaced;
         e.current           = m->current;

         /* The same size and rate twice (a fixed and a variable rate
          * mode, say) is one choice in the menu */
         for (j = 0; j < n; j++)
         {
            float d = conf[j].refreshrate_float - e.refreshrate_float;
            if (     conf[j].dims       == e.dims
                  && conf[j].interlaced == e.interlaced
                  && d < 0.005f && d > -0.005f)
            {
               conf[j].current = conf[j].current || e.current;
               dup             = true;
               break;
            }
         }
         if (!dup)
            conf[n++] = e;
      }
      qsort(conf, n, sizeof(*conf), mdc_list_qsort);
      for (j = 0; j < n; j++)
         conf[j].idx = j;
   }

   if (!n)
   {
      free(conf);
      return MUTTER_DC_FAILED;
   }
   *list = conf;
   *len  = n;
   return MUTTER_DC_OK;
}

/* ------------------------------------------------------------------
 * ApplyMonitorsConfig
 * ------------------------------------------------------------------ */

static bool mdc_append_dict_bool(rdbus_iter_t *dict, const char *key,
      bool value)
{
   rdbus_iter_t e, v;
   rdbus_bool_t b = value ? 1 : 0;
   return mdc_rd->message_iter_open_container(dict, RDBUS_TYPE_DICT_ENTRY, NULL, &e)
      && mdc_rd->message_iter_append_basic(&e, RDBUS_TYPE_STRING, &key)
      && mdc_rd->message_iter_open_container(&e, RDBUS_TYPE_VARIANT, "b", &v)
      && mdc_rd->message_iter_append_basic(&v, RDBUS_TYPE_BOOLEAN, &b)
      && mdc_rd->message_iter_close_container(&e, &v)
      && mdc_rd->message_iter_close_container(dict, &e);
}

static bool mdc_append_dict_u32(rdbus_iter_t *dict, const char *key,
      unsigned value)
{
   rdbus_iter_t e, v;
   uint32_t u = value;
   return mdc_rd->message_iter_open_container(dict, RDBUS_TYPE_DICT_ENTRY, NULL, &e)
      && mdc_rd->message_iter_append_basic(&e, RDBUS_TYPE_STRING, &key)
      && mdc_rd->message_iter_open_container(&e, RDBUS_TYPE_VARIANT, "u", &v)
      && mdc_rd->message_iter_append_basic(&v, RDBUS_TYPE_UINT32, &u)
      && mdc_rd->message_iter_close_container(&e, &v)
      && mdc_rd->message_iter_close_container(dict, &e);
}

/* (ssa{sv}): connector, mode id, and the monitor's own settings as it
 * reported them, so colour mode, RGB range and underscanning carry
 * over instead of falling back to their defaults */
static bool mdc_append_monitor(rdbus_iter_t *arr, const mdc_monitor_t *mon,
      const char *mode_id)
{
   rdbus_iter_t s, props;
   const char *c = mon->connector;
   if (     !mdc_rd->message_iter_open_container(arr, RDBUS_TYPE_STRUCT, NULL, &s)
         || !mdc_rd->message_iter_append_basic(&s, RDBUS_TYPE_STRING, &c)
         || !mdc_rd->message_iter_append_basic(&s, RDBUS_TYPE_STRING, &mode_id)
         || !mdc_rd->message_iter_open_container(&s, RDBUS_TYPE_ARRAY, "{sv}", &props))
      return false;
   if (mon->has_underscanning
         && !mdc_append_dict_bool(&props, "enable_underscanning", mon->underscanning))
      return false;
   if (mon->has_color_mode
         && !mdc_append_dict_u32(&props, "color-mode", mon->color_mode))
      return false;
   if (mon->has_rgb_range
         && !mdc_append_dict_u32(&props, "rgb-range", mon->rgb_range))
      return false;
   return mdc_rd->message_iter_close_container(&s, &props)
      && mdc_rd->message_iter_close_container(arr, &s);
}

/* Switches the target head as asked, against the state Mutter reports
 * now, on the worker's connection. */
static enum mutter_dc_result mdc_apply(rdbus_connection_t *conn,
      const mutter_dc_target_t *target,
      unsigned dims, int int_hz, float hz)
{
   int i, j, t;
   int want_w, want_h, old_w = 0, old_h = 0, new_w = 0, new_h = 0;
   int *x = NULL, *y = NULL;
   double new_scale;
   double want_hz                  = hz;
   uint32_t serial, method    = MDC_METHOD_TEMPORARY;
   enum mutter_dc_result result    = MUTTER_DC_FAILED;
   const mdc_mode_t **chosen       = NULL;
   const mdc_mode_t *cur, *best;
   const mdc_monitor_t *mon;
   mdc_logical_t *lm;
   rdbus_iter_t it, lms, props;
   rdbus_message_t *msg                = NULL;
   rdbus_message_t *reply              = NULL;
   mdc_state_t *st                 = NULL;
   rdbus_error_t err;

   mdc_rd->error_init(&err);
   if (!(st = mdc_get_state(conn)))
      return MUTTER_DC_UNAVAILABLE;

   t      = mdc_pick_logical(st, target);
   lm     = &st->lm[t];
   mon    = &st->mon[lm->mon[0]];
   cur    = mdc_current_mode(mon);
   want_w = (int)VIDEO_SCALE_W(dims);
   want_h = (int)VIDEO_SCALE_H(dims);
   if (!want_w)
      want_w = cur ? cur->w : 0;
   if (!want_h)
      want_h = cur ? cur->h : 0;
   if (want_hz <= 0.0 && int_hz > 0)
      want_hz = int_hz;
   if (want_hz <= 0.0 && cur)
      want_hz = cur->refresh;

   if (!(best = mdc_find_mode(mon, want_w, want_h, int_hz, want_hz)))
   {
      RARCH_WARN("[Mutter] No listed mode %dx%d at %.3f Hz on %s.\n",
            want_w, want_h, want_hz, mon->connector);
      goto end;
   }
   if (cur && best == cur && lm->nmon == 1)
   {
      result = MUTTER_DC_OK;
      goto end;
   }

   /* Mode per monitor of the head: a mirror takes the same size and
    * rate on each of its monitors, or nothing */
   if (!(chosen = (const mdc_mode_t**)calloc(st->nmon, sizeof(*chosen)))
         || !(x = (int*)calloc(st->nlm, sizeof(*x)))
         || !(y = (int*)calloc(st->nlm, sizeof(*y))))
      goto end;
   for (i = 0; i < st->nmon; i++)
      chosen[i] = mdc_current_mode(&st->mon[i]);
   chosen[lm->mon[0]] = best;
   for (j = 1; j < lm->nmon; j++)
   {
      const mdc_mode_t *mm = mdc_find_mode(&st->mon[lm->mon[j]],
            best->w, best->h, (int)(best->refresh + 0.001), best->refresh);
      if (!mm)
      {
         RARCH_WARN("[Mutter] Mirror %s has no %dx%d at %.3f Hz.\n",
               st->mon[lm->mon[j]].connector, best->w, best->h, best->refresh);
         goto end;
      }
      chosen[lm->mon[j]] = mm;
   }
   for (i = 0; i < st->nmon; i++)
   {
      bool lit = false;
      int k;
      for (j = 0; j < st->nlm && !lit; j++)
         for (k = 0; k < st->lm[j].nmon; k++)
            if (st->lm[j].mon[k] == i)
               lit = true;
      if (lit && !chosen[i])
         goto end;
   }

   /* Keep the scale unless the new mode cannot take it */
   new_scale = lm->scale;
   if (!mdc_mode_has_scale(best, new_scale))
      new_scale = mdc_nearest_scale(best, new_scale);

   /* Heads to the right of and below this one move with its new
    * extent, so the layout stays adjacent without overlaps */
   mdc_logical_size(st, lm, cur, lm->scale, &old_w, &old_h);
   mdc_logical_size(st, lm, best, new_scale, &new_w, &new_h);
   for (i = 0; i < st->nlm; i++)
   {
      x[i] = st->lm[i].x;
      y[i] = st->lm[i].y;
      if (i == t)
         continue;
      if (st->lm[i].x >= lm->x + old_w)
         x[i] += new_w - old_w;
      if (st->lm[i].y >= lm->y + old_h)
         y[i] += new_h - old_h;
   }

   if (!(msg = mdc_rd->message_new_method_call(MDC_BUS_NAME, MDC_PATH,
               MDC_IFACE, "ApplyMonitorsConfig")))
      goto end;
   serial = st->serial;
   mdc_rd->message_iter_init_append(msg, &it);
   if (     !mdc_rd->message_iter_append_basic(&it, RDBUS_TYPE_UINT32, &serial)
         || !mdc_rd->message_iter_append_basic(&it, RDBUS_TYPE_UINT32, &method)
         || !mdc_rd->message_iter_open_container(&it, RDBUS_TYPE_ARRAY,
               "(iiduba(ssa{sv}))", &lms))
      goto end;
   for (i = 0; i < st->nlm; i++)
   {
      rdbus_iter_t s, mons;
      const mdc_logical_t *l  = &st->lm[i];
      int32_t lx         = x[i];
      int32_t ly         = y[i];
      double sc               = (i == t) ? new_scale : l->scale;
      uint32_t tr        = l->transform;
      rdbus_bool_t pr          = l->primary ? 1 : 0;
      if (     !mdc_rd->message_iter_open_container(&lms, RDBUS_TYPE_STRUCT, NULL, &s)
            || !mdc_rd->message_iter_append_basic(&s, RDBUS_TYPE_INT32, &lx)
            || !mdc_rd->message_iter_append_basic(&s, RDBUS_TYPE_INT32, &ly)
            || !mdc_rd->message_iter_append_basic(&s, RDBUS_TYPE_DOUBLE, &sc)
            || !mdc_rd->message_iter_append_basic(&s, RDBUS_TYPE_UINT32, &tr)
            || !mdc_rd->message_iter_append_basic(&s, RDBUS_TYPE_BOOLEAN, &pr)
            || !mdc_rd->message_iter_open_container(&s, RDBUS_TYPE_ARRAY,
                  "(ssa{sv})", &mons))
         goto end;
      for (j = 0; j < l->nmon; j++)
         if (!mdc_append_monitor(&mons, &st->mon[l->mon[j]],
                  chosen[l->mon[j]]->id))
            goto end;
      if (     !mdc_rd->message_iter_close_container(&s, &mons)
            || !mdc_rd->message_iter_close_container(&lms, &s))
         goto end;
   }
   if (     !mdc_rd->message_iter_close_container(&it, &lms)
         || !mdc_rd->message_iter_open_container(&it, RDBUS_TYPE_ARRAY, "{sv}", &props))
      goto end;
   /* The layout mode goes along only where Mutter lets it be chosen;
    * elsewhere naming it is an error even at its current value */
   if (st->has_layout_mode && st->supports_layout_change
         && !mdc_append_dict_u32(&props, "layout-mode", st->layout_mode))
      goto end;
   if (!mdc_rd->message_iter_close_container(&it, &props))
      goto end;

   reply = mdc_rd->connection_send_with_reply_and_block(conn, msg,
         MDC_TIMEOUT_MS, &err);
   if (mdc_rd->error_is_set(&err))
   {
      RARCH_ERR("[Mutter] Switching %s to %dx%d %.3f Hz failed: %s.\n",
            mon->connector, best->w, best->h, best->refresh, err.message);
      goto end;
   }
   RARCH_LOG("[Mutter] %s switched to %dx%d %.3f Hz.\n",
         mon->connector, best->w, best->h, best->refresh);
   result = MUTTER_DC_OK;

end:
   if (mdc_rd->error_is_set(&err))
      mdc_rd->error_free(&err);
   if (reply)
      mdc_rd->message_unref(reply);
   if (msg)
      mdc_rd->message_unref(msg);
   free(chosen);
   free(x);
   free(y);
   mdc_state_free(st);
   return result;
}

/* ------------------------------------------------------------------
 * The worker
 *
 * One thread for the process, started on first use, owns everything
 * that waits: loading libdbus, the session-bus connection, following
 * MonitorsChanged, GetCurrentState and ApplyMonitorsConfig. It waits in
 * poll() on the bus socket and a wake pipe. Callers read the state it
 * publishes and queue switches to it, under a lock it takes only to
 * publish or to take the queue, never across a D-Bus call.
 * ------------------------------------------------------------------ */

#define MDC_MAX_REQUESTS 8

enum mdc_status
{
   MDC_STATUS_PENDING = 0,
   MDC_STATUS_ABSENT,
   MDC_STATUS_PRESENT
};

typedef struct mdc_request
{
   mutter_dc_target_t target;
   char               connector[64];
   unsigned           dims;
   int                int_hz;
   float              hz;
} mdc_request_t;

typedef struct mdc_ctl
{
   slock_t      *lock;
   mdc_state_t  *state;
   int           wake[2];
   int           status;
   unsigned      nreq;
   mdc_request_t req[MDC_MAX_REQUESTS];
} mdc_ctl_t;

static mdc_ctl_t *mdc_ctl;

/* Whether a switch can be asked of Mutter at all, from the snapshot:
 * the target head lists the size and rate, and every mirror of it
 * does too. *noop is set when the head already shows that mode. */
static enum mutter_dc_result mdc_check(const mdc_state_t *st,
      const mutter_dc_target_t *target, unsigned dims, int int_hz,
      float hz, bool *noop)
{
   int j, want_w, want_h;
   double want_hz              = hz;
   int t                       = mdc_pick_logical(st, target);
   const mdc_logical_t *lm     = &st->lm[t];
   const mdc_monitor_t *mon    = &st->mon[lm->mon[0]];
   const mdc_mode_t *cur       = mdc_current_mode(mon);
   const mdc_mode_t *best;

   *noop  = false;
   want_w = (int)VIDEO_SCALE_W(dims);
   want_h = (int)VIDEO_SCALE_H(dims);
   if (!want_w)
      want_w = cur ? cur->w : 0;
   if (!want_h)
      want_h = cur ? cur->h : 0;
   if (want_hz <= 0.0 && int_hz > 0)
      want_hz = int_hz;
   if (want_hz <= 0.0 && cur)
      want_hz = cur->refresh;

   if (!(best = mdc_find_mode(mon, want_w, want_h, int_hz, want_hz)))
   {
      RARCH_WARN("[Mutter] No listed mode %dx%d at %.3f Hz on %s.\n",
            want_w, want_h, want_hz, mon->connector);
      return MUTTER_DC_FAILED;
   }
   for (j = 1; j < lm->nmon; j++)
      if (!mdc_find_mode(&st->mon[lm->mon[j]], best->w, best->h,
               (int)(best->refresh + 0.001), best->refresh))
      {
         RARCH_WARN("[Mutter] Mirror %s has no %dx%d at %.3f Hz.\n",
               st->mon[lm->mon[j]].connector, best->w, best->h,
               best->refresh);
         return MUTTER_DC_FAILED;
      }
   *noop = (cur && best == cur && lm->nmon == 1);
   return MUTTER_DC_OK;
}

static void mdc_publish(mdc_ctl_t *ctl, mdc_state_t *st, int status)
{
   mdc_state_t *old;
   slock_lock(ctl->lock);
   old         = ctl->state;
   ctl->state  = st;
   ctl->status = status;
   slock_unlock(ctl->lock);
   mdc_state_free(old);
}

static void mdc_worker(void *data)
{
   int fd                   = -1;
   bool refresh             = true;
   mdc_ctl_t *ctl           = (mdc_ctl_t*)data;
   rdbus_connection_t *conn = NULL;

   sthread_setname("ra-mutter");

   if (     !(mdc_rd = rdbus_get())
         || !(conn   = mdc_connect())
         || !mdc_has_owner(conn)
         || !mdc_rd->connection_get_unix_fd(conn, &fd))
   {
      mdc_disconnect(conn);
      mdc_publish(ctl, NULL, MDC_STATUS_ABSENT);
      return;
   }

   {
      rdbus_error_t err;
      mdc_rd->error_init(&err);
      mdc_rd->bus_add_match(conn,
            "type='signal',interface='" MDC_IFACE "',member='MonitorsChanged'",
            &err);
      if (mdc_rd->error_is_set(&err))
         mdc_rd->error_free(&err);
   }

   for (;;)
   {
      unsigned i, n;
      rdbus_message_t *m;
      mdc_request_t req[MDC_MAX_REQUESTS];
      struct pollfd pfd[2];

      /* Whatever the bus sent while this thread was in a call or
       * asleep: only a change of monitors matters. */
      mdc_rd->connection_read_write(conn, 0);
      while ((m = mdc_rd->connection_pop_message(conn)))
      {
         if (mdc_rd->message_is_signal(m, MDC_IFACE, "MonitorsChanged"))
            refresh = true;
         mdc_rd->message_unref(m);
      }

      if (refresh)
      {
         mdc_state_t *st = mdc_get_state(conn);
         refresh         = false;
         if (st)
            mdc_publish(ctl, st, MDC_STATUS_PRESENT);
      }

      slock_lock(ctl->lock);
      n = ctl->nreq;
      memcpy(req, ctl->req, n * sizeof(req[0]));
      ctl->nreq = 0;
      slock_unlock(ctl->lock);

      for (i = 0; i < n; i++)
      {
         if (req[i].connector[0])
            req[i].target.connector = req[i].connector;
         mdc_apply(conn, &req[i].target, req[i].dims, req[i].int_hz,
               req[i].hz);
         /* Mutter announces the change too; the state is asked again
          * either way, so the next reader sees the new mode. */
         refresh = true;
      }
      if (refresh || n)
         continue;

      pfd[0].fd      = fd;
      pfd[0].events  = POLLIN;
      pfd[0].revents = 0;
      pfd[1].fd      = ctl->wake[0];
      pfd[1].events  = POLLIN;
      pfd[1].revents = 0;
      if (poll(pfd, 2, -1) < 0)
         continue;
      if (pfd[1].revents & POLLIN)
      {
         char buf[64];
         while (read(ctl->wake[0], buf, sizeof(buf)) > 0) { }
      }
      if (pfd[0].revents & (POLLHUP | POLLERR))
         break;
   }

   /* The bus went away: nothing to follow any more. */
   mdc_disconnect(conn);
   mdc_publish(ctl, NULL, MDC_STATUS_ABSENT);
}

static mdc_ctl_t *mdc_start(void)
{
   sthread_t *thread;
   mdc_ctl_t *ctl = mdc_ctl;

   if (ctl)
      return ctl;
   if (!(ctl = (mdc_ctl_t*)calloc(1, sizeof(*ctl))))
      return NULL;
   ctl->wake[0] = ctl->wake[1] = -1;
   if (     !(ctl->lock = slock_new())
         || pipe(ctl->wake) != 0
         || fcntl(ctl->wake[0], F_SETFL, O_NONBLOCK) != 0
         || fcntl(ctl->wake[1], F_SETFL, O_NONBLOCK) != 0
         || !(thread = sthread_create(mdc_worker, ctl)))
   {
      if (ctl->wake[0] >= 0)
         close(ctl->wake[0]);
      if (ctl->wake[1] >= 0)
         close(ctl->wake[1]);
      if (ctl->lock)
         slock_free(ctl->lock);
      free(ctl);
      return NULL;
   }
   /* Lives as long as the process; the block stays with it. */
   sthread_detach(thread);
   mdc_ctl = ctl;
   return ctl;
}

bool mutter_displayconfig_available(void)
{
   bool ret;
   mdc_ctl_t *ctl = mdc_start();
   if (!ctl)
      return false;
   slock_lock(ctl->lock);
   ret = ctl->status == MDC_STATUS_PRESENT && ctl->state;
   slock_unlock(ctl->lock);
   return ret;
}

enum mutter_dc_result mutter_displayconfig_get_resolution_list(
      const mutter_dc_target_t *target,
      video_display_config_t **list, unsigned *len)
{
   enum mutter_dc_result ret = MUTTER_DC_UNAVAILABLE;
   mdc_ctl_t *ctl            = mdc_start();

   *list = NULL;
   *len  = 0;
   if (!ctl)
      return MUTTER_DC_UNAVAILABLE;
   slock_lock(ctl->lock);
   if (ctl->status == MDC_STATUS_PRESENT && ctl->state)
      ret = mdc_list_from(ctl->state, target, list, len);
   slock_unlock(ctl->lock);
   return ret;
}

enum mutter_dc_result mutter_displayconfig_set_resolution(
      const mutter_dc_target_t *target,
      unsigned dims, int int_hz, float hz)
{
   bool noop                 = false;
   bool wake                 = false;
   enum mutter_dc_result ret = MUTTER_DC_UNAVAILABLE;
   mdc_ctl_t *ctl            = mdc_start();

   if (!ctl)
      return MUTTER_DC_UNAVAILABLE;
   slock_lock(ctl->lock);
   if (     ctl->status == MDC_STATUS_PRESENT && ctl->state
         && (ret = mdc_check(ctl->state, target, dims, int_hz, hz,
               &noop)) == MUTTER_DC_OK
         && !noop)
   {
      /* A full queue keeps its newest slot for the newest request. */
      mdc_request_t *r = &ctl->req[ctl->nreq < MDC_MAX_REQUESTS
         ? ctl->nreq++ : MDC_MAX_REQUESTS - 1];
      memset(r, 0, sizeof(*r));
      if (target)
      {
         r->target           = *target;
         r->target.connector = NULL;
         if (target->connector)
            strlcpy(r->connector, target->connector, sizeof(r->connector));
      }
      r->dims   = dims;
      r->int_hz = int_hz;
      r->hz     = hz;
      wake      = true;
   }
   slock_unlock(ctl->lock);
   if (wake)
   {
      char c = 1;
      if (write(ctl->wake[1], &c, 1) < 0) { }
   }
   return ret;
}

#endif
