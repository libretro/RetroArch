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
#include <unistd.h>

#include <dbus/dbus.h>

#include <compat/strl.h>

#include "mutter_displayconfig.h"
#include "../../verbosity.h"

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
static DBusConnection *mdc_connect(void)
{
   DBusError err;
   DBusConnection *conn;
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

   dbus_error_init(&err);
   conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
   if (dbus_error_is_set(&err))
      dbus_error_free(&err);
   if (conn)
      dbus_connection_set_exit_on_disconnect(conn, false);
   return conn;
}

static void mdc_disconnect(DBusConnection *conn)
{
   if (!conn)
      return;
   dbus_connection_close(conn);
   dbus_connection_unref(conn);
}

static bool mdc_has_owner(DBusConnection *conn)
{
   DBusError err;
   bool has;
   dbus_error_init(&err);
   has = dbus_bus_name_has_owner(conn, MDC_BUS_NAME, &err) ? true : false;
   if (dbus_error_is_set(&err))
   {
      dbus_error_free(&err);
      return false;
   }
   return has;
}

bool mutter_displayconfig_available(void)
{
   bool has;
   DBusConnection *conn = mdc_connect();
   if (!conn)
      return false;
   has = mdc_has_owner(conn);
   mdc_disconnect(conn);
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

static int mdc_count(DBusMessageIter *array)
{
   int n = 0;
   DBusMessageIter it;
   dbus_message_iter_recurse(array, &it);
   while (dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_INVALID)
   {
      n++;
      dbus_message_iter_next(&it);
   }
   return n;
}

/* One a{sv} entry's variant, if it holds a basic value of type t */
static bool mdc_variant_basic(DBusMessageIter *variant, int t, void *out)
{
   DBusMessageIter v;
   dbus_message_iter_recurse(variant, &v);
   if (dbus_message_iter_get_arg_type(&v) != t)
      return false;
   dbus_message_iter_get_basic(&v, out);
   return true;
}

/* Walks an a{sv}; cb is handed each key and its variant */
typedef void (*mdc_prop_cb)(void *ud, const char *key,
      DBusMessageIter *variant);

static bool mdc_props(DBusMessageIter *array, mdc_prop_cb cb, void *ud)
{
   DBusMessageIter it;
   if (dbus_message_iter_get_arg_type(array) != DBUS_TYPE_ARRAY)
      return false;
   dbus_message_iter_recurse(array, &it);
   while (dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_DICT_ENTRY)
   {
      DBusMessageIter e;
      const char *key = NULL;
      dbus_message_iter_recurse(&it, &e);
      if (dbus_message_iter_get_arg_type(&e) == DBUS_TYPE_STRING)
      {
         dbus_message_iter_get_basic(&e, &key);
         dbus_message_iter_next(&e);
         if (dbus_message_iter_get_arg_type(&e) == DBUS_TYPE_VARIANT)
            cb(ud, key, &e);
      }
      dbus_message_iter_next(&it);
   }
   return true;
}

static void mdc_mode_prop(void *ud, const char *key, DBusMessageIter *v)
{
   mdc_mode_t *m = (mdc_mode_t*)ud;
   dbus_bool_t b = FALSE;
   if (!strcmp(key, "is-current") && mdc_variant_basic(v, DBUS_TYPE_BOOLEAN, &b))
      m->current    = b ? true : false;
   else if (!strcmp(key, "is-interlaced") && mdc_variant_basic(v, DBUS_TYPE_BOOLEAN, &b))
      m->interlaced = b ? true : false;
}

static void mdc_monitor_prop(void *ud, const char *key, DBusMessageIter *v)
{
   mdc_monitor_t *mon = (mdc_monitor_t*)ud;
   dbus_bool_t b      = FALSE;
   dbus_uint32_t u    = 0;
   if (!strcmp(key, "is-underscanning") && mdc_variant_basic(v, DBUS_TYPE_BOOLEAN, &b))
   {
      mon->underscanning     = b ? true : false;
      mon->has_underscanning = true;
   }
   else if (!strcmp(key, "color-mode") && mdc_variant_basic(v, DBUS_TYPE_UINT32, &u))
   {
      mon->color_mode     = u;
      mon->has_color_mode = true;
   }
   else if (!strcmp(key, "rgb-range") && mdc_variant_basic(v, DBUS_TYPE_UINT32, &u))
   {
      mon->rgb_range     = u;
      mon->has_rgb_range = true;
   }
}

static void mdc_global_prop(void *ud, const char *key, DBusMessageIter *v)
{
   mdc_state_t *st = (mdc_state_t*)ud;
   dbus_bool_t b   = FALSE;
   dbus_uint32_t u = 0;
   if (!strcmp(key, "layout-mode") && mdc_variant_basic(v, DBUS_TYPE_UINT32, &u))
   {
      st->layout_mode     = u;
      st->has_layout_mode = true;
   }
   else if (!strcmp(key, "supports-changing-layout-mode")
         && mdc_variant_basic(v, DBUS_TYPE_BOOLEAN, &b))
      st->supports_layout_change = b ? true : false;
}

/* (ssss): the connector is the first string */
static bool mdc_spec_connector(DBusMessageIter *spec, char *s, size_t len)
{
   DBusMessageIter f;
   const char *c = NULL;
   if (dbus_message_iter_get_arg_type(spec) != DBUS_TYPE_STRUCT)
      return false;
   dbus_message_iter_recurse(spec, &f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_STRING)
      return false;
   dbus_message_iter_get_basic(&f, &c);
   strlcpy(s, c ? c : "", len);
   return true;
}

/* (siiddada{sv}) */
static bool mdc_parse_mode(DBusMessageIter *s, mdc_mode_t *m)
{
   DBusMessageIter f, sc;
   const char *id    = NULL;
   dbus_int32_t w    = 0, h = 0;
   double refresh    = 0.0, pref = 0.0;

   dbus_message_iter_recurse(s, &f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_STRING)
      return false;
   dbus_message_iter_get_basic(&f, &id);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_INT32)
      return false;
   dbus_message_iter_get_basic(&f, &w);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_INT32)
      return false;
   dbus_message_iter_get_basic(&f, &h);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_DOUBLE)
      return false;
   dbus_message_iter_get_basic(&f, &refresh);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_DOUBLE)
      return false;
   dbus_message_iter_get_basic(&f, &pref);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_ARRAY)
      return false;

   memset(m, 0, sizeof(*m));
   strlcpy(m->id, id ? id : "", sizeof(m->id));
   m->w       = (int)w;
   m->h       = (int)h;
   m->refresh = refresh;

   dbus_message_iter_recurse(&f, &sc);
   while (dbus_message_iter_get_arg_type(&sc) == DBUS_TYPE_DOUBLE
         && m->nscales < MDC_MAX_SCALES)
   {
      dbus_message_iter_get_basic(&sc, &m->scales[m->nscales++]);
      dbus_message_iter_next(&sc);
   }
   dbus_message_iter_next(&f);
   mdc_props(&f, mdc_mode_prop, m);
   return m->id[0] && m->w > 0 && m->h > 0;
}

