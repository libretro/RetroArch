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

#ifndef __RARCH_GAMEMODE_LINUX_H
#define __RARCH_GAMEMODE_LINUX_H

#include <boolean.h>
#include <retro_common_api.h>

/* Game Mode on desktop Linux, done in-process instead of through Feral's
 * libgamemode and the gamemoded daemon. Of what gamemoded does with its
 * stock configuration, the parts that matter to RetroArch are:
 *
 *  - I/O priority best-effort level 0 on every thread of the process,
 *    which an unprivileged process may set on itself;
 *  - the CPU in its performance mode (governor or energy preference,
 *    and the platform profile). Here that is a "performance" hold
 *    through power-profiles-daemon (or tuned-ppd) on the system bus,
 *    which desktop polkit policy grants an active session without a
 *    password. The daemon drops the hold by itself when our connection
 *    goes away, so a crash never leaves the machine in performance mode.
 *    As with the Windows Low-Latency Power Plan, the hold only applies
 *    on AC power: it follows UPower's OnBattery (or, without UPower,
 *    the power supplies in sysfs, re-read when the kernel announces a
 *    power supply change), released when the charger is pulled and
 *    held again when it is plugged back in.
 *
 * Screensaver inhibit, the other default, RetroArch already does through
 * its own Suspend Screensaver setting. */
#if defined(__linux__) && !defined(ANDROID) && !defined(__ANDROID__)
#define RARCH_HAVE_GAMEMODE_LINUX 1
#endif

#ifdef RARCH_HAVE_GAMEMODE_LINUX

RETRO_BEGIN_DECLS

/**
 * Enters or leaves Game Mode. Returns at once: the D-Bus exchange runs
 * on a helper thread that lives while Game Mode is on, and its outcome
 * is only logged. False when nothing could be applied at all.
 *
 * @exiting: the process is about to end. Only the I/O priority is put
 * back; the profile hold goes with the bus connection when the process
 * exits, so no helper is started during shutdown.
 */
bool gamemode_linux_set(bool on, bool exiting);

RETRO_END_DECLS

#endif

#endif
