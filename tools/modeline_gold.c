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

/* Golden harness for the video modeline engine.
 *
 * Runs a fixed matrix of monitor presets, generator options and source
 * modes through the engine and prints one line per result. The output
 * is compared against tools/modeline_gold.ref, which was produced by
 * the same matrix compiled against the SwitchRes 2.2.1 library this
 * engine replaced (-DMODELINE_GOLD_REFERENCE, C++). Any drift in
 * scoring, timing arithmetic or geometry handling shows up as a diff.
 *
 * Build and check:
 *
 *   gcc -std=c89 -D_GNU_SOURCE -Ilibretro-common/include -I. \
 *       tools/modeline_gold.c -lm -o modeline_gold
 *   ./modeline_gold | diff - tools/modeline_gold.ref
 *
 * The matrix also covers a synthetic "listed modes" display with
 * update-only capabilities, which is the Windows ADL shape: the
 * engine must rewrite the timing of an existing listed mode rather
 * than generate a fresh one. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef MODELINE_GOLD_REFERENCE

#include "../deps/switchres/switchres.h"
#include "../deps/switchres/display.h"
#include "../deps/switchres/custom_video.h"
#include "../deps/switchres/edid.h"

class gold_video : public custom_video
{
   public:
      gold_video(int c) : m_caps(c) {}
      const char *api_name() { return "gold"; }
      int caps() { return m_caps; }
      bool add_mode(modeline *) { return true; }
      bool delete_mode(modeline *) { return true; }
      bool update_mode(modeline *) { return true; }
      bool process_modelist(std::vector<modeline *> l)
      {
         for (auto &m : l) m->type &= ~MODE_ERROR;
         return true;
      }
   private:
      int m_caps;
};

typedef struct
{
   switchres_manager *swr;
   display_manager   *disp;
   gold_video        *vid;
} gold_ctx;

static void gold_log(const char *, ...) {}

static gold_ctx *gold_new(int caps)
{
   gold_ctx *c = new gold_ctx;
   c->swr      = new switchres_manager;
   c->swr->set_log_info_fn((void*)gold_log);
   c->swr->set_log_error_fn((void*)gold_log);
   c->swr->set_log_verbose_fn((void*)gold_log);
   c->swr->display_factory()->set_screen("dummy");
   c->disp     = c->swr->add_display(false);
   c->vid      = NULL;
   if (caps >= 0)
   {
      c->vid = new gold_video(caps);
      c->disp->set_custom_video(c->vid);
   }
   return c;
}

static void gold_free(gold_ctx *c)
{
   /* The display manager restores modes in its destructor through
    * the custom_video; keep it alive until then. */
   c->disp->set_keep_changes(true);
   delete c->swr;
   delete c->vid;
   delete c;
}

static void gold_option(gold_ctx *c, const char *k, const char *v)
{
   c->swr->set_option(k, v);
}

static void gold_parse(gold_ctx *c)
{
   c->disp->parse_options();
}

static void gold_list(gold_ctx *c, const modeline *list, int n)
{
   c->disp->video_modes.clear();
   c->disp->backup_modes.clear();
   for (int i = 0; i < n; i++)
   {
      c->disp->video_modes.push_back(list[i]);
      c->disp->backup_modes.push_back(list[i]);
      if (list[i].type & MODE_DESKTOP)
         c->disp->desktop_mode = list[i];
   }
   if (!strcmp(c->disp->monitor(), "lcd"))
      c->disp->auto_specs();
   c->disp->filter_modes();
}

static const modeline *gold_get(gold_ctx *c, int w, int h, double hz, int flags)
{
   modeline *m = c->disp->get_mode(w, h, (float)hz, flags);
   if (m)
      c->disp->flush_modes();
   return m;
}

/* The EDID's detailed timing and range/name descriptors for a mode:
 * the bytes the 2.2.1 generator and the in-tree one agree on (the
 * vendor, serial and feature-flag bytes differ on purpose). */
static void gold_edid(gold_ctx *c, const modeline *m, unsigned char *out)
{
   edid_block e;
   memset(&e, 0, sizeof(e));
   edid_from_modeline((modeline*)m, &c->disp->range[m->range], c->disp->monitor(), &e);
   memcpy(out, e.b, 128);
}

static const char *gold_monitor(gold_ctx *c) { return c->disp->monitor(); }