/* ((ssss)a(siiddada{sv})a{sv}) */
static bool mdc_parse_monitor(DBusMessageIter *s, mdc_monitor_t *mon)
{
   DBusMessageIter f, modes;
   int n;

   memset(mon, 0, sizeof(*mon));
   dbus_message_iter_recurse(s, &f);
   if (!mdc_spec_connector(&f, mon->connector, sizeof(mon->connector)))
      return false;
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_ARRAY)
      return false;

   if ((n = mdc_count(&f)) > 0
         && !(mon->modes = (mdc_mode_t*)calloc(n, sizeof(*mon->modes))))
      return false;
   dbus_message_iter_recurse(&f, &modes);
   while (dbus_message_iter_get_arg_type(&modes) == DBUS_TYPE_STRUCT
         && mon->nmodes < n)
   {
      if (mdc_parse_mode(&modes, &mon->modes[mon->nmodes]))
         mon->nmodes++;
      dbus_message_iter_next(&modes);
   }
   dbus_message_iter_next(&f);
   mdc_props(&f, mdc_monitor_prop, mon);
   return true;
}

/* (iiduba(ssss)a{sv}) */
static bool mdc_parse_logical(DBusMessageIter *s, const mdc_state_t *st,
      mdc_logical_t *lm)
{
   DBusMessageIter f, specs;
   dbus_int32_t x = 0, y = 0;
   dbus_uint32_t transform = 0;
   dbus_bool_t primary = FALSE;
   double scale = 1.0;

   memset(lm, 0, sizeof(*lm));
   dbus_message_iter_recurse(s, &f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_INT32)
      return false;
   dbus_message_iter_get_basic(&f, &x);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_INT32)
      return false;
   dbus_message_iter_get_basic(&f, &y);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_DOUBLE)
      return false;
   dbus_message_iter_get_basic(&f, &scale);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_UINT32)
      return false;
   dbus_message_iter_get_basic(&f, &transform);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_BOOLEAN)
      return false;
   dbus_message_iter_get_basic(&f, &primary);
   dbus_message_iter_next(&f);
   if (dbus_message_iter_get_arg_type(&f) != DBUS_TYPE_ARRAY)
      return false;

   lm->x         = (int)x;
   lm->y         = (int)y;
   lm->scale     = scale > 0.0 ? scale : 1.0;
   lm->transform = transform;
   lm->primary   = primary ? true : false;

   dbus_message_iter_recurse(&f, &specs);
   while (dbus_message_iter_get_arg_type(&specs) == DBUS_TYPE_STRUCT)
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
      dbus_message_iter_next(&specs);
   }
   return lm->nmon > 0;
}

