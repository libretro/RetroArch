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


#ifndef RETROARCH_ALSA_H
#define RETROARCH_ALSA_H

#include <stdbool.h>
#include <stddef.h>
#include <alsa/asoundlib.h>

/**
 * @brief Common information for PCM devices.
 */
typedef struct alsa_stream_info {
    size_t buffer_size;
    size_t period_size;
    snd_pcm_uframes_t period_frames;
    unsigned int frame_bits;
    bool has_float;
    bool can_pause;
} alsa_stream_info_t;

/**
 * @brief Initialize a PCM device.
 * 
 * @param[out] pcm Pointer to the PCM handle.
 * @param[in] device Device name.
 * @param[in] stream Stream direction (playback or capture).
 * @param[in] rate Desired sample rate.
 * @param[in] latency Desired latency in milliseconds.
 * @param[in] channels Number of channels.
 * @param[out] stream_info Pointer to store stream information.
 * @param[out] new_rate Pointer to store the actual sample rate.
 * @param[in] mode ALSA open mode.
 * @return int 0 on success, negative error code on failure.
 */
int alsa_init_pcm(snd_pcm_t **pcm,
                  const char *device,
                  snd_pcm_stream_t stream,
                  unsigned int rate,
                  unsigned int latency,
                  unsigned int channels,
                  alsa_stream_info_t *stream_info,
                  unsigned int *new_rate,
                  int mode);

/**
 * @brief Free a PCM device.
 * 
 * @param pcm PCM handle to free.
 */
void alsa_free_pcm(snd_pcm_t *pcm);

/**
 * @brief Create a new ALSA device list.
 * 
 * @param data User data.
 * @return void* Pointer to the new device list.
 */
void *alsa_device_list_new(void *data);

/**
 * @brief Create a new ALSA device list of a specific type.
 * 
 * @param type Device type.
 * @return struct string_list* List of devices.
 */
struct string_list *alsa_device_list_type_new(const char *type);

/**
 * @brief Free an ALSA device list.
 * 
 * @param data User data.
 * @param array_list_data Array list data to free.
 */
void alsa_device_list_free(void *data, void *array_list_data);

/**
 * @brief Start a PCM device.
 * 
 * @param pcm PCM handle.
 * @return true if successful, false otherwise.
 */
bool alsa_start_pcm(snd_pcm_t *pcm);

/**
 * @brief Stop a PCM device.
 * 
 * @param pcm PCM handle.
 * @return true if successful, false otherwise.
 */
bool alsa_stop_pcm(snd_pcm_t *pcm);

#endif /* RETROARCH_ALSA_H */

bool alsa_start_pcm(snd_pcm_t *pcm);
bool alsa_stop_pcm(snd_pcm_t *pcm);

#endif /* _RETROARCH_ALSA */
