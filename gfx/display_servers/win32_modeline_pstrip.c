/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include <compat/strl.h>
#include <retro_timers.h>

#include "win32_modeline.h"
#include "../../verbosity.h"

/* PowerStrip (EnTech) is a third-party tray utility that programs
 * custom timings on a range of GPUs. RetroArch neither ships nor
 * launches it: this path finds a running instance by its hidden
 * window and talks to it with the developer WM_USER messages, using
 * its decimal timing string
 *   W,HFP,HSW,HBP,H,VFP,VSW,VBP,pclock_kHz,flags
 * (porch widths, not modeline endpoints). A custom_timing option
 * bypasses generation and is sent verbatim. */

#define UM_SETCUSTOMTIMING      (WM_USER + 200)
#define UM_SETREFRESHRATE       (WM_USER + 201)
#define UM_SETPOLARITY          (WM_USER + 202)
#define UM_REMOTECONTROL        (WM_USER + 210)
#define UM_SETGAMMARAMP         (WM_USER + 203)
#define UM_CREATERESOLUTION     (WM_USER + 204)
#define UM_GETTIMING            (WM_USER + 205)
#define UM_GETSETCLOCKS         (WM_USER + 206)

#define PS_NEGATIVE_H_POLARITY  0x02
#define PS_NEGATIVE_V_POLARITY  0x04
#define PS_INTERLACE            0x08

typedef struct
{
   int HorizontalActivePixels;
   int HorizontalFrontPorch;
   int HorizontalSyncWidth;
   int HorizontalBackPorch;
   int VerticalActivePixels;
   int VerticalFrontPorch;
   int VerticalSyncWidth;
   int VerticalBackPorch;
   int PixelClockInKiloHertz;
   int TimingFlags;
} ps_timing_t;

typedef struct
{
   HWND hwnd;
   video_modeline_t user_mode;
   ps_timing_t timing_backup;
   int monitor_index;
   char m_device_name[32];
   char m_ps_timing[256];
} pstrip_ctx_t;

static bool ps_read_timing_string(const char *in, ps_timing_t *t)
{
   return sscanf(in, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
         &t->HorizontalActivePixels, &t->HorizontalFrontPorch,
         &t->HorizontalSyncWidth, &t->HorizontalBackPorch,
         &t->VerticalActivePixels, &t->VerticalFrontPorch,
         &t->VerticalSyncWidth, &t->VerticalBackPorch,
         &t->PixelClockInKiloHertz, &t->TimingFlags) == 10;
}

static void ps_fill_timing_string(char *out, size_t len, const ps_timing_t *t)
{
   snprintf(out, len, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
         t->HorizontalActivePixels, t->HorizontalFrontPorch,
         t->HorizontalSyncWidth, t->HorizontalBackPorch,
         t->VerticalActivePixels, t->VerticalFrontPorch,
         t->VerticalSyncWidth, t->VerticalBackPorch,
         t->PixelClockInKiloHertz, t->TimingFlags);
}

static bool ps_modeline_to_pstiming(const video_modeline_t *m, ps_timing_t *t)
{
   if (m->pclock == 0 || m->hactive == 0 || m->vactive == 0)
   {
      RARCH_DBG("[PStrip] Invalid modeline\n");
      return false;
   }
   t->HorizontalActivePixels = m->hactive;
   t->HorizontalFrontPorch   = m->hbegin - m->hactive;
   t->HorizontalSyncWidth    = m->hend - m->hbegin;
   t->HorizontalBackPorch    = m->htotal - m->hend;
   t->VerticalActivePixels   = m->vactive;
   t->VerticalFrontPorch     = m->vbegin - m->vactive;
   t->VerticalSyncWidth      = m->vend - m->vbegin;
   t->VerticalBackPorch      = m->vtotal - m->vend;
   t->PixelClockInKiloHertz  = (int)(m->pclock / 1000);
   if (m->hsync == 0)
      t->TimingFlags |= PS_NEGATIVE_H_POLARITY;
   if (m->vsync == 0)
      t->TimingFlags |= PS_NEGATIVE_V_POLARITY;
   if (m->interlace)
      t->TimingFlags |= PS_INTERLACE;
   return true;
}