static mdc_state_t *mdc_get_state(DBusConnection *conn)
{
   DBusMessageIter it, arr;
   DBusError err;
   int n;
   dbus_uint32_t serial = 0;
   DBusMessage *reply   = NULL;
   mdc_state_t *st      = NULL;
   DBusMessage *msg     = dbus_message_new_method_call(MDC_BUS_NAME,
         MDC_PATH, MDC_IFACE, "GetCurrentState");

   if (!msg)
      return NULL;
   dbus_error_init(&err);
   reply = dbus_connection_send_with_reply_and_block(conn, msg,
         MDC_TIMEOUT_MS, &err);
   dbus_message_unref(msg);
   if (dbus_error_is_set(&err))
   {
      RARCH_WARN("[Mutter] GetCurrentState: %s.\n", err.message);
      dbus_error_free(&err);
      return NULL;
   }
   if (!reply)
      return NULL;

   if (     !dbus_message_iter_init(reply, &it)
         || dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_UINT32
         || !(st = (mdc_state_t*)calloc(1, sizeof(*st))))
      goto fail;
   dbus_message_iter_get_basic(&it, &serial);
   st->serial      = serial;
   st->layout_mode = MDC_LAYOUT_LOGICAL;

   /* monitors */
   dbus_message_iter_next(&it);
   if (dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_ARRAY)
      goto fail;
   if ((n = mdc_count(&it)) > 0
         && !(st->mon = (mdc_monitor_t*)calloc(n, sizeof(*st->mon))))
      goto fail;
   dbus_message_iter_recurse(&it, &arr);
   while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_STRUCT
         && st->nmon < n)
   {
      if (mdc_parse_monitor(&arr, &st->mon[st->nmon]))
         st->nmon++;
      else
         free(st->mon[st->nmon].modes);
      dbus_message_iter_next(&arr);
   }

   /* logical monitors */
   dbus_message_iter_next(&it);
   if (dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_ARRAY)
      goto fail;
   if ((n = mdc_count(&it)) > 0
         && !(st->lm = (mdc_logical_t*)calloc(n, sizeof(*st->lm))))
      goto fail;
   dbus_message_iter_recurse(&it, &arr);
   while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_STRUCT
         && st->nlm < n)
   {
      if (mdc_parse_logical(&arr, st, &st->lm[st->nlm]))
         st->nlm++;
      dbus_message_iter_next(&arr);
   }

   /* properties */
   dbus_message_iter_next(&it);
   mdc_props(&it, mdc_global_prop, st);

   dbus_message_unref(reply);
   if (!st->nlm)
   {
      mdc_state_free(st);
      return NULL;
   }
   return st;