typedef modeline gold_mode;
#define GM_STRETCH(m) (((m)->result.weight & R_RES_STRETCH) ? 1 : 0)
#define GM_VOFF(m)    (((m)->result.weight & R_V_FREQ_OFF) ? 1 : 0)
#define GOLD_FLAG_ROT SR_MODE_ROTATED
#define GOLD_FLAG_INT SR_MODE_INTERLACED
#define GOLD_DESKTOP  MODE_DESKTOP
#define GOLD_SYSTEM   CUSTOM_VIDEO_TIMING_SYSTEM
#define GOLD_CAPS_UPDATE (CUSTOM_VIDEO_CAPS_UPDATE)
#define GOLD_CAPS_ADD    (CUSTOM_VIDEO_CAPS_ADD)

#else /* C89 engine */

#define MODELINE_STANDALONE
#define RARCH_LOG(...)  do { } while (0)
#define RARCH_DBG(...)  do { } while (0)
#define RARCH_ERR(...)  do { } while (0)
#define RARCH_WARN(...) do { } while (0)

#include "../gfx/modeline/modeline_core.c"
#include "../gfx/modeline/modeline_monitor.c"
#include "../gfx/modeline/modeline_list.c"
#include "../gfx/modeline/modeline_ini.c"
#include "../gfx/modeline/modeline_edid.c"

typedef struct
{
   video_modeline_gen_t *gen;
   video_modeline_ops_t  ops;
   int caps;
} gold_ctx;

static unsigned gold_caps(void *data)
{
   return (unsigned)*(int*)data;
}

static bool gold_true(void *data, video_modeline_t *m)
{
   return true;
}

static gold_ctx *gold_new(int caps)
{
   gold_ctx *c = (gold_ctx*)calloc(1, sizeof(*c));
   c->gen      = modeline_gen_new();
   c->caps     = caps;
   c->ops.data = &c->caps;
   c->ops.name = "gold";
   c->ops.add  = gold_true;
   c->ops.update = gold_true;
   c->ops.del  = gold_true;
   if (caps >= 0)
      c->ops.caps = gold_caps;
   modeline_list_init(c->gen, &c->ops);
   return c;
}

static void gold_free(gold_ctx *c)
{
   modeline_gen_free(c->gen);
   free(c);
}

static void gold_option(gold_ctx *c, const char *k, const char *v)
{
   modeline_set_option(c->gen, k, v);
}

static void gold_parse(gold_ctx *c)
{
   modeline_parse_options(c->gen);
}

static void gold_list(gold_ctx *c, const video_modeline_t *list, int n)
{
   int i;
   memset(c->gen->modes, 0, MODELINE_MAX_MODES * sizeof(*c->gen->modes));
   for (i = 0; i < n; i++)
   {
      c->gen->modes[i]  = list[i];
      c->gen->backup[i] = list[i];
      if (list[i].type & MODELINE_DESKTOP)
         c->gen->desktop_mode = list[i];
   }
   c->gen->num_modes  = n;
   c->gen->num_backup = n;
   if (!strcmp(c->gen->monitor, "lcd"))
      modeline_auto_specs(c->gen);
   modeline_filter_modes(c->gen);
}

static const video_modeline_t *gold_get(gold_ctx *c, int w, int h,
      double hz, int flags)
{
   video_modeline_t *m = modeline_get(c->gen, &c->ops, w, h, hz, flags);
   if (m)
      modeline_flush(c->gen, &c->ops);
   return m;
}

static void gold_edid(gold_ctx *c, const video_modeline_t *m, unsigned char *out)
{
   modeline_edid_build(m, &c->gen->range[m->range], c->gen->monitor, out);
}

static const char *gold_monitor(gold_ctx *c) { return c->gen->monitor; }

typedef video_modeline_t gold_mode;
#define GM_STRETCH(m) (((m)->result.weight & MODELINE_R_RES_STRETCH) ? 1 : 0)
#define GM_VOFF(m)    (((m)->result.weight & MODELINE_R_V_FREQ_OFF) ? 1 : 0)
#define GOLD_FLAG_ROT MODELINE_REQ_ROTATED
#define GOLD_FLAG_INT MODELINE_REQ_INTERLACED
#define GOLD_DESKTOP  MODELINE_DESKTOP
#define GOLD_SYSTEM   MODELINE_TIMING_SYSTEM
#define GOLD_CAPS_UPDATE (MODELINE_CAPS_UPDATE)
#define GOLD_CAPS_ADD    (MODELINE_CAPS_ADD)

#endif

typedef struct
{
   int w, h, flags;
   double hz;
} gold_src;

