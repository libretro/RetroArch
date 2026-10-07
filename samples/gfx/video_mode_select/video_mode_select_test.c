/* gfx/common/video_mode_select.c: the mode a size and rate name, and
 * the order a resolution list is shown in. Every display server -
 * Mutter, KWin, wlroots - switches modes through this, so the rules it
 * applies are checked here rather than three times over. */

#include <stdio.h>
#include <string.h>

#include "gfx/common/video_mode_select.h"

/* A display's modes, as any backend would hold them */
typedef struct { int w, h; float hz; bool interlaced, current; } mode_t_;

static mode_t_ modes[] = {
   { 1920, 1080,  60.000f, false, false },
   { 1920, 1080,  59.940f, false, true  },  /* the one running */
   { 1920, 1080, 120.000f, false, false },
   { 1920, 1080,  60.000f, true,  false },  /* interlaced twin */
   { 1280,  720,  60.000f, false, false },
   { 2560, 1440, 143.998f, false, false },
   {    0,    0,   0.000f, false, false },  /* a mode with nothing in it */
};
#define NMODES ((unsigned)(sizeof(modes) / sizeof(modes[0])))

static bool get(void *ctx, unsigned i, video_mode_desc_t *out)
{
   const mode_t_ *m = &((const mode_t_*)ctx)[i];
   if (m->w <= 0 || m->h <= 0 || m->hz <= 0.0f)
      return false;
   out->dims       = VIDEO_SCALE_PACK(m->w, m->h);
   out->refresh    = m->hz;
   out->interlaced = m->interlaced;
   out->current    = m->current;
   return true;
}

static int fails;
static void check(const char *what, int ok)
{
   printf("   %s %s\n", ok ? "ok  " : "FAIL", what);
   if (!ok)
      fails++;
}
static int pick(int w, int h, int int_hz, float hz)
{
   return video_mode_find_nearest(modes, get, NMODES,
         VIDEO_SCALE_PACK(w, h), int_hz, hz);
}

int main(void)
{
   printf("1. the mode a size and rate name\n");
   check("an exact rate", pick(1920, 1080, 120, 120.0f) == 2);
   check("59.94 asked for as 59.94", pick(1920, 1080, 60, 59.94f) == 1);
   check("60 asked for takes 60.000, not the running 59.94",
         pick(1920, 1080, 60, 60.0f) == 0);
   check("a whole-hertz label matches a rate further than half a hertz off",
         pick(2560, 1440, 144, 144.0f) == 5);
   check("another size", pick(1280, 720, 60, 60.0f) == 4);

   printf("2. what is not a mode the caller asked for\n");
   check("a rate no mode is near", pick(1920, 1080, 0, 75.0f) == -1);
   check("a size no mode has", pick(1024, 768, 60, 60.0f) == -1);
   check("a mode with nothing in it is skipped", pick(0, 0, 60, 60.0f) == -1);
   check("no modes at all",
         video_mode_find_nearest(modes, get, 0, VIDEO_SCALE_PACK(1920, 1080),
            60, 60.0f) == -1);
   check("no getter", video_mode_find_nearest(modes, NULL, NMODES,
            VIDEO_SCALE_PACK(1920, 1080), 60, 60.0f) == -1);

   printf("3. progressive before interlaced, the running mode on a tie\n");
   check("60.000 progressive beats its interlaced twin",
         pick(1920, 1080, 60, 60.0f) == 0);
   {
      /* the progressive 60 gone, a progressive 59.94 is still nearer
       * than an interlaced 60 */
      mode_t_ saved = modes[0];
      modes[0].hz   = 0.0f;
      check("a progressive neighbour beats an interlaced exact match",
            pick(1920, 1080, 60, 60.0f) == 1);
      modes[0] = saved;
   }
   {
      /* nothing progressive at that size: the interlaced one it is */
      mode_t_ s0 = modes[0], s1 = modes[1], s2 = modes[2];
      modes[0].hz = modes[1].hz = modes[2].hz = 0.0f;
      check("an interlaced mode is used when it is the only one",
            pick(1920, 1080, 60, 60.0f) == 3);
      modes[0] = s0; modes[1] = s1; modes[2] = s2;
   }
   {
      /* two identical rates, one of them running */
      mode_t_ saved = modes[2];
      modes[2].hz   = 59.940f;
      check("the running mode wins a tie", pick(1920, 1080, 60, 59.94f) == 1);
      modes[2] = saved;
   }

   printf("4. no rate asked for means the rate in use\n");
   check("size only takes the running 59.94", pick(1920, 1080, 0, 0.0f) == 1);

   printf("5. the order the menu shows a list in\n");
   {
      video_display_config_t list[4];
      unsigned i;
      memset(list, 0, sizeof(list));
      list[0].dims = VIDEO_SCALE_PACK(1920, 1080);
      list[0].refreshrate_float = 120.0f;
      list[1].dims = VIDEO_SCALE_PACK(1280, 720);
      list[1].refreshrate_float = 60.0f;
      list[2].dims = VIDEO_SCALE_PACK(1920, 1080);
      list[2].refreshrate_float = 60.0f;
      list[2].interlaced        = true;
      list[3].dims = VIDEO_SCALE_PACK(1920, 1080);
      list[3].refreshrate_float = 60.0f;
      video_mode_list_finish(list, 4);
      check("by size, then progressive, then rate",
            VIDEO_SCALE_W(list[0].dims) == 1280
            && VIDEO_SCALE_W(list[1].dims) == 1920 && list[1].refreshrate_float == 60.0f && !list[1].interlaced
            && VIDEO_SCALE_W(list[2].dims) == 1920 && list[2].refreshrate_float == 120.0f
            && list[3].interlaced);
      for (i = 0; i < 4; i++)
         if (list[i].idx != i)
            break;
      check("numbered in that order", i == 4);
      video_mode_list_finish(NULL, 4);
      video_mode_list_finish(list, 0);
      check("nothing to sort is harmless", 1);
   }

   if (fails)
   {
      printf("video_mode_select: %d check(s) failed\n", fails);
      return 1;
   }
   printf("video_mode_select: all checks passed\n");
   return 0;
}