fail:
   RARCH_WARN("[Mutter] GetCurrentState reply not understood.\n");
   dbus_message_unref(reply);
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

enum mutter_dc_result mutter_displayconfig_get_resolution_list(
      const mutter_dc_target_t *target,
      video_display_config_t **list, unsigned *len)
{
   int i;
   unsigned j, n                = 0;
   const mdc_monitor_t *mon     = NULL;
   video_display_config_t *conf = NULL;
   mdc_state_t *st              = NULL;
   DBusConnection *conn         = mdc_connect();

   *list = NULL;
   *len  = 0;
   if (!conn)
      return MUTTER_DC_UNAVAILABLE;
   if (!mdc_has_owner(conn) || !(st = mdc_get_state(conn)))
   {
      mdc_disconnect(conn);
      return MUTTER_DC_UNAVAILABLE;
   }
   mdc_disconnect(conn);

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
   mdc_state_free(st);

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

static bool mdc_append_dict_bool(DBusMessageIter *dict, const char *key,
      bool value)
{
   DBusMessageIter e, v;
   dbus_bool_t b = value ? TRUE : FALSE;
   return dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &e)
      && dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key)
      && dbus_message_iter_open_container(&e, DBUS_TYPE_VARIANT, "b", &v)
      && dbus_message_iter_append_basic(&v, DBUS_TYPE_BOOLEAN, &b)
      && dbus_message_iter_close_container(&e, &v)
      && dbus_message_iter_close_container(dict, &e);
}

static bool mdc_append_dict_u32(DBusMessageIter *dict, const char *key,
      unsigned value)
{
   DBusMessageIter e, v;
   dbus_uint32_t u = value;
   return dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL, &e)
      && dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key)
      && dbus_message_iter_open_container(&e, DBUS_TYPE_VARIANT, "u", &v)
      && dbus_message_iter_append_basic(&v, DBUS_TYPE_UINT32, &u)
      && dbus_message_iter_close_container(&e, &v)
      && dbus_message_iter_close_container(dict, &e);
}

/* (ssa{sv}): connector, mode id, and the monitor's own settings as it
 * reported them, so colour mode, RGB range and underscanning carry
 * over instead of falling back to their defaults */
static bool mdc_append_monitor(DBusMessageIter *arr, const mdc_monitor_t *mon,
      const char *mode_id)
{
   DBusMessageIter s, props;
   const char *c = mon->connector;
   if (     !dbus_message_iter_open_container(arr, DBUS_TYPE_STRUCT, NULL, &s)
         || !dbus_message_iter_append_basic(&s, DBUS_TYPE_STRING, &c)
         || !dbus_message_iter_append_basic(&s, DBUS_TYPE_STRING, &mode_id)
         || !dbus_message_iter_open_container(&s, DBUS_TYPE_ARRAY, "{sv}", &props))
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
   return dbus_message_iter_close_container(&s, &props)
      && dbus_message_iter_close_container(arr, &s);
}