static const gold_src gold_sources[] = {
   { 256,  224, 0,             60.0988 },
   { 320,  240, 0,             59.94   },
   { 384,  256, 0,             55.017  },
   { 640,  480, 0,             60.0    },
   { 256,  240, 0,             50.0    },
   { 512,  448, 0,             60.1    },
   { 320,  224, 0,             57.5    },
   { 1280, 720, 0,             60.0    },
   { 640,  240, 0,             60.0    },
   { 2560, 240, 0,             60.0988 },
   { 240,  320, GOLD_FLAG_ROT, 60.0    },
   { 224,  384, GOLD_FLAG_ROT, 59.185  },
   { 720,  480, GOLD_FLAG_INT, 59.94   },
   { 800,  600, 0,             72.0    },
   { 352,  240, 0,             59.83   },
   { 292,  240, 0,             61.0    },
   { 400,  254, 0,             54.6    },
   { 1920, 1080, 0,            60.0    },
   { 160,  144, 0,             59.73   },
   { 640,  400, 0,             70.0    }
};

typedef struct
{
   const char *name;
   const char *opts[10];
} gold_case;

static const gold_case gold_cases[] = {
   { "arcade_15",       { "monitor", "arcade_15", NULL } },
   { "arcade_31",       { "monitor", "arcade_31", NULL } },
   { "arcade_15_25_31", { "monitor", "arcade_15_25_31", NULL } },
   { "pc_31_120",       { "monitor", "pc_31_120", NULL } },
   { "pc_70_120",       { "monitor", "pc_70_120", NULL } },
   { "d9800",           { "monitor", "d9800", NULL } },
   { "m3129",           { "monitor", "m3129", NULL } },
   { "ntsc",            { "monitor", "ntsc", NULL } },
   { "pal",             { "monitor", "pal", NULL } },
   { "generic_15",      { "monitor", "generic_15", NULL } },
   { "vesa_768",        { "monitor", "vesa_768", NULL } },
   { "vesa_1024",       { "monitor", "vesa_1024", NULL } },
   { "lcd_59_61",       { "monitor", "lcd", "lcd_range", "59-61", NULL } },
   { "custom",          { "monitor", "custom",
      "crt_range0", "15625-16200, 49.50-65.00, 2.000, 4.700, 8.000, 0.064, 0.192, 1.024, 0, 0, 192, 288, 448, 576",
      "crt_range1", "24960-24960, 49.50-65.00, 0.800, 4.000, 3.200, 0.080, 0.200, 1.000, 0, 0, 384, 400, 768, 800", NULL } },
   { "super_2560",      { "monitor", "arcade_15", "super_width", "2560", "user_mode", "2560x0@0", NULL } },
   { "no_doublescan",   { "monitor", "arcade_31", "doublescan", "0", NULL } },
   { "no_interlace",    { "monitor", "arcade_15", "interlace", "0", NULL } },
   { "v_shift_correct", { "monitor", "arcade_15", "v_shift_correct", "1", NULL } },
   { "geometry",        { "monitor", "arcade_15", "h_size", "1.05", "h_shift", "5", "v_shift", "3", NULL } },
   { "geometry_neg",    { "monitor", "arcade_15", "h_size", "0.9", "h_shift", "-7", "v_shift", "-4", NULL } },
   { "dotclock_min_25", { "monitor", "arcade_15", "dotclock_min", "25", NULL } },
   { "pixel_prec_0",    { "monitor", "arcade_15", "pixel_precision", "0", NULL } },
   { "tolerance_0.5",   { "monitor", "arcade_15", "sync_refresh_tolerance", "0.5", NULL } },
   { "not_proportional",{ "monitor", "arcade_15", "scale_proportional", "0", NULL } },
   { "aspect_16_9",     { "monitor", "arcade_15", "aspect", "16:9", NULL } },
   { "force_even",      { "monitor", "arcade_15", "interlace_force_even", "1", NULL } },
   { "user_modeline",   { "monitor", "arcade_15", "modeline", "\"320x240\" 6.700 320 336 367 426 240 244 247 262 -hsync -vsync", NULL } },
   { "no_generation",   { "monitor", "arcade_15", "modeline_generation", "0", NULL } }
};

