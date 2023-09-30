/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2023 - Gavin_Darkglider
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

#ifndef __RARCH_LAKKA_SWITCH_H
#define __RARCH_LAKKA_SWITCH_H

#define SWITCH_OC_TOGGLE_PATH "/sys/kernel/tegra_cpufreq/overclock"
#define SWITCH_CEC_TOGGLE_PATH "/tmp/.CEC"
#define SWITCH_GPU_PROFILE_FILE_MAX_PATH  "/sys/devices/57000000.gpu/devfreq/57000000.gpu/max_freq"
#define SWITCH_GPU_PROFILE_FILE_MIN_PATH  "/sys/devices/57000000.gpu/devfreq/57000000.gpu/min_freq"
#define SWITCH_GPU_PROFILE_FILE_GOVERNOR_PATH "/sys/devices/57000000.gpu/devfreq/57000000.gpu/governor"
#define SWITCH_GPU_PROFILE_FILE_AVAILABLE_GOVERNORS_PATH "/sys/devices/57000000.gpu/devfreq/57000000.gpu/available_governors"
#define SWITCH_GPU_PROFILE_FILE_AVAILABLE_FREQ_PATH "/sys/devices/57000000.gpu/devfreq/57000000.gpu/available_governors"

#define BLUETOOTH_ERTM_TOGGLE_PATH "/sys/module/bluetooth/parameters/disable_ertm"

#define SWITCH_R2P_ENABLED_PATH "/sys/module/pmc_r2p/parameters/enabled" 
#define SWITCH_R2P_ACTION_PATH /sys/module/pmc_r2p/parameters/action 
#define SWITCH_R2P_ENTRY_ID_PATH /sys/module/pmc_r2p/parameters/entry_id 
#define SWITCH_R2P_PARAM1_PATH /sys/module/pmc_r2p/parameters/param1
#define SWITCH_R2P_PARAM2_PATH /sys/module/pmc_r2p/parameters/param2
#endif