enum mutter_dc_result mutter_displayconfig_set_resolution(
      const mutter_dc_target_t *target,
      unsigned dims, int int_hz, float hz)
{
   int i, j, t;
   int want_w, want_h, old_w = 0, old_h = 0, new_w = 0, new_h = 0;
   int *x = NULL, *y = NULL;
   double new_scale;
   double want_hz                  = hz;
   dbus_uint32_t serial, method    = MDC_METHOD_TEMPORARY;
   enum mutter_dc_result result    = MUTTER_DC_FAILED;
   const mdc_mode_t **chosen       = NULL;
   const mdc_mode_t *cur, *best;
   const mdc_monitor_t *mon;
   mdc_logical_t *lm;
   DBusMessageIter it, lms, props;
   DBusMessage *msg                = NULL;
   DBusMessage *reply              = NULL;
   mdc_state_t *st                 = NULL;
   DBusConnection *conn            = mdc_connect();
   DBusError err;

   dbus_error_init(&err);
   if (!conn)
      return MUTTER_DC_UNAVAILABLE;
   if (!mdc_has_owner(conn) || !(st = mdc_get_state(conn)))
   {
      mdc_disconnect(conn);
      return MUTTER_DC_UNAVAILABLE;
   }

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

   if (!(msg = dbus_message_new_method_call(MDC_BUS_NAME, MDC_PATH,
               MDC_IFACE, "ApplyMonitorsConfig")))
      goto end;
   serial = st->serial;
   dbus_message_iter_init_append(msg, &it);
   if (     !dbus_message_iter_append_basic(&it, DBUS_TYPE_UINT32, &serial)
         || !dbus_message_iter_append_basic(&it, DBUS_TYPE_UINT32, &method)
         || !dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY,
               "(iiduba(ssa{sv}))", &lms))
      goto end;
   for (i = 0; i < st->nlm; i++)
   {
      DBusMessageIter s, mons;
      const mdc_logical_t *l  = &st->lm[i];
      dbus_int32_t lx         = x[i];
      dbus_int32_t ly         = y[i];
      double sc               = (i == t) ? new_scale : l->scale;
      dbus_uint32_t tr        = l->transform;
      dbus_bool_t pr          = l->primary ? TRUE : FALSE;
      if (     !dbus_message_iter_open_container(&lms, DBUS_TYPE_STRUCT, NULL, &s)
            || !dbus_message_iter_append_basic(&s, DBUS_TYPE_INT32, &lx)
            || !dbus_message_iter_append_basic(&s, DBUS_TYPE_INT32, &ly)
            || !dbus_message_iter_append_basic(&s, DBUS_TYPE_DOUBLE, &sc)
            || !dbus_message_iter_append_basic(&s, DBUS_TYPE_UINT32, &tr)
            || !dbus_message_iter_append_basic(&s, DBUS_TYPE_BOOLEAN, &pr)
            || !dbus_message_iter_open_container(&s, DBUS_TYPE_ARRAY,
                  "(ssa{sv})", &mons))
         goto end;
      for (j = 0; j < l->nmon; j++)
         if (!mdc_append_monitor(&mons, &st->mon[l->mon[j]],
                  chosen[l->mon[j]]->id))
            goto end;
      if (     !dbus_message_iter_close_container(&s, &mons)
            || !dbus_message_iter_close_container(&lms, &s))
         goto end;
   }
   if (     !dbus_message_iter_close_container(&it, &lms)
         || !dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &props))
      goto end;
   /* The layout mode goes along only where Mutter lets it be chosen;
    * elsewhere naming it is an error even at its current value */
   if (st->has_layout_mode && st->supports_layout_change
         && !mdc_append_dict_u32(&props, "layout-mode", st->layout_mode))
      goto end;
   if (!dbus_message_iter_close_container(&it, &props))
      goto end;

   reply = dbus_connection_send_with_reply_and_block(conn, msg,
         MDC_TIMEOUT_MS, &err);
   if (dbus_error_is_set(&err))
   {
      RARCH_ERR("[Mutter] Switching %s to %dx%d %.3f Hz failed: %s.\n",
            mon->connector, best->w, best->h, best->refresh, err.message);
      goto end;
   }
   RARCH_LOG("[Mutter] %s switched to %dx%d %.3f Hz.\n",
         mon->connector, best->w, best->h, best->refresh);
   result = MUTTER_DC_OK;

end:
   if (dbus_error_is_set(&err))
      dbus_error_free(&err);
   if (reply)
      dbus_message_unref(reply);
   if (msg)
      dbus_message_unref(msg);
   free(chosen);
   free(x);
   free(y);
   mdc_state_free(st);
   mdc_disconnect(conn);
   return result;
}
