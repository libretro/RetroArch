/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>

#include <signal.h>

#include <linux/input.h>
#include <linux/kd.h>
#include <termios.h>
#include <unistd.h>

#include "linux_common.h"

/* TODO/FIXME - static globals */
static struct termios old_term, new_term;
static long old_kbmd               = 0xffff;
static bool linux_stdin_claimed    = false;

void linux_terminal_restore_input(void)
{
   if (old_kbmd == 0xffff)
      return;

   if (ioctl(0, KDSKBMODE, old_kbmd) < 0)
      return;

   tcsetattr(0, TCSAFLUSH, &old_term);
   old_kbmd = 0xffff;

   linux_stdin_claimed = false;
}

/* Disables input */
static bool linux_terminal_init(void)
{
   if (old_kbmd != 0xffff)
      return false;

   if (tcgetattr(0, &old_term) < 0)
      return false;

   new_term              = old_term;
   new_term.c_lflag     &= ~(ECHO | ICANON | ISIG);
   new_term.c_iflag     &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
   new_term.c_cc[VMIN]   = 0;
   new_term.c_cc[VTIME]  = 0;

   /* Be careful about recovering the terminal. */
   if (ioctl(0, KDGKBMODE, &old_kbmd) < 0)
      return false;

   if (tcsetattr(0, TCSAFLUSH, &new_term) < 0)
      return false;

   return true;
}

/* We need to disable use of stdin command interface if
 * stdin is supposed to be used for input. */
void linux_terminal_claim_stdin(void)
{
   linux_stdin_claimed = true;
}

bool linux_terminal_grab_stdin(void *data)
{
   return linux_stdin_claimed;
}

static void linux_terminal_restore_signal(int sig)
{
   linux_terminal_restore_input();
   kill(getpid(), sig);
}

bool linux_terminal_disable_input(void)
{
   struct sigaction sa = {{0}};

   /* Avoid accidentally typing stuff. */
   if (!isatty(0))
      return false;

   if (!linux_terminal_init())
      return false;

   if (ioctl(0, KDSKBMODE, K_MEDIUMRAW) < 0)
   {
      tcsetattr(0, TCSAFLUSH, &old_term);
      return false;
   }

   sa.sa_handler = linux_terminal_restore_signal;
   sa.sa_flags   = SA_RESTART | SA_RESETHAND;
   sigemptyset(&sa.sa_mask);

   /* Trap some standard termination codes so we
    * can restore the keyboard before we lose control. */
   sigaction(SIGABRT, &sa, NULL);
   sigaction(SIGBUS,  &sa, NULL);
   sigaction(SIGFPE,  &sa, NULL);
   sigaction(SIGILL,  &sa, NULL);
   sigaction(SIGQUIT, &sa, NULL);
   sigaction(SIGSEGV, &sa, NULL);

   atexit(linux_terminal_restore_input);

   return true;
}