static void ps_pstiming_to_modeline(const ps_timing_t *t, video_modeline_t *m)
{
   m->hactive = t->HorizontalActivePixels;
   m->hbegin  = m->hactive + t->HorizontalFrontPorch;
   m->hend    = m->hbegin + t->HorizontalSyncWidth;
   m->htotal  = m->hend + t->HorizontalBackPorch;
   m->vactive = t->VerticalActivePixels;
   m->vbegin  = m->vactive + t->VerticalFrontPorch;
   m->vend    = m->vbegin + t->VerticalSyncWidth;
   m->vtotal  = m->vend + t->VerticalBackPorch;
   m->width   = m->hactive;
   m->height  = m->vactive;
   m->pclock  = (uint64_t)t->PixelClockInKiloHertz * 1000;
   if (!(t->TimingFlags & PS_NEGATIVE_H_POLARITY))
      m->hsync = 1;
   if (!(t->TimingFlags & PS_NEGATIVE_V_POLARITY))
      m->vsync = 1;
   if (t->TimingFlags & PS_INTERLACE)
      m->interlace = 1;
   /* Whole hertz for the line rate, as PowerStrip reports it */
   m->hfreq   = (double)(m->pclock / (uint64_t)m->htotal);
   m->vfreq   = m->hfreq / m->vtotal * (m->interlace ? 2 : 1);
   m->refresh = (int)m->vfreq;
}

static bool ps_get_monitor_timing(pstrip_ctx_t *c, ps_timing_t *t)
{
   LRESULT lresult;
   char in[256];

   if (!c->hwnd)
      return false;
   lresult = SendMessage(c->hwnd, UM_GETTIMING, c->monitor_index, 0);
   if (lresult == -1)
   {
      RARCH_DBG("[PStrip] Could not get PowerStrip timing string\n");
      return false;
   }
   if (!GlobalGetAtomNameA((ATOM)lresult, in, sizeof(in)))
   {
      RARCH_DBG("[PStrip] GlobalGetAtomName failed\n");
      return false;
   }
   RARCH_DBG("[PStrip] Get monitor timing(%d): %s\n", c->monitor_index, in);
   ps_read_timing_string(in, t);
   /* The atom was created by PowerStrip for this reply */
   GlobalDeleteAtom((ATOM)lresult);
   return true;
}

static bool ps_set_monitor_timing(pstrip_ctx_t *c, const ps_timing_t *t)
{
   LRESULT lresult;
   ATOM atom;
   char out[256];

   if (!c->hwnd)
      return false;
   ps_fill_timing_string(out, sizeof(out), t);
   atom = GlobalAddAtomA(out);
   if (!atom)
   {
      RARCH_DBG("[PStrip] Atom creation failed\n");
      return false;
   }
   lresult = SendMessage(c->hwnd, UM_SETCUSTOMTIMING, c->monitor_index, atom);
   if (lresult < 0)
   {
      RARCH_DBG("[PStrip] SendMessage failed\n");
      GlobalDeleteAtom(atom);
      return false;
   }
   RARCH_DBG("[PStrip] Set monitor timing(%d): %s\n", c->monitor_index, out);
   return true;
}

static bool ps_set_monitor_timing_string(pstrip_ctx_t *c, const char *in)
{
   ps_timing_t t;
   memset(&t, 0, sizeof(t));
   ps_read_timing_string(in, &t);
   return ps_set_monitor_timing(c, &t);
}

/* Probe around the wanted dotclock for the value the card locks to */
static int ps_best_pclock(pstrip_ctx_t *c, ps_timing_t *t, int desired_pclock)
{
   int i;
   int best_pclock = 0;
   ps_timing_t timing_read;

   RARCH_DBG("[PStrip] Best pclock(%d): probing stable dotclocks for %d\n",
         c->monitor_index, desired_pclock);
   for (i = -50; i <= 50; i += 25)
   {
      memset(&timing_read, 0, sizeof(timing_read));
      t->PixelClockInKiloHertz = desired_pclock + i;
      ps_set_monitor_timing(c, t);
      ps_get_monitor_timing(c, &timing_read);
      if (abs(timing_read.PixelClockInKiloHertz - desired_pclock)
            < abs(desired_pclock - best_pclock))
         best_pclock = timing_read.PixelClockInKiloHertz;
   }
   RARCH_DBG("[PStrip] Best pclock(%d): new dotclock %d\n",
         c->monitor_index, best_pclock);
   return best_pclock;
}

static bool ps_set_modeline(pstrip_ctx_t *c, const video_modeline_t *m)
{
   ps_timing_t t;
   memset(&t, 0, sizeof(t));
   if (!ps_modeline_to_pstiming(m, &t))
      return false;
   t.PixelClockInKiloHertz = ps_best_pclock(c, &t, t.PixelClockInKiloHertz);
   return ps_set_monitor_timing(c, &t);
}