static void gold_print(const char *tag, const gold_src *s, const gold_mode *m)
{
   printf("%s %dx%d@%.4f f%d: ", tag, s->w, s->h, s->hz, s->flags);
   if (!m)
   {
      printf("none\n");
      return;
   }
   printf("%llu %d %d %d %d %d %d %d %d i%d d%d h%d v%d %.6f %.6f %dx%d@%d sc(%.6f,%.6f,%.6f) st%d off%d t%08x\n",
         (unsigned long long)m->pclock,
         m->hactive, m->hbegin, m->hend, m->htotal,
         m->vactive, m->vbegin, m->vend, m->vtotal,
         m->interlace, m->doublescan, m->hsync, m->vsync,
         m->vfreq, m->hfreq, m->width, m->height, m->refresh,
         m->result.x_scale, m->result.y_scale, m->result.v_scale,
         GM_STRETCH(m), GM_VOFF(m), (unsigned)m->type);
}

static void gold_run_case(const gold_case *c, int caps,
      const gold_mode *list, int nlist, const char *tag)
{
   size_t i;
   int o;
   gold_ctx *ctx = gold_new(caps);

   for (o = 0; c->opts[o]; o += 2)
      gold_option(ctx, c->opts[o], c->opts[o + 1]);
   gold_parse(ctx);
   if (list)
      gold_list(ctx, list, nlist);

   for (i = 0; i < sizeof(gold_sources) / sizeof(gold_sources[0]); i++)
   {
      char line[64];
      /* The consumer hands the engine a float core rate; model that so
       * both builds see the same input. */
      double hz = (double)(float)gold_sources[i].hz;
      const gold_mode *m = gold_get(ctx, gold_sources[i].w, gold_sources[i].h,
            hz, gold_sources[i].flags);
      snprintf(line, sizeof(line), "%s/%s", tag, c->name);
      gold_print(line, &gold_sources[i], m);
      /* The EDID block's timing, limits and name bytes for the
       * 320x240 source on the dummy display */
      if (m && i == 1 && caps < 0)
      {
         unsigned char edid[128];
         int b;
         memset(edid, 0, sizeof(edid));
         gold_edid(ctx, m, edid);
         printf("%s edid:", line);
         for (b = 54; b < 71; b++)
            printf(" %02x", edid[b]);
         /* up to the end of the name: the padding after it is the
          * one byte where the two generators differ by design */
         printf(" |");
         for (b = 90; b < 113 + (int)strlen(gold_monitor(ctx)) && b < 125; b++)
            printf(" %02x", edid[b]);
         printf("\n");
      }
   }
   gold_free(ctx);
}

static void gold_fill_listed(gold_mode *m, int w, int h, int r,
      int interlace, int type)
{
   memset(m, 0, sizeof(*m));
   m->width = m->hactive = w;
   m->height = m->vactive = h;
   m->refresh = r;
   m->vfreq = r;
   m->interlace = interlace;
   m->type = type;
   /* Listed modes carry a plausible desktop-style timing so an UPDATE
    * has something to compare against. */
   m->pclock = (uint64_t)w * h * r * 5 / 4;
   m->hbegin = w + 16;
   m->hend   = w + 64;
   m->htotal = w + 160;
   m->vbegin = h + 3;
   m->vend   = h + 6;
   m->vtotal = h + 30;
   m->hfreq  = (double)m->pclock / m->htotal;
}

int main(int argc, char *argv[])
{
   size_t c;
   gold_mode listed[6];

   /* Dummy display: generate freely (KMS / X11 add path) */
   for (c = 0; c < sizeof(gold_cases) / sizeof(gold_cases[0]); c++)
      gold_run_case(&gold_cases[c], -1, NULL, 0, "dummy");

   /* Update-only display with a listed mode set (the Windows ADL shape) */
   gold_fill_listed(&listed[0], 2560, 240, 60, 0, 0);
   gold_fill_listed(&listed[1], 2560, 480, 60, 1, 0);
   gold_fill_listed(&listed[2], 2560, 288, 50, 0, 0);
   gold_fill_listed(&listed[3], 2560, 576, 50, 1, 0);
   gold_fill_listed(&listed[4], 640, 480, 60, 0, 0);
   gold_fill_listed(&listed[5], 1920, 1080, 60, 0, GOLD_DESKTOP | GOLD_SYSTEM);
   for (c = 0; c < sizeof(gold_cases) / sizeof(gold_cases[0]); c++)
      gold_run_case(&gold_cases[c], GOLD_CAPS_UPDATE, listed, 6, "listed");

   /* Add-capable display with the same list (X11 with pre-existing modes) */
   for (c = 0; c < sizeof(gold_cases) / sizeof(gold_cases[0]); c++)
      gold_run_case(&gold_cases[c], GOLD_CAPS_ADD, listed, 6, "listed_add");

   return 0;
}
