/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <linux/input.h>
#include <linux/kd.h>
#include <termios.h>

#include "linux_common.h"

static struct termios oldTerm, newTerm;
static long oldKbmd                = 0xffff;
static bool linux_stdin_claimed    = false;

void linux_terminal_flush(void)
{
   tcsetattr(0, TCSAFLUSH, &oldTerm);
}

void linux_terminal_restore_input(void)
{
   if (oldKbmd == 0xffff)
      return;

   ioctl(0, KDSKBMODE, oldKbmd);
   linux_terminal_flush();
   oldKbmd = 0xffff;

   linux_stdin_claimed = false;
}

/* Disables input */

bool linux_terminal_init(void)
{
   if (oldKbmd != 0xffff)
      return false;

   if (tcgetattr(0, &oldTerm) < 0)
      return false;

   newTerm              = oldTerm;
   newTerm.c_lflag     &= ~(ECHO | ICANON | ISIG);
   newTerm.c_iflag     &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
   newTerm.c_cc[VMIN]   = 0;
   newTerm.c_cc[VTIME]  = 0;

   /* Be careful about recovering the terminal. */
   if (ioctl(0, KDGKBMODE, &oldKbmd) < 0)
      return false;

   if (tcsetattr(0, TCSAFLUSH, &newTerm) < 0)
      return false;

   return true;
}

void linux_terminal_claim_stdin(void)
{
   /* We need to disable use of stdin command interface if 
    * stdin is supposed to be used for input. */
   linux_stdin_claimed = true; 
}

bool linux_terminal_grab_stdin(void *data)
{
   return linux_stdin_claimed;
}