static int ps_monitor_index(const char *display_name)
{
   int monitor_index = 0;
   char sub_index[2];
   size_t len = strlen(display_name);
   sub_index[0] = len ? display_name[len - 1] : '\0';
   sub_index[1] = '\0';
   if (sscanf(sub_index, "%d", &monitor_index) == 1)
      monitor_index--;
   return monitor_index;
}

static unsigned pstrip_caps(void *ctx)
{
   return MODELINE_CAPS_UPDATE | MODELINE_CAPS_SCAN_EDITABLE
      | MODELINE_CAPS_DESKTOP_EDITABLE;
}

static bool pstrip_get_timing(void *ctx, video_modeline_t *mode)
{
   pstrip_ctx_t *c = (pstrip_ctx_t*)ctx;
   ps_timing_t t;
   video_modeline_t m_temp;

   /* A custom_timing string locks every mode that does not match it */
   if (c->user_mode.hactive)
   {
      if (mode->width != c->user_mode.width || mode->height != c->user_mode.height)
      {
         mode->type |= MODELINE_DISABLED;
         return false;
      }
   }

   memset(&t, 0, sizeof(t));
   memset(&m_temp, 0, sizeof(m_temp));
   if (!ps_get_monitor_timing(c, &t))
      return false;
   ps_pstiming_to_modeline(&t, &m_temp);

   /* Only the current desktop mode's timing is readable */
   if (m_temp.width == mode->width && m_temp.height == mode->height
         && m_temp.refresh == mode->refresh)
      *mode = m_temp;
   mode->type |= MODELINE_TIMING_POWERSTRIP;
   return true;
}

static bool pstrip_update_mode(void *ctx, video_modeline_t *mode)
{
   pstrip_ctx_t *c = (pstrip_ctx_t*)ctx;
   bool ok;
   if (c->user_mode.hactive)
      ok = ps_set_monitor_timing_string(c, c->m_ps_timing);
   else
      ok = ps_set_modeline(c, mode);
   /* PowerStrip applies asynchronously; give it its settle time */
   retro_sleep(100);
   if (!ok)
      return false;
   mode->type |= MODELINE_TIMING_POWERSTRIP;
   return true;
}

static bool pstrip_flush(void *ctx)
{
   return true;
}

static void pstrip_close(void *ctx)
{
   pstrip_ctx_t *c = (pstrip_ctx_t*)ctx;
   if (!c)
      return;
   /* The timing the desktop had when the path opened goes back */
   ps_set_monitor_timing(c, &c->timing_backup);
   free(c);
}

bool win32_modeline_pstrip_create(win32_modeline_backend_t *b,
      const char *device_name, const video_modeline_disp_t *ds)
{
   pstrip_ctx_t *c = (pstrip_ctx_t*)calloc(1, sizeof(*c));
   if (!c)
      return false;

   strlcpy(c->m_device_name, device_name, sizeof(c->m_device_name));
   strlcpy(c->m_ps_timing, ds->custom_timing, sizeof(c->m_ps_timing));
   c->monitor_index = ps_monitor_index(c->m_device_name);

   c->hwnd = FindWindowA("TPShidden", NULL);
   if (!c->hwnd)
   {
      RARCH_DBG("[PStrip] Could not get PowerStrip API interface\n");
      free(c);
      return false;
   }
   RARCH_DBG("[PStrip] PowerStrip found\n");

   ps_get_monitor_timing(c, &c->timing_backup);

   if (strcmp(c->m_ps_timing, "auto"))
   {
      ps_timing_t t;
      memset(&t, 0, sizeof(t));
      if (ps_read_timing_string(c->m_ps_timing, &t))
      {
         char txt[256];
         ps_pstiming_to_modeline(&t, &c->user_mode);
         c->user_mode.type |= MODELINE_TIMING_POWERSTRIP;
         RARCH_DBG("[PStrip] custom_timing: %s (%s)\n", c->m_ps_timing,
               modeline_print(&c->user_mode, txt, sizeof(txt), MODELINE_PRINT_PARAMS));
      }
      else
         RARCH_DBG("[PStrip] custom_timing string with invalid format\n");
   }

   b->ctx         = c;
   b->name        = "PowerStrip";
   b->caps        = pstrip_caps;
   b->get_timing  = pstrip_get_timing;
   b->add_mode    = NULL;
   b->update_mode = pstrip_update_mode;
   b->delete_mode = pstrip_update_mode; /* restores the backed-up timing */
   b->flush       = pstrip_flush;
   b->close       = pstrip_close;
   return true;
}
