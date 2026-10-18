/*  RetroArch - A frontend for libretro.
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

#ifndef __UI_QT_FLOAT_SYNC_H
#define __UI_QT_FLOAT_SYNC_H

#include <string.h>

/* Paint-time resync of a float widget against the setting it edits.
 *
 * A float widget rounds what it is given to its own precision (the
 * setting's display decimals), so a stored value finer than that is
 * never what the widget reports back.  The widget is therefore
 * compared with the stored value it was last given, never with what
 * it shows: returns nonzero, and records 'stored' in *synced, only
 * when the setting changed since the last push.  Bit comparison, so
 * a NaN read from a config file settles too. */
static int ui_qt_float_resync(float *synced, float stored)
{
   if (!memcmp(synced, &stored, sizeof(stored)))
      return 0;
   memcpy(synced, &stored, sizeof(stored));
   return 1;
}

#endif
