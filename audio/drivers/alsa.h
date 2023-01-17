/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2023 The RetroArch team
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


#ifndef _RETROARCH_ALSA
#define _RETROARCH_ALSA

/* Header file for common functions that are used by alsa and alsathread. */

bool alsa_find_float_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
void *alsa_device_list_new(void *data);
void alsa_device_list_free(void *data, void *array_list_data);

#endif /* _RETROARCH_ALSA */
