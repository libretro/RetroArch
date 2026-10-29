/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - gavin_darkglider
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

#ifndef __RARCH_REBOOT2PAYLOAD_H
#define __RARCH_REBOOT2PAYLOAD_H

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Reboot-to-payload support for Lakka on the Nintendo Switch.
 *
 * The Switchroot L4T kernel exposes the pmc_r2p driver at
 * /sys/devices/r2p. Writing action/param1/param2 there selects what
 * Hekate boots on the next reboot:
 *
 *    action=bootloader             -> stop in the Hekate menu
 *    action=self, param2=0, param1=N -> Nth [entry] of hekate_ipl.ini
 *    action=self, param2=1, param1=N -> Nth [entry] across
 *                                       bootloader/ini/ (sorted)
 *
 * The list of destinations is parsed from the Hekate configuration on
 * the FAT boot partition (mounted at /flash by Lakka). The default
 * selection is the entry Lakka was booted from, taken from the
 * pmc_r2p.* parameters on the kernel command line. */

#define R2P_NAME_MAXLEN 64

struct settings;

/* True when the running kernel provides the pmc_r2p sysfs interface. */
bool        r2p_is_supported(void);

/* Re-parses the payload list, then restores the selection persisted in
 * settings (reboot_payload_kind / reboot_payload_name). The persisted
 * entry is matched by kind and name, so it survives INI reordering;
 * if it no longer exists the selection falls back to the entry Lakka
 * was booted from. */
void        r2p_load_selection(const struct settings *settings);

/* Steps the selection forward or backward (wrapping) and mirrors the
 * new selection into settings so it is saved with the config. */
void        r2p_cycle_selection(struct settings *settings, bool forward);

/* Display name of the selected destination. The entry Lakka was
 * booted from is suffixed with " (self)". */
const char *r2p_get_destination_name(void);

/* Arms pmc_r2p for the selected destination. */
bool        r2p_arm_selected(void);

/* Arms pmc_r2p with the pmc_r2p.* values from the kernel command
 * line, i.e. reboots back into the running Lakka entry regardless
 * of the selection. */
bool        r2p_arm_self(void);

void        r2p_deinit(void);

RETRO_END_DECLS

#endif
